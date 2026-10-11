#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent signed-integer/CPSR oracle for the original ARM9 Q corpus."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build,groups,cases,PATTERNS
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];MASK=0xffffffff

def require(ok,why):
    if not ok:raise ValueError(why)
def signed(n):return n if n<2**31 else n-2**32
def predicate(cond,flags):
    n,z,c,v=(bool(flags&(1<<bit)) for bit in (4,3,2,1))
    return (z,not z,c,not c,n,not n,v,not v,c and not z,not c or z,n==v,n!=v,not z and n==v,z or n!=v,True)[cond]
def expected(op,a,b,flags,cond,alias):
    psw=0xdf|(flags<<27);rd=0 if alias in ('destination_left','all_same') else 1 if alias=='destination_right' else 4
    original=(a,b);value=(a,b,0,0,0xdeadbeef)[rd]
    if predicate(cond,flags):
        rhs=signed(a if alias in ('same_sources','all_same') else b)
        clipped=False
        if op.startswith('qd'):
            doubled=2*rhs;rhs=max(-2**31,min(2**31-1,doubled));clipped=rhs!=doubled
        math=signed(a)+rhs if op.endswith('add') else signed(a)-rhs
        result=max(-2**31,min(2**31-1,math));clipped|=result!=math
        value=result&MASK
        if clipped:psw|=1<<27
    left=value if rd==0 else original[0];right=value if rd==1 else original[1]
    return value,psw,left,right

def references():
    require(sum(cases(g) for g in groups())==10846208,'full domain changed')
    for op,a,b,flags,want in (
        ('qadd',0x7fffffff,1,0,(0x7fffffff,0x080000df)),
        ('qsub',0x80000000,1,0,(0x80000000,0x080000df)),
        ('qdadd',0x80000000,0x40000000,0,(0xffffffff,0x080000df)),
        ('qdsub',0x80000000,0x40000000,0,(0x80000000,0x080000df)),
        ('qdsub',0x7fffffff,0xc0000000,0,(0x7fffffff,0x080000df)),
        ('qadd',1,2,31,(3,0xf80000df))):
        require(expected(op,a,b,flags,14,'distinct')[:2]==want,'oracle reference '+op)
    require(expected('qadd',1,2,0,0,'distinct')==(0xdeadbeef,0xdf,1,2),'failed condition')

def evaluate(path,rom,full=True):
    domain=groups(full);total=sum(cases(g) for g in domain)
    report=dict(status='error',console='nds',suite='saturating_registers',expected_cases=total,observed_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','file identity')
            require(f.readline()=='VELOCE 1 ds saturating_registers cpu=9\n','suite identity')
            require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags=32\n','domain identity')
            for g in domain:
                name,op,values,cond,alias=g
                require(f.readline()==f'BEGIN {name} cases={cases(g)} words=4\n','missing/reordered group '+name)
                group=dict(id=name,observed_cases=0,mismatches=0);report['groups'].append(group)
                for a in values:
                    for b in values:
                        for flags in range(32):
                            line=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{32}\n',line),'missing/malformed observation')
                            got=tuple(int(line[i:i+8],16) for i in (6,14,22,30));want=expected(op,a,b,flags,cond,alias)
                            report['observed_cases']+=1;group['observed_cases']+=1
                            if got!=want:
                                report['mismatches']+=1;group['mismatches']+=1
                                if len(report['examples'])<8:report['examples'].append(dict(group=name,a=f'{a:08x}',b=f'{b:08x}',flags=flags,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
            require(f.readline()==f'LOG complete cases={total}\n','false count')
            require(f.readline()=='END observations_complete\n','false completion')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'missing/ungraded trailer')
            require(not f.read(),'extra data');report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
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
    report=evaluate(result,mutant,False);write_report(root/'mutant.json',report)
    require(report['status']=='fail' and report['mismatches']>0,'executed wrong-double mutant accepted')
    require(all(g['mismatches']==0 for g in report['groups'] if not g['id'].startswith('qdsub_')),'unrelated mutant groups failed')
    content=(root/'control.result').read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('result-bit',content.replace(row,row[:6]+f'{int(row[6:14],16)^1:08x}'+row[14:],1)),
        ('q-bit',content.replace(row,row[:14]+f'{int(row[14:22],16)^0x08000000:08x}'+row[22:],1)),
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('false-completion',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated-trailer',content.rstrip('\n')),
        ('trailing-data',content+'extra\n')):
        path=root/(name+'.result');path.write_text(broken);report=evaluate(path,rom,False);write_report(root/(name+'.json'),report)
        require(report['status']==('fail' if name in ('result-bit','q-bit') else 'error'),'accepted '+name)
        if name in ('result-bit','q-bit'):require(report['mismatches']==1,'mutation count '+name)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_saturating');p.add_argument('--baseline-only',action='store_true');p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True);references()
    if a.baseline_only:
        rom=build(False);image=a.artifacts/'baseline.nds';image.write_bytes(rom);result=a.artifacts/'baseline.result';run(a.emulator,image,result,1);report=evaluate(result,rom,False);write_report(a.artifacts/'baseline.json',report);require(report['status']!='pass','missing instruction unexpectedly passed');print('Before implementation:',report['status'],report.get('detail'));return 0
    controls(a.emulator,a.artifacts)
    if a.controls_only:print('ARM9 saturation controls PASS; executed mutant FAIL; damaged files ERROR');return 0
    rom=build();require((HERE/'saturating_registers.nds').read_bytes()==rom,'archived image differs');result=a.artifacts/'saturating_registers.result';run(a.emulator,HERE/'saturating_registers.nds',result);report=evaluate(result,rom);write_report(a.artifacts/'saturating_registers.json',report);print(f"ARM9 saturation: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']}; mismatches={report['mismatches']}")
    if report.get('detail'):print(report['detail'])
    return 0 if report['status']=='pass' else 1
if __name__=='__main__':sys.exit(main())
