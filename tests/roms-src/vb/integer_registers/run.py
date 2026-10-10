#!/usr/bin/env python3
"""Independently grade observed V810 result/PSW pairs from original guest code."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build, groups

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
MASK=(1<<32)-1


def require(ok,message):
    if not ok: raise ValueError(message)


def signed(value): return value if value<(1<<31) else value-(1<<32)


def expected(op,left,right,flags):
    carry=bool(flags&8); overflow=False
    if op in ('add','sub','cmp'):
        wide=left+right if op=='add' else left-right
        result=wide&MASK
        carry=wide>MASK if op=='add' else wide<0
        mathematical=signed(left)+signed(right) if op=='add' else signed(left)-signed(right)
        overflow=not -(1<<31)<=mathematical<(1<<31)
    elif op in ('or','and','xor'):
        result=(left|right) if op=='or' else (left&right) if op=='and' else left^right
    else:
        count=right%32; carry=False
        if op=='shl':
            result=(left*(1<<count))&MASK
            if count: carry=bool((left>>(32-count))&1)
        elif op=='shr':
            result=left//(1<<count)
            if count: carry=bool((left>>(count-1))&1)
        else:
            result=(signed(left)//(1<<count))&MASK
            if count: carry=bool((left>>(count-1))&1)
    psw=int(result==0)|int(result>=(1<<31))*2|int(overflow)*4|int(carry)*8
    return (left if op=='cmp' else result,psw)


def references():
    for op,a,b,flags,want in (
        ('add',0x7fffffff,1,15,(0x80000000,6)),('add',MASK,1,0,(0,9)),
        ('sub',0,1,0,(MASK,10)),('sub',0x80000000,1,0,(0x7fffffff,4)),
        ('cmp',0,1,15,(0,10)),('and',MASK,0,15,(0,9)),
        ('xor',MASK,MASK,0,(0,1)),('or',0x80000000,0,8,(0x80000000,10)),
        ('shl',1,0,15,(1,0)),('shl',0x80000000,1,0,(0,9)),
        ('shr',0x80000001,31,15,(1,0)),('sar',0x80000001,31,0,(MASK,2)),
        ('sar',MASK,32,15,(MASK,2)),('shr',1,1,0,(0,9)),
    ): require(expected(op,a,b,flags)==want,'oracle reference failed: '+op)


def evaluate(path,rom,byte_values=256):
    domain=groups(byte_values); total=sum(len(a)*len(b)*16 for *_,a,b in domain)
    report=dict(status='error',console='vb',suite='integer_registers',expected_cases=total,
                observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],
                rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 vb integer_registers\n','wrong suite')
            require(f.readline()==f'INFO byte_values={byte_values} flags=16 groups={len(domain)}\n','wrong domain')
            for name,op,_,left,right in domain:
                require(f.readline()==f'BEGIN {name} cases={len(left)*len(right)*16} words=2\n','missing/reordered group')
                group=dict(id=name,observed_cases=0,mismatches=0); report['groups'].append(group)
                for a in left:
                    for b in right:
                        for flags in range(16):
                            line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{16}\n',line),'missing/malformed observation')
                            got=(int(line[6:14],16),int(line[14:22],16)); want=expected(op,a,b,flags)
                            report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                            if got!=want:
                                report['mismatches']+=1; group['mismatches']+=1
                                if len(report['examples'])<8:
                                    report['examples'].append(dict(group=name,left=f'{a:08x}',right=f'{b:08x}',flags=flags,
                                        expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
            require(f.readline()==f'LOG complete cases={total}\n','missing completion')
            require(f.readline()=='END observations_complete\n','false END')
            trailer=f.readline()
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',trailer),'missing/ungraded trailer')
            require(not f.read(),'trailing data')
            report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e: report['detail']=str(e)
    return report


def run(emulator,image,result):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):
        env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='60000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=400)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    rom=build(16,wrong_add=True); image=artifacts/'negative.vb'; image.write_bytes(rom)
    result=artifacts/'negative.result'; run(emulator,image,result)
    report=evaluate(result,rom,16)
    require(report['status']=='fail' and all(g['mismatches']>0 if g['id'].startswith('add_') else g['mismatches']==0
            for g in report['groups']),'executed ADD mutation not isolated/rejected: '+json.dumps(report))
    (artifacts/'negative.json').write_text(json.dumps(report,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN add_boundary','BEGIN sub_boundary',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,rom,16); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_integer')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'integer_registers.vb').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('VB controls: executed mutant FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'integer_registers.result'; run(args.emulator,HERE/'integer_registers.vb',result)
    report=evaluate(result,rom); (args.artifacts/'integer_registers.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB integer registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} result/PSW pairs; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
