#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent address/register/RAM oracle for original ARM9 LDRD/STRD."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build,groups,cases,values,flags,name,TARGET
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];MASK=0xffffffff

def require(ok,why):
    if not ok:raise ValueError(why)
def predicate(cond,flags):
    n,z,c,v=(bool(flags&(1<<bit)) for bit in (4,3,2,1))
    return (z,not z,c,not c,n,not n,v,not v,c and not z,not c or z,n==v,n!=v,not z and n==v,z or n!=v,True)[cond]
def expected(g,a,b,flag):
    start=TARGET if g.index=='post' else (TARGET-g.offset if g.up else TARGET+g.offset)&MASK
    regs=[a^MASK,b^0xa5a5a5a5,start,g.offset];memory=[0xfacefeed,a,b,0xcafebabe]
    if g.alias=='base_high':regs[1]=start
    if g.alias=='index_low' or (g.alias=='base_low' and g.op=='ldrd' and g.form=='reg'):regs[0]=g.offset
    if g.alias=='index_high':regs[1]=g.offset
    if predicate(g.cond,flag):
        rd=2 if g.alias=='base_low' else 0;rn=1 if g.alias=='base_high' else 2
        if g.op=='ldrd':regs[rd:rd+2]=memory[1:3]
        else:memory[1:3]=regs[rd:rd+2]
        if g.index!='offset':regs[rn]=TARGET if g.index=='pre' else (TARGET+g.offset if g.up else TARGET-g.offset)&MASK
    return tuple(regs)+(0xdf|(flag<<27),)+tuple(memory)
def references():
    from build import Group
    require(expected(Group('ldrd'),1,2,31)==(1,2,TARGET,0,0xf80000df,0xfacefeed,1,2,0xcafebabe),'load literal')
    require(expected(Group('strd',index='post',up=False,offset=255),0,0xffffffff,0)==(0xffffffff,0x5a5a5a5a,TARGET-255,255,0xdf,0xfacefeed,0xffffffff,0x5a5a5a5a,0xcafebabe),'store/post literal')
    require(expected(Group('ldrd',index='pre',offset=0xffffffff,form='reg'),1,2,0)[2]==TARGET,'wrapped pre address')
    require(expected(Group('ldrd',alias='base_high'),1,2,0)[:4]==(1,2,TARGET,0),'base/source snapshot')
    require(expected(Group('ldrd',cond=0),1,2,0)[:4]==(0xfffffffe,0xa5a5a5a7,TARGET,0),'failed condition')
    require(sum(cases(g) for g in groups())==16646144,'full domain changed')
def evaluate(path,rom,full=True):
    domain=groups(full);total=sum(cases(g,full) for g in domain)
    report=dict(status='error',console='nds',suite='doubleword_registers',expected_cases=total,observed_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','file identity')
            require(f.readline()=='VELOCE 1 ds doubleword_registers cpu=9\n','suite identity')
            require(f.readline()==f'INFO full={int(full)} groups={len(domain)} words=9\n','domain identity')
            pattern=re.compile(r'REG r=[0-9a-f]{72}\n')
            for g in domain:
                label=name(g);require(f.readline()==f'BEGIN {label} cases={cases(g,full)} words=9\n','missing/reordered group '+label)
                group=dict(id=label,observed_cases=0,mismatches=0);report['groups'].append(group)
                for a in values(g,full):
                    for b in values(g,full):
                        for flag in flags(g):
                            line=f.readline();require(pattern.fullmatch(line),'missing/malformed observation')
                            got=tuple(int(line[i:i+8],16) for i in range(6,78,8));want=expected(g,a,b,flag)
                            report['observed_cases']+=1;group['observed_cases']+=1
                            if got!=want:
                                report['mismatches']+=1;group['mismatches']+=1
                                if len(report['examples'])<8:report['examples'].append(dict(group=label,a=f'{a:08x}',b=f'{b:08x}',flags=flag,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
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
    require(report['status']=='fail' and report['mismatches']>0,'executed missing-load mutant accepted')
    require(all(g['mismatches']==0 for g in report['groups'] if g['id'].startswith('strd_')),'unrelated store mutant groups failed')
    require(all(g['mismatches']>0 for g in report['groups'] if g['id'].startswith('ldrd_') and '_byte_' in g['id']),'mutant failed to exercise every instruction form')
    content=(root/'control.result').read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('result-bit',content.replace(row,row[:6]+f'{int(row[6:14],16)^1:08x}'+row[14:],1)),
        ('ram-bit',content.replace(row,row[:62]+f'{int(row[62:70],16)^1:08x}'+row[70:],1)),
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('false-completion',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated-trailer',content.rstrip('\n')),
        ('trailing-data',content+'extra\n')):
        path=root/(name+'.result');path.write_text(broken);report=evaluate(path,rom,False);write_report(root/(name+'.json'),report)
        require(report['status']==('fail' if name in ('result-bit','ram-bit') else 'error'),'accepted '+name)
        if name in ('result-bit','ram-bit'):require(report['mismatches']==1,'mutation count '+name)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_doubleword');p.add_argument('--baseline-only',action='store_true');p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True);references()
    if a.baseline_only:
        rom=build(False);image=a.artifacts/'baseline.nds';image.write_bytes(rom);result=a.artifacts/'baseline.result';run(a.emulator,image,result,1);report=evaluate(result,rom,False);write_report(a.artifacts/'baseline.json',report);require(report['status']!='pass','missing instruction unexpectedly passed');print('Before implementation:',report['status'],report.get('detail'));return 0
    controls(a.emulator,a.artifacts)
    if a.controls_only:print('ARM9 doubleword controls PASS; executed missing-load mutant FAIL; damaged files ERROR');return 0
    rom=build();require((HERE/'doubleword_registers.nds').read_bytes()==rom,'archived image differs');result=a.artifacts/'doubleword_registers.result';run(a.emulator,HERE/'doubleword_registers.nds',result);report=evaluate(result,rom);write_report(a.artifacts/'doubleword_registers.json',report);print(f"ARM9 doubleword transfers: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']}; mismatches={report['mismatches']}")
    if report.get('detail'):print(report['detail'])
    return 0 if report['status']=='pass' else 1
if __name__=='__main__':sys.exit(main())
