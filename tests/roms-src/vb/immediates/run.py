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
from build import build, groups, cases, PATTERNS, BOUNDARIES

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
MASK=(1<<32)-1


def require(ok,message):
    if not ok: raise ValueError(message)


def signed(value): return value if value<(1<<31) else value-(1<<32)


def expected(op,left,raw,flags):
    incoming=0x8000|flags
    if op=='mov': return ((raw if raw<16 else raw-32)&MASK,incoming,left)
    if op in ('movea','movhi'):
        delta=(raw if raw<32768 else raw-65536) if op=='movea' else raw*65536
        return ((left+delta)&MASK,incoming,left)
    overflow=False; carry=bool(flags&8)
    if op in ('add','addi','cmp'):
        width=65536 if op=='addi' else 32
        immediate=raw if raw<width//2 else raw-width
        unsigned_immediate=immediate&MASK
        mathematical=signed(left)-immediate if op=='cmp' else signed(left)+immediate
        unsigned_total=left-unsigned_immediate if op=='cmp' else left+unsigned_immediate
        result=mathematical&MASK
        carry=unsigned_total<0 if op=='cmp' else unsigned_total>MASK
        overflow=not -(1<<31)<=mathematical<(1<<31)
    elif op in ('ori','andi','xori'):
        result=(left|raw) if op=='ori' else (left&raw) if op=='andi' else left^raw
    else:
        carry=False
        if op=='shl':
            result=(left*(1<<raw))&MASK
            if raw: carry=bool((left>>(32-raw))&1)
        elif op=='shr':
            result=left//(1<<raw)
            if raw: carry=bool((left>>(raw-1))&1)
        else:
            result=(signed(left)//(1<<raw))&MASK
            if raw: carry=bool((left>>(raw-1))&1)
    psw=0x8000|int(result==0)|int(result>=(1<<31))*2|int(overflow)*4|int(carry)*8
    return (left if op=='cmp' else result,psw,left)


def references():
    domain=groups(); require(sum(cases(g) for g in domain)==100712448,'domain count changed')
    for g in domain:
        require(tuple(g[3])==PATTERNS and len(set(g[3]))==16,'operand pattern domain changed')
        size=65536 if g[2]>=0x28 else 32
        require(tuple(g[4])==tuple(range(size)),'immediate domain incomplete')
    for op,a,b,flags,want in (
        ('movea',0,65535,15,(MASK,0x800f,0)),('movhi',MASK,1,15,(65535,0x800f,MASK)),
        ('mov',MASK,16,15,(0xfffffff0,0x800f,MASK)),
        ('addi',0x7fffffff,1,15,(0x80000000,0x8006,0x7fffffff)),
        ('addi',1,65535,0,(0,0x8009,1)),('add',0,31,0,(MASK,0x8002,0)),
        ('cmp',0,31,15,(0,0x8008,0)),('andi',MASK,65535,15,(65535,0x8008,MASK)),
        ('ori',0x80000000,65535,8,(0x8000ffff,0x800a,0x80000000)),
        ('xori',65535,65535,0,(0,0x8001,65535)),
        ('shl',1,0,15,(1,0x8000,1)),('shl',0x80000000,1,0,(0,0x8009,0x80000000)),
        ('shr',0x80000001,31,15,(1,0x8000,0x80000001)),
        ('sar',0x80000001,31,0,(MASK,0x8002,0x80000001)),
    ): require(expected(op,a,b,flags)==want,'oracle reference failed: '+op)


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g) for g in domain)
    report=dict(status='error',console='vb',suite='immediates',expected_cases=total,
                observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],
                rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 vb immediates\n','wrong suite')
            require(f.readline()==f'INFO full={int(full)} flags=16 groups={len(domain)}\n','wrong domain')
            for name,op,_,left,right in domain:
                require(f.readline()==f'BEGIN {name} cases={len(left)*len(right)*16} words=3\n','missing/reordered group')
                group=dict(id=name,observed_cases=0,mismatches=0); report['groups'].append(group)
                for a in left:
                    for b in right:
                        for flags in range(16):
                            line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{24}\n',line),'missing/malformed observation')
                            got=tuple(int(line[n:n+8],16) for n in (6,14,22)); want=expected(op,a,b,flags)
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
    env.update(HEADLESS='1',FRAMES='1500000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=2100)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    baseline=build(False); image=artifacts/'control.vb'; image.write_bytes(baseline)
    result=artifacts/'control.result'; run(emulator,image,result); report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    for mutant,options,target in (('wrong-addi',{'wrong_addi':True},'addi'),('wrong-mov',{'wrong_mov':True},'mov')):
        rom=build(False,**options); image=artifacts/(mutant+'.vb'); image.write_bytes(rom)
        path=artifacts/(mutant+'.result'); run(emulator,image,path); invalid=evaluate(path,rom,False)
        require(invalid['status']=='fail' and all(g['mismatches']>0 if g['id']==target else g['mismatches']==0
            for g in invalid['groups']),'executed mutant not isolated/rejected: '+json.dumps(invalid))
        (artifacts/(mutant+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN movea','BEGIN movhi',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_immediates')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'immediates.vb').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('VB controls: 2 executed mutants FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'immediates.result'; run(args.emulator,HERE/'immediates.vb',result)
    report=evaluate(result,rom); (args.artifacts/'immediates.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB immediate registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} result/PSW/source snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
