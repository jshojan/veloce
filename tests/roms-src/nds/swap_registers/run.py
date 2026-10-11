#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent byte-array/register oracle for original dual-CPU exchanges."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build,groups,cases,PATTERNS
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
def require(ok,why):
    if not ok:raise ValueError(why)
def predicate(cond,flags):
    n,z,c,v=(bool(flags&(1<<bit)) for bit in (4,3,2,1))
    return (z,not z,c,not c,n,not n,v,not v,c and not z,not c or z,n==v,n!=v,not z and n==v,z or n!=v,True)[cond]
def expected(a,b,flags,width,lane,alias,cond,cpu):
    memory=list(b.to_bytes(4,'little'));result=a if alias else 0xdeadbeef
    if predicate(cond,flags if cpu==9 else flags<<1):
        result=b if width==4 else memory[lane]
        if width==4:memory=list(a.to_bytes(4,'little'))
        else:memory[lane]=a&255
    return result,0xdf|(flags<<(27 if cpu==9 else 28)),result if alias else a,0x02001040+lane,int.from_bytes(bytes(memory),'little'),0xfacefeed

def references():
    require(sum(cases(g,cpu) for g in groups() for cpu in (9,7))==33300480,'full domain changed')
    require(expected(0x01020304,0xaabbccdd,31,4,0,False,14,9)==(0xaabbccdd,0xf80000df,0x01020304,0x02001040,0x01020304,0xfacefeed),'word oracle')
    require(expected(0x01020304,0xaabbccdd,0,1,1,True,14,7)==(0xcc,0xdf,0xcc,0x02001041,0xaabb04dd,0xfacefeed),'byte alias oracle')
    require(expected(1,2,0,4,0,False,0,9)==(0xdeadbeef,0xdf,1,0x02001040,2,0xfacefeed),'failed condition')

def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g,cpu) for g in domain for cpu in (9,7))
    report=dict(status='error',console='nds',suite='swap_registers',expected_cases=total,observed_cases=0,
        graded_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            for cpu in (9,7):
                require(f.readline()==f'VELOCE 1 ds swap_registers cpu={cpu}\n','wrong CPU/suite order')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n','wrong domain')
                for g in domain:
                    name,width,lane,alias,cond,values=g
                    require(f.readline()==f'BEGIN cpu{cpu}_{name} cases={cases((name,width,lane,alias,cond,values),cpu)} words=6\n','missing/reordered group')
                    group=dict(id=f'cpu{cpu}_{name}',observed_cases=0,mismatches=0); report['groups'].append(group)
                    for a in values:
                        for b in values:
                            for flags in range(32 if cpu==9 else 16):
                                line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{48}\n',line),'missing/malformed observation')
                                got=tuple(int(line[n:n+8],16) for n in (6,14,22,30,38,46)); want=expected(a,b,flags,width,lane,alias,cond,cpu)
                                report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                                if got!=want:
                                    report['mismatches']+=1; group['mismatches']+=1
                                    if len(report['examples'])<8: report['examples'].append(dict(cpu=cpu,group=name,left=f'{a:08x}',right=f'{b:08x}',flags=flags,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
                require(f.readline()==f'LOG cpu={cpu} complete cases={sum(cases(g,cpu) for g in domain)}\n','missing CPU completion')
            require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'missing/ungraded trailer')
            require(not f.read(),'trailing data'); report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e: report['detail']=str(e)
    return report


def run(emulator,image,result,frames=100000):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):env.pop(key,None)
    env.update(HEADLESS='1',FRAMES=str(frames),VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    p=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=1500)
    (result.with_suffix('.log')).write_text(p.stdout+p.stderr)
    require(p.returncode==0,p.stdout+p.stderr)
def write_report(path,report):path.write_text(json.dumps(report,indent=2)+'\n')
def controls(emulator,root):
    rom=build(False);image=root/'control.nds';image.write_bytes(rom);result=root/'control.result';run(emulator,image,result)
    report=evaluate(result,rom,False);write_report(root/'control.json',report);require(report['status']=='pass',str(report))
    mutant=build(False,True);image=root/'mutant.nds';image.write_bytes(mutant);result=root/'mutant.result';run(emulator,image,result)
    report=evaluate(result,mutant,False);write_report(root/'mutant.json',report);require(report['status']=='fail' and report['mismatches']>0,'executed missing-store mutant accepted')
    content=(root/'control.result').read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('result-bit',content.replace(row,row[:6]+f'{int(row[6:14],16)^1:08x}'+row[14:],1)),
        ('memory-bit',content.replace(row,row[:38]+f'{int(row[38:46],16)^1:08x}'+row[46:],1)),
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('false-completion',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated-trailer',content.rstrip('\n')),
        ('trailing-data',content+'extra\n')):
        path=root/(name+'.result');path.write_text(broken);report=evaluate(path,rom,False);write_report(root/(name+'.json'),report)
        require(report['status']==('fail' if name in ('result-bit','memory-bit') else 'error'),'accepted '+name)
        if name in ('result-bit','memory-bit'):require(report['mismatches']==1,'mutation count '+name)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_swap');p.add_argument('--baseline-only',action='store_true');p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True);references()
    if a.baseline_only:
        rom=build(False);image=a.artifacts/'baseline.nds';image.write_bytes(rom);result=a.artifacts/'baseline.result';run(a.emulator,image,result,1);report=evaluate(result,rom,False);write_report(a.artifacts/'baseline.json',report);require(report['status']!='pass','missing instruction unexpectedly passed');print('Before implementation:',report['status'],report.get('detail'));return 0
    controls(a.emulator,a.artifacts)
    if a.controls_only:print('Dual-CPU ARM swaps controls PASS; executed mutant FAIL; damaged files ERROR');return 0
    rom=build();require((HERE/'swap_registers.nds').read_bytes()==rom,'archived image differs');result=a.artifacts/'swap_registers.result';run(a.emulator,HERE/'swap_registers.nds',result);report=evaluate(result,rom);write_report(a.artifacts/'swap_registers.json',report);print(f"Dual-CPU ARM swaps: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']}; mismatches={report['mismatches']}")
    if report.get('detail'):print(report['detail'])
    return 0 if report['status']=='pass' else 1
if __name__=='__main__':sys.exit(main())
