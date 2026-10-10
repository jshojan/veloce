#!/usr/bin/env python3
"""Independently grade observed V810 address-trap registers from original guest code."""
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


def expected(address,psw,ecr):
    adtre=address&0xfffffffe; hit=bool(psw&0x2000) and adtre==0x05003000
    after=psw&~0x2000
    regs=[0x05001000,0x101,0x05002000,0x202,ecr,after,adtre]
    context=(0,0,0,0)
    if hit:
        nested=bool(psw&0x4000); bank=2 if nested else 0
        regs[bank]=0x05003000; regs[bank+1]=after
        ecr=(0xffc0<<16)|(ecr&0xffff) if nested else (ecr&0xffff0000)|0xffc0
        regs[4]=ecr
        context=(2 if nested else 1,0x05003000,(psw|(0x9000 if nested else 0x5000))&~0x2000,psw)
    return tuple(regs)+context+(0x89abcdef,0x76543210,0x13579bdf),ecr,hit


def references():
    # Cardinality and containment prove the enumerated domains are complete;
    # this is independent of the builder's iteration order/loop structure.
    domain=groups(); complete=domain[0][2]
    require(len(complete)==len(set(complete))==131072 and all(v&~0xf73ff==0 for v in complete),'nonfatal PSW domain incomplete')
    for _,_,statuses in domain[1:]:
        require(len(statuses)==len(set(statuses))==64 and all(v&~0x600f==0 for v in statuses),'AE/EP/flag domain incomplete')
    flips=domain[1][1]
    require(len(flips)==len(set(flips))==32 and all((v^0x05003000).bit_count()==1 for v in flips),'single-bit address domain incomplete')
    require(set(domain[2][1])==set(range(0x05003000,0x05013000)),'low-half address domain incomplete')
    require(sum(len(a)*len(p) for _,a,p in domain)==4982784,'full case count changed')
    values,ecr,hit=expected(0x05003001,0x232f,0x12345678)
    require(hit and values[:7]==(0x05003000,0x32f,0x05002000,0x202,0x1234ffc0,0x32f,0x05003000)
        and values[7:11]==(1,0x05003000,0x532f,0x232f),'normal oracle reference failed')
    values,ecr,hit=expected(0x05003000,0x632f,0x12345678)
    require(hit and values[2:6]==(0x05003000,0x432f,0xffc05678,0x432f)
        and values[7:11]==(2,0x05003000,0xd32f,0x632f),'nested oracle reference failed')
    for address,psw in ((0x85003000,0x232f),(0x05013000,0x632f),(0x05003002,0x232f),(0x05003000,15)):
        values,ecr,hit=expected(address,psw,0x12345678)
        require(not hit and ecr==0x12345678 and values[7:11]==(0,0,0,0),'negative oracle reference failed')


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(len(addresses)*len(statuses) for _,addresses,statuses in domain)
    report=dict(status='error',console='vb',suite='address_traps',expected_cases=total,
                observed_cases=0,graded_cases=0,expected_address_traps=0,observed_address_traps=0,mismatches=0,groups=[],examples=[],
                rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 vb address_traps\n','wrong suite')
            require(f.readline()==f'INFO full={int(full)} groups={len(domain)}\n','wrong domain')
            ecr=0xfff0
            for name,addresses,statuses in domain:
                count=len(addresses)*len(statuses)
                require(f.readline()==f'BEGIN {name} cases={count} words=14\n','missing/reordered group')
                group=dict(id=name,observed_cases=0,mismatches=0); report['groups'].append(group)
                for address in addresses:
                    for psw in statuses:
                        line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{112}\n',line),'missing/malformed observation')
                        got=tuple(int(line[n:n+8],16) for n in range(6,118,8)); want,ecr,hit=expected(address,psw,ecr)
                        report['expected_address_traps']+=int(hit); report['observed_address_traps']+=int(got[7]!=0); report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                        if got!=want:
                            report['mismatches']+=1; group['mismatches']+=1
                            if len(report['examples'])<8:
                                report['examples'].append(dict(group=name,adtre_input=f'{address:08x}',psw=f'{psw:08x}',
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
    for name,rom in (('wrong-address',build(False,wrong_address=True)),('wrong-capture',build(False,wrong_capture=True))):
        image=artifacts/(name+'.vb'); image.write_bytes(rom)
        path=artifacts/(name+'.result'); run(emulator,image,path); invalid=evaluate(path,rom,False)
        require(invalid['status']=='fail',name+' mutation accepted: '+json.dumps(invalid))
        if name=='wrong-capture': require(invalid['mismatches']==report['expected_address_traps']==160,'capture mutation not limited to trap cases')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN full_status','BEGIN single_bits',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_address_traps')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'address_traps.vb').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('VB controls: executed address/context mutants FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'address_traps.result'; run(args.emulator,HERE/'address_traps.vb',result)
    report=evaluate(result,rom); (args.artifacts/'address_traps.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB address traps: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} register snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
