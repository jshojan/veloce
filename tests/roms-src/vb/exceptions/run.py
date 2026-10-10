#!/usr/bin/env python3
"""Independently grade observed V810 TRAP/reserved-opcode exception registers from original guest code."""
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


def require(ok,message):
    if not ok: raise ValueError(message)


def expected(word,psw,ecr):
    trap=word//1024==0x18
    code=0xffa0+(word%32) if trap else 0xff90
    restore_pc=0x05003002 if trap else 0x05003000
    nested=bool(psw&0x4000)
    if nested:
        ecr=(code<<16)|(ecr&0xffff)
        regs=(0x05001000,0x101,0x05003002,psw,ecr,psw)
        tag=3
    else:
        ecr=(ecr&0xffff0000)|code
        regs=(0x05003002,psw,0x05002000,0x202,ecr,psw)
        tag=(1 if word%32<16 else 2) if trap else 0
    entry=(psw|(0x9000 if nested else 0x5000))&~0x2000
    return regs+(tag,restore_pc,entry,psw,0x89abcdef,0x76543210,0x13579bdf),ecr


def references():
    for word,psw,old,want in (
        (0x6000,15,0x12345678,(0x500f,0x1234ffa0,0x05003002,1)),
        (0x601f,0x632f,0x12345678,(0xd32f,0xffbf5678,0x05003002,3)),
        (0x6c00,0x232f,0x12345678,(0x532f,0x1234ff90,0x05003000,0)),
    ):
        values,ecr=expected(word,psw,old)
        require((values[8],ecr,values[7],values[6])==want,'oracle exception reference failed')


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(len(words)*len(statuses) for _,_,words,statuses in domain)
    report=dict(status='error',console='vb',suite='exceptions',expected_cases=total,
                observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],
                rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 vb exceptions\n','wrong suite')
            require(f.readline()==f'INFO full={int(full)} groups={len(domain)}\n','wrong domain')
            ecr=0xfff0
            for name,_,words,statuses in domain:
                count=len(words)*len(statuses)
                require(f.readline()==f'BEGIN {name} cases={count} words=13\n','missing/reordered group')
                group=dict(id=name,observed_cases=0,mismatches=0); report['groups'].append(group)
                for word in words:
                    for psw in statuses:
                        line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{104}\n',line),'missing/malformed observation')
                        got=tuple(int(line[n:n+8],16) for n in range(6,110,8)); want,ecr=expected(word,psw,ecr)
                        report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                        if got!=want:
                            report['mismatches']+=1; group['mismatches']+=1
                            if len(report['examples'])<8:
                                report['examples'].append(dict(group=name,instruction=f'{word:04x}',psw=f'{psw:08x}',
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
    baseline=build(False); image=artifacts/'control.vb'; image.write_bytes(baseline)
    result=artifacts/'control.result'; run(emulator,image,result)
    report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive control failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,rom in (('wrong-vector',build(False,wrong_trap=True)),('wrong-capture',build(False,wrong_capture=True))):
        image=artifacts/(name+'.vb'); image.write_bytes(rom)
        path=artifacts/(name+'.result'); run(emulator,image,path); invalid=evaluate(path,rom,False)
        require(invalid['status']=='fail',name+' mutation accepted: '+json.dumps(invalid))
        if name=='wrong-vector': require(all(g['mismatches']>0 for g in invalid['groups'] if g['id'].startswith('trap_')),'trap mutation missed')
        else: require(invalid['mismatches']==invalid['expected_cases'],'capture mutation not exhaustive')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN trap_00','BEGIN trap_01',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_exceptions')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'exceptions.vb').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('VB controls: executed vector/context mutants FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'exceptions.result'; run(args.emulator,HERE/'exceptions.vb',result)
    report=evaluate(result,rom); (args.artifacts/'exceptions.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB exceptions: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} register snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
