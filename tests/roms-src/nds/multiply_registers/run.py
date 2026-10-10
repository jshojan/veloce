#!/usr/bin/env python3
"""Independently grade observed dual-CPU DS ARM result/CPSR/source registers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build, groups, cases, PATTERNS

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
MASK=(1<<32)-1


def require(ok,message):
    if not ok: raise ValueError(message)


def signed(value): return value if value<(1<<31) else value-(1<<32)


def expected(op,left,right,flags,seed,set_flags,cpu=7):
    psw=0xdf|(flags<<(27 if cpu==9 else 28))
    long=op not in ('mul','mla')
    product=(signed(left)*signed(right)) if op.startswith('s') else left*right
    result=product+(seed if long else seed&MASK) if op in ('mla','umlal','smlal') else product
    result&=(1<<64)-1 if long else MASK
    if set_flags:
        psw=(psw&0x3fffffff)|(int(result>=(1<<(63 if long else 31)))<<31)|(int(result==0)<<30)
    return (result&MASK,result>>32 if long else seed>>32,psw,left,right)


def score_mask(op,set_flags,cpu):
    return 0xffffffff ^ (0x30000000 if op not in ('mul','mla') else 0x20000000) if cpu==7 and set_flags else 0xffffffff


def references():
    require(sum(cases(g,cpu) for g in groups() for cpu in (9,7))==38338560,'declared count changed')
    for g in groups():
        require(g[3]==(PATTERNS if '_boundary_' in g[0] else tuple(n*0x01010101 for n in range(256))),'operand domain changed')
    for op,a,b,seed,set_flags,want in (
        ('mul',0xffffffff,2,0,True,(0xfffffffe,0,0x800000df,0xffffffff,2)),
        ('mla',0xffffffff,2,2,True,(0,0,0x400000df,0xffffffff,2)),
        ('umull',0xffffffff,0xffffffff,0,True,(1,0xfffffffe,0x800000df,0xffffffff,0xffffffff)),
        ('smull',0xffffffff,0xffffffff,0,True,(1,0,0xdf,0xffffffff,0xffffffff)),
        ('umlal',1,1,0xffffffffffffffff,True,(0,0,0x400000df,1,1)),
        ('smlal',0x80000000,2,0x100000000,True,(0,0,0x400000df,0x80000000,2)),
    ): require(expected(op,a,b,0,seed,set_flags)==want,'oracle reference failed: '+op)
    require(score_mask('mul',True,7)==0xdfffffff and score_mask('smull',True,7)==0xcfffffff and score_mask('smull',False,7)==MASK,'undefined flag mask wrong')


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g,cpu) for g in domain for cpu in (9,7))
    report=dict(status='error',console='nds',suite='multiply_registers',expected_cases=total,observed_cases=0,
        graded_cases=0,mismatches=0,masked_cpsr_cases=sum(cases(g,7) for g in domain if g[2]),
        unscored_cpsr_bits=sum(cases(g,7)*(1 if g[1] in ('mul','mla') else 2) for g in domain if g[2]),groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            for cpu in (9,7):
                require(f.readline()==f'VELOCE 1 ds multiply_registers cpu={cpu}\n','wrong CPU/suite order')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n','wrong domain')
                for name,op,set_flags,values,seed in domain:
                    require(f.readline()==f'BEGIN cpu{cpu}_{name} cases={cases((name,op,set_flags,values,seed),cpu)} words=5\n','missing/reordered group')
                    group=dict(id=f'cpu{cpu}_{name}',observed_cases=0,mismatches=0); report['groups'].append(group)
                    for a in values:
                        for b in values:
                            for flags in range(32 if cpu==9 else 16):
                                line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{40}\n',line),'missing/malformed observation')
                                got=tuple(int(line[n:n+8],16) for n in (6,14,22,30,38)); want=expected(op,a,b,flags,seed,set_flags,cpu)
                                report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                                if got[:2]+got[3:]!=want[:2]+want[3:] or ((got[2]^want[2])&score_mask(op,set_flags,cpu)):
                                    report['mismatches']+=1; group['mismatches']+=1
                                    if len(report['examples'])<8: report['examples'].append(dict(cpu=cpu,group=name,left=f'{a:08x}',right=f'{b:08x}',flags=flags,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
                require(f.readline()==f'LOG cpu={cpu} complete cases={sum(cases(g,cpu) for g in domain)}\n','missing CPU completion')
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
                        capture_output=True,text=True,timeout=1200)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    baseline=build(False); image=artifacts/'control.nds'; image.write_bytes(baseline)
    result=artifacts/'control.result'; run(emulator,image,result); report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    rom=build(False,wrong_signed=True); image=artifacts/'wrong-signed.nds'; image.write_bytes(rom)
    path=artifacts/'wrong-signed.result'; run(emulator,image,path); invalid=evaluate(path,rom,False)
    require(invalid['status']=='fail' and all(g['mismatches']>0 if ('_smull_' in g['id'] or '_smlal_' in g['id']) else g['mismatches']==0 for g in invalid['groups']),'signed-to-unsigned mutation not isolated/rejected: '+json.dumps(invalid))
    (artifacts/'wrong-signed.json').write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,group,bit,status in (
        ('arm7-short-c','cpu7_mul_1_boundary_0000000000000000',0x20000000,'pass'),
        ('arm7-long-cv','cpu7_smull_1_boundary_0000000000000000',0x30000000,'pass'),
        ('arm7-short-v','cpu7_mul_1_boundary_0000000000000000',0x10000000,'fail'),
        ('arm9-long-c','cpu9_smull_1_boundary_0000000000000000',0x20000000,'fail'),
        ('arm7-nons-c','cpu7_smull_0_boundary_0000000000000000',0x20000000,'fail')):
        offset=content.index('REG r=',content.index('BEGIN '+group))+22
        changed=content[:offset]+f'{int(content[offset:offset+8],16)^bit:08x}'+content[offset+8:]
        path=artifacts/(name+'.result'); path.write_text(changed); invalid=evaluate(path,baseline,False)
        require(invalid['status']==status and invalid['mismatches']==(0 if status=='pass' else 1),name+' scored incorrectly')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN cpu9_mul','BEGIN cpu7_mul',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_multiply')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'multiply_registers.nds').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('DS controls: executed mutant FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'multiply_registers.result'; run(args.emulator,HERE/'multiply_registers.nds',result)
    report=evaluate(result,rom); (args.artifacts/'multiply_registers.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"DS dual-CPU multiply registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} multiply result/PSW/source snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
