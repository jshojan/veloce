#!/usr/bin/env python3
"""Independently grade observed V810 multiply/divide and exception registers from original guest code."""
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


def expected(op,left,right,flags,pc):
    if op.startswith('div') and right==0:
        return (left,0x13579bdf,flags,1,pc,flags,0xff80,flags|0x5000,right)
    if op.startswith('mul'):
        product=left*right if op=='mulu' else signed(left)*signed(right)
        result=product&MASK; high=(product>>32)&MASK
        overflow=product>MASK if op=='mulu' else not -(1<<31)<=product<(1<<31)
    else:
        if op=='divu': quotient,remainder=divmod(left,right)
        else:
            dividend,divisor=signed(left),signed(right)
            quotient=abs(dividend)//abs(divisor)
            if (dividend<0)!=(divisor<0): quotient=-quotient
            remainder=dividend-quotient*divisor
        result=quotient&MASK; high=remainder&MASK
        overflow=op=='div' and quotient>=(1<<31)
    psw=int(result==0)|int(result>=(1<<31))*2|int(overflow)*4|(flags&8)
    return (result,high,psw,0,0,0,0,0,right)


def references():
    for op,a,b,flags,want in (
        ('mul',0xffffffff,2,8,(0xfffffffe,0xffffffff,10)),
        ('mul',0x80000000,0xffffffff,0,(0x80000000,0,6)),
        ('mulu',0xffffffff,0xffffffff,0,(1,0xfffffffe,4)),
        ('div',0xfffffff9,3,8,(0xfffffffe,0xffffffff,10)),
        ('div',7,0xfffffffd,0,(0xfffffffe,1,2)),
        ('div',0x80000000,0xffffffff,8,(0x80000000,0,14)),
        ('divu',0xffffffff,1,0,(0xffffffff,0,2)),
        ('divu',1,2,15,(0,1,9)),
    ): require(expected(op,a,b,flags,0)[:3]==want,'oracle reference failed: '+op)
    require(expected('div',42,0,15,0x7000000)==
            (42,0x13579bdf,15,1,0x7000000,15,0xff80,0x500f,0),'exception oracle reference failed')


def evaluate(path,rom,byte_values=256):
    pcs={}; build(byte_values,metadata=pcs)
    domain=groups(byte_values); total=sum(len(a)*len(b)*16 for *_,a,b in domain)
    report=dict(status='error',console='vb',suite='muldiv',expected_cases=total,
                observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],
                rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 vb muldiv\n','wrong suite')
            require(f.readline()==f'INFO byte_values={byte_values} flags=16 groups={len(domain)}\n','wrong domain')
            for name,op,_,left,right in domain:
                require(f.readline()==f'BEGIN {name} cases={len(left)*len(right)*16} words=9\n','missing/reordered group')
                group=dict(id=name,observed_cases=0,mismatches=0); report['groups'].append(group)
                for a in left:
                    for b in right:
                        for flags in range(16):
                            line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{72}\n',line),'missing/malformed observation')
                            got=tuple(int(line[n:n+8],16) for n in range(6,78,8)); want=expected(op,a,b,flags,pcs[name])
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
    env.update(HEADLESS='1',FRAMES='120000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=700)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    rom=build(16,wrong_mul=True); image=artifacts/'negative.vb'; image.write_bytes(rom)
    result=artifacts/'negative.result'; run(emulator,image,result)
    report=evaluate(result,rom,16)
    require(report['status']=='fail' and all(g['mismatches']>0 if g['id']=='mul_boundary' else g['mismatches']==0
            for g in report['groups']),'executed MUL mutation not isolated/rejected: '+json.dumps(report))
    (artifacts/'negative.json').write_text(json.dumps(report,indent=2)+'\n')
    fault_rom=build(16,wrong_exception=True); fault_image=artifacts/'negative-exception.vb'; fault_image.write_bytes(fault_rom)
    fault_result=artifacts/'negative-exception.result'; run(emulator,fault_image,fault_result)
    fault_report=evaluate(fault_result,fault_rom,16)
    require(fault_report['status']=='fail' and fault_report['mismatches']==1024 and all(
        g['mismatches']==256 if g['id'].startswith('div') else g['mismatches']==0
        for g in fault_report['groups']),'executed exception capture mutation not isolated/rejected')
    (artifacts/'negative-exception.json').write_text(json.dumps(fault_report,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN mul_boundary','BEGIN div_boundary',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,rom,16); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_muldiv')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'muldiv.vb').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('VB controls: executed arithmetic/exception mutants FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'muldiv.result'; run(args.emulator,HERE/'muldiv.vb',result)
    report=evaluate(result,rom); (args.artifacts/'muldiv.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB multiply/divide: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} register snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
