#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent integer/list/address oracle for original DS ARM LDM/STM."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build,groups,masks,seeds,flag_values,cases,name,program,TARGET
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];MASK=0xffffffff

def require(ok,why):
    if not ok:raise ValueError(why)
def predicate(cond,psw):
    n,z,c,v=(bool(psw&(1<<bit)) for bit in (31,30,29,28))
    return (z,not z,c,not c,n,not n,v,not v,c and not z,not c or z,n==v,n!=v,not z and n==v,z or n!=v,True)[cond]
def expected(g,cpu,mask,seed,flag,layout):
    listed=[r for r in range(16) if mask&(1<<r)];count=len(listed)
    stimulus=mask|(mask<<16) if seed is None else seed
    regs=[((r+1)*0x01020304)^stimulus for r in range(15)]
    base=TARGET+{'ia':0,'ib':-4,'da':4*count-4,'db':4*count}[g.mode];regs[g.rn]=base
    memory=[0xfacefeed]+[(((i+1)*0x11111111)&MASK)^stimulus for i in range(16)]+[0xcafebabe]
    if g.load and mask&0x8000:memory[count]=layout['capture']
    psw=0xdf|(flag<<(27 if cpu==9 else 28))
    if predicate(g.cond,psw):
        for index,reg in enumerate(listed):
            if g.load:
                if reg<15:regs[reg]=memory[index+1]
            else:memory[index+1]=layout['transfer']+(8 if cpu==9 else 12) if reg==15 else regs[reg]
        if g.wb:regs[g.rn]=(base+(4*count if g.mode in ('ia','ib') else -4*count))&MASK
    return (mask,)+tuple(regs)+(psw,)+tuple(memory)
def references():
    from build import Group
    layout={'capture':0x02010000,'transfer':0x02008000}
    row=expected(Group(True,'ia',False),9,3,0,0,layout)
    require(row[:3]==(3,0x11111111,0x22222222) and row[16]==0xdf,'literal load/order')
    row=expected(Group(False,'db',True),7,3,0,15,layout)
    require(row[4]==TARGET and row[16]==0xf00000df and row[18:20]==(0x01020304,0x02040608),'literal store/writeback/flags')
    require(expected(Group(True,'ia',False,cond=0),9,1,0,0,layout)[1]==0x01020304,'failed predicate')
    require(sum(cases(g,cpu) for cpu in (9,7) for g in groups())==2881632,'full domain changed')
def evaluate(path,rom,full=True):
    domain=groups(full);total=sum(cases(g,cpu,full) for cpu in (9,7) for g in domain)
    report=dict(status='error',console='nds',suite='arm_multiple_registers',expected_cases=total,observed_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','file identity')
            pattern=re.compile(r'REG r=[0-9a-f]{136}\n');memory_pattern=re.compile(r'MEM m=[0-9a-f]{144}\n')
            for cpu in (9,7):
                _,layout=program(cpu,full,layout=True)
                require(f.readline()==f'VELOCE 1 ds arm_multiple_registers cpu={cpu}\n','CPU identity')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} transfer={layout["transfer"]:08x} capture={layout["capture"]:08x} words=35\n','domain/layout identity')
                for g in domain:
                    label=name(g);require(f.readline()==f'BEGIN {label} cases={cases(g,cpu,full)} words=35\n','missing/reordered group '+label)
                    group=dict(id=f'{cpu}_{label}',observed_cases=0,mismatches=0);report['groups'].append(group)
                    for mask in masks(g,full):
                        for seed in seeds(g):
                            for flag in flag_values(g,cpu,mask):
                                line=f.readline();require(pattern.fullmatch(line),'missing/malformed observation')
                                memory_line=f.readline();require(memory_pattern.fullmatch(memory_line),'missing/malformed memory observation')
                                got=tuple(int(line[i:i+8],16) for i in range(6,142,8))+tuple(int(memory_line[i:i+8],16) for i in range(6,150,8));want=expected(g,cpu,mask,seed,flag,layout)
                                report['observed_cases']+=1;group['observed_cases']+=1
                                if got!=want:
                                    report['mismatches']+=1;group['mismatches']+=1
                                    if len(report['examples'])<8:report['examples'].append(dict(group=group['id'],mask=f'{mask:04x}',seed=seed,flags=flag,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
                require(f.readline()==f'LOG complete cpu={cpu} cases={sum(cases(g,cpu,full) for g in domain)}\n','false count')
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
    require(report['status']=='fail' and report['mismatches']>0,'executed wrong-start mutant accepted')
    require(all(g['mismatches']==0 for g in report['groups'] if '_stm_ib_' not in g['id']),'unrelated store mutant groups failed')
    require(all(g['mismatches']>0 for g in report['groups'] if '_stm_ib_' in g['id'] and '_lists_' in g['id']),'mutant failed to exercise store addressing change')
    content=(root/'control.result').read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    memory_row=next(line for line in content.splitlines() if line.startswith('MEM m='))
    for name,broken in (
        ('result-bit',content.replace(row,row[:14]+f'{int(row[14:22],16)^1:08x}'+row[22:],1)),
        ('ram-bit',content.replace(memory_row,memory_row[:14]+f'{int(memory_row[14:22],16)^1:08x}'+memory_row[22:],1)),
        ('missing-memory-row',content.replace(memory_row+'\n','',1)),('malformed-memory-row',content.replace(memory_row,'MEM m=bad',1)),
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('false-completion',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated-trailer',content.rstrip('\n')),
        ('trailing-data',content+'extra\n')):
        path=root/(name+'.result');path.write_text(broken);report=evaluate(path,rom,False);write_report(root/(name+'.json'),report)
        require(report['status']==('fail' if name in ('result-bit','ram-bit') else 'error'),'accepted '+name)
        if name in ('result-bit','ram-bit'):require(report['mismatches']==1,'mutation count '+name)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_arm_multiple');mode=p.add_mutually_exclusive_group();mode.add_argument('--baseline-only',action='store_true');mode.add_argument('--controls-only',action='store_true');mode.add_argument('--full-only',action='store_true',help='run full corpus after separately verified controls');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True);references()
    if a.baseline_only:
        rom=build(False);image=a.artifacts/'baseline.nds';image.write_bytes(rom);result=a.artifacts/'baseline.result';run(a.emulator,image,result,1);report=evaluate(result,rom,False);write_report(a.artifacts/'baseline.json',report);require(report['status']!='pass','missing instruction unexpectedly passed');print('Before implementation:',report['status'],report.get('detail'));return 0
    if not a.full_only:controls(a.emulator,a.artifacts)
    if a.controls_only:print('ARM block controls PASS; executed wrong-start mutant FAIL; damaged files ERROR');return 0
    rom=build();require((HERE/'arm_multiple_registers.nds').read_bytes()==rom,'archived image differs');result=a.artifacts/'arm_multiple_registers.result';run(a.emulator,HERE/'arm_multiple_registers.nds',result);report=evaluate(result,rom);write_report(a.artifacts/'arm_multiple_registers.json',report);print(f"ARM block transfers: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']}; mismatches={report['mismatches']}")
    if report.get('detail'):print(report['detail'])
    return 0 if report['status']=='pass' else 1
if __name__=='__main__':sys.exit(main())
