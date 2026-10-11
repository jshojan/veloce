#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent signed displacement/LR/PC oracle for actual ARM-to-Thumb calls."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build,groups,fields,cases,name,CALL,LINK
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
def require(ok,why):
    if not ok:raise ValueError(why)
def expected(g,low,flags):
    encoded=g[0]+low;signed=encoded if encoded<2**23 else encoded-2**24
    target=(CALL+8+signed*4+g[1]*2)&0xffffffff
    return CALL+4,0xdf|(flags<<27),(target+6)&0xffffffff,LINK,encoded,g[1],CALL,0
def references():
    require(sum(cases(g) for g in groups())==16777088,'full domain changed')
    require(expected((0,0),0,0)[:3]==(0x02200004,0xdf,0x0220000e),'zero displacement')
    require(expected((0xff0000,1),65535,31)[:3]==(0x02200004,0xf80000df,0x0220000c),'negative/H')
def evaluate(path,rom,full=True):
    report=dict(status='error',suite='arm_blx_registers',expected_cases=sum(cases(g,full) for g in groups()),observed_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest())
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','identity')
            require(f.readline()=='VELOCE 1 ds arm_blx_registers cpu=9\n','suite')
            require(f.readline()==f'INFO full={int(full)} groups=8 flags=32\n','domain')
            for g in groups():
                require(f.readline()==f'BEGIN {name(g)} cases={cases(g,full)} words=8\n','group order')
                for low in fields(g,full):
                    for flags in range(32):
                        line=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{64}\n',line),'missing/malformed row')
                        got=tuple(int(line[i:i+8],16) for i in range(6,70,8));want=expected(g,low,flags);report['observed_cases']+=1
                        if got!=want:
                            report['mismatches']+=1
                            if len(report['examples'])<8:report['examples'].append(dict(group=name(g),low=low,flags=flags,expected=[f'{n:08x}' for n in want],observed=[f'{n:08x}' for n in got]))
            require(f.readline()==f"LOG complete cases={report['expected_cases']}\n",'false count')
            require(f.readline()=='END observations_complete\n','false completion')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'missing/ungraded trailer');require(not f.read(),'trailing data')
            report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report
def run(emulator,image,result,frames=100000):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):env.pop(key,None)
    env.update(HEADLESS='1',FRAMES=str(frames),VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    p=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=1500)
    result.with_suffix('.log').write_text(p.stdout+p.stderr);require(p.returncode==0,p.stdout+p.stderr)
def report(path,d):path.write_text(json.dumps(d,indent=2)+'\n')
def controls(emulator,root):
    rom=build(False);image=root/'control.nds';image.write_bytes(rom);result=root/'control.result';run(emulator,image,result);d=evaluate(result,rom,False);report(root/'control.json',d);require(d['status']=='pass',str(d))
    mutant=build(False,True);image=root/'mutant.nds';image.write_bytes(mutant);result=root/'mutant.result';run(emulator,image,result);d=evaluate(result,mutant,False);report(root/'mutant.json',d);require(d['status']=='fail' and d['mismatches']>0,'executed wrong H mutant accepted')
    content=(root/'control.result').read_text();row=next(s for s in content.splitlines() if s.startswith('REG r='))
    for label,broken in (
        ('link-bit',content.replace(row,row[:6]+f'{int(row[6:14],16)^1:08x}'+row[14:],1)),
        ('pc-bit',content.replace(row,row[:22]+f'{int(row[22:30],16)^2:08x}'+row[30:],1)),
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('false-completion',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=root/(label+'.result');path.write_text(broken);d=evaluate(path,rom,False);report(root/(label+'.json'),d)
        require(d['status']==('fail' if label in ('link-bit','pc-bit') else 'error'),'accepted '+label)
        if label in ('link-bit','pc-bit'):require(d['mismatches']==1,'mutation count '+label)
def main():
    p=argparse.ArgumentParser();p.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_arm_blx');p.add_argument('--baseline-only',action='store_true');p.add_argument('--controls-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True);references()
    if a.baseline_only:
        rom=build(False);image=a.artifacts/'baseline.nds';image.write_bytes(rom);result=a.artifacts/'baseline.result';run(a.emulator,image,result,1);d=evaluate(result,rom,False);report(a.artifacts/'baseline.json',d);require(d['status']!='pass','missing instruction passed');print(d);return 0
    controls(a.emulator,a.artifacts)
    if a.controls_only:print('ARM BLX controls PASS; executed wrong H mutant FAIL; damaged files ERROR');return 0
    rom=build();require((HERE/'arm_blx_registers.nds').read_bytes()==rom,'archive differs');result=a.artifacts/'arm_blx_registers.result';run(a.emulator,HERE/'arm_blx_registers.nds',result);d=evaluate(result,rom);report(a.artifacts/'arm_blx_registers.json',d);print(f"ARM BLX: {d['status'].upper()} {d['observed_cases']}/{d['expected_cases']}; mismatches={d['mismatches']}");return 0 if d['status']=='pass' else 1
if __name__=='__main__':sys.exit(main())
