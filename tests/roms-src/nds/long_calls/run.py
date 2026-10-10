#!/usr/bin/env python3
"""Independently grade valid adjacent DS Thumb BL and ARM9 BLX calls."""
import argparse
from dataclasses import asdict
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build,groups,cases,fields,seeds,Group,LINK_SEED
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
MASK=(1<<32)-1
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(g,field,value,flags,cpu):
    high=field if g.high else int(g.exchange)
    low=(2 if g.exchange else 0) if g.high else field
    source=g.base+g.align-(0 if g.high else field*2)
    signed_high=high if high<1024 else high-2048
    destination=source+4+signed_high*4096+low*2
    # Both Thumb entry halfwords converge on a PC read at word-base+4;
    # ARM targets store LR then read PC in their third word instruction.
    pc_read=(destination//4)*4+(16 if g.exchange else 8)
    link=(source+4)|1
    return (link,0xdf|(flags<<(27 if cpu==9 else 28)),pc_read,value,high,low,source,link if g.exchange else LINK_SEED)
def references():
    require(sum(cases(g,c) for c in (9,7) for g in groups(c))==9437184,'full domain changed')
    require(sum(cases(g,c,False) for c in (9,7) for g in groups(c))==9472,'control domain changed')
    for c in (9,7):
        for g in groups(c):
            require(fields(g)==tuple(range(0,2048,2 if g.kind==3 else 1)),'encoding domain changed')
            for f in fields(g):
                r=expected(g,f,0,0,c)
                require(0x02000000<=r[2]<0x03000000 and not (r[6]&1),'call outside mapped/halfword-aligned domain')
                high=r[4] if r[4]<1024 else r[4]-2048
                destination=r[6]+4+high*4096+r[5]*2
                require((destination//4*4 if g.exchange else destination)!=r[6]+2,'unpredictable self-suffix in ROM domain')
    require(expected(Group(0,0),1024,0,0,7)==(0x02802205,0xdf,0x0240220c,0,1024,0,0x02802200,LINK_SEED),'negative high reference failed')
    require(expected(Group(3,2),0,0x89abcdef,31,9)==(0x02803003,0xf80000df,0x02804010,0x89abcdef,1,0,0x02802ffe,0x02803003),'halfword BLX source/alignment/Q reference failed')
    require(expected(Group(1,0),2047,0,0,7)[2:7]==(0x02803008,0,0,2047,0x02801ffe),'low-field/page-crossing reference failed')
def evaluate(path,rom,full=True):
    total=sum(cases(g,c,full) for c in (9,7) for g in groups(c))
    report=dict(status='error',console='nds',suite='long_calls',expected_cases=total,observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            for cpu in (9,7):
                domain=groups(cpu)
                require(f.readline()==f'VELOCE 1 ds long_calls cpu={cpu}\n','wrong CPU/suite order')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n','wrong domain')
                for g in domain:
                    require(f.readline()==f'BEGIN cpu{cpu}_{g.name} cases={cases(g,cpu,full)} words=8\n','missing/reordered group')
                    group=dict(id=f'cpu{cpu}_{g.name}',instruction=asdict(g),observed_cases=0,mismatches=0); report['groups'].append(group)
                    for field in fields(g,full):
                        for value in seeds(full):
                            for flags in range(32 if cpu==9 else 16):
                                line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{64}\n',line),'missing/malformed observation')
                                got=tuple(int(line[n:n+8],16) for n in range(6,70,8)); want=expected(g,field,value,flags,cpu)
                                report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                                if got!=want:
                                    report['mismatches']+=1; group['mismatches']+=1
                                    if len(report['examples'])<8: report['examples'].append(dict(cpu=cpu,group=g.name,field=field,value=f'{value:08x}',flags=flags,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
                require(f.readline()==f'LOG cpu={cpu} complete cases={sum(cases(g,cpu,full) for g in domain)}\n','missing CPU completion')
            require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'missing/ungraded trailer')
            require(not f.read(),'trailing data'); report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e: report['detail']=str(e)
    return report
def run(emulator,image,result):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):
        env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='1000000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=2400)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    baseline=build(False); image=artifacts/'control.nds'; image.write_bytes(baseline)
    result=artifacts/'control.result'; run(emulator,image,result); report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,options,count in (('wrong-sign',dict(wrong_sign=True),9472),('wrong-link',dict(wrong_link=True),3328)):
        rom=build(False,**options); image=artifacts/(name+'.nds'); image.write_bytes(rom)
        path=artifacts/(name+'.result'); run(emulator,image,path); invalid=evaluate(path,rom,False)
        require(invalid['status']=='fail' and invalid['mismatches']==count,'executed '+name+' mutation escaped: '+json.dumps(invalid))
        if name=='wrong-link': require(all(g['mismatches']==g['observed_cases'] if g['instruction']['kind']&2 else g['mismatches']==0 for g in invalid['groups']),'wrong-link mutation not isolated to BLX')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for word in range(8):
        index=6+word*8; damaged=row[:index]+f'{int(row[index:index+8],16)^1:08x}'+row[index+8:]
        path=artifacts/f'field-{word}.result'; path.write_text(content.replace(row,damaged,1))
        invalid=evaluate(path,baseline,False); require(invalid['status']=='fail' and invalid['mismatches']==1,'record field not independently graded')
        (artifacts/f'field-{word}.json').write_text(json.dumps(invalid,indent=2)+'\n')
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN cpu9_bl_high_a0','BEGIN cpu7_bl_high_a0',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_long_calls')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'long_calls.nds').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('DS controls: 2 executed mutants and 8 field mutations FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'long_calls.result'; run(args.emulator,HERE/'long_calls.nds',result)
    report=evaluate(result,rom); (args.artifacts/'long_calls.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"DS dual-CPU long-call registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} link/PSW/PC/input/source/memory snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
