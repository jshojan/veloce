#!/usr/bin/env python3
"""Independently grade observed V810 arithmetic bit-string registers from original guest code."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build, groups, cases

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]


def require(ok,message):
    if not ok: raise ValueError(message)


MASK=0xffffffff

def inputs(group):
    _,_,_,values,params=group
    for source in values:
        for destination in values:
            for source_offset,destination_offset,length in params:
                for flags in range(16): yield source,destination,source_offset,destination_offset,length,flags

def expected(op,source,destination,source_offset,destination_offset,length,flags):
    source_words=(source,source^MASK,source^0x55555555)
    destination_words=(destination,destination^MASK,destination^0xa5a5a5a5,destination^0x89abcdef)
    source_bits=sum(word<<(32*n) for n,word in enumerate(source_words))
    destination_bits=sum(word<<(32*n) for n,word in enumerate(destination_words))
    mask=(1<<length)-1; s=(source_bits>>source_offset)&mask; d=(destination_bits>>destination_offset)&mask
    value={'or':lambda:d|s,'and':lambda:d&s,'xor':lambda:d^s,'mov':lambda:s,
           'orn':lambda:d|(~s&mask),'andn':lambda:d&(~s&mask),'xorn':lambda:d^(~s&mask),'not':lambda:~s&mask}[op]()
    result=(destination_bits&~(mask<<destination_offset))|((value&mask)<<destination_offset)
    words=tuple((result>>(32*n))&MASK for n in range(4))
    return words+source_words+((destination_offset+length)%32,(source_offset+length)%32,0,
        0x05002100+((destination_offset+length)//32)*4,0x05002000+((source_offset+length)//32)*4,0x8000|flags,0xdeadbeef)

def references():
    domain=groups(); require(sum(cases(g) for g in domain)==14680064,'full case count changed')
    for g in domain[:8]: require(set(g[3])==set(n*0x01010101 for n in range(256)),'byte pair domain incomplete')
    for g in domain[8:]:
        require(len(g[4])==len(set(g[4]))==12288 and set(g[4])==set((s,d,n) for s in range(32) for d in range(32)
            for n in (0,1,7,8,15,16,31,32,33,63,64,65)),'offset/length domain incomplete')
    require(expected('mov',0x12345678,0x89abcdef,0,0,8,15)[0]==0x89abcd78,'MOV byte reference failed')
    require(expected('not',0,0,0,0,32,0)[0]==MASK,'NOT reference failed')
    require(expected('xor',MASK,MASK,31,31,2,8)[:2]==(0x7fffffff,0),'cross-word XOR reference failed')
    require(expected('mov',MASK,0,31,31,33,0)[7:12]==(0,0,0,0x05002108,0x05002008),'working register reference failed')
    for op in ('or','and','xor','mov','orn','andn','xorn','not'):
        require(expected(op,0x12345678,0x89abcdef,7,17,0,15)[:4]==(0x89abcdef,0x76543210,0x2c0e684a,0), 'zero-length reference failed')


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g) for g in domain)
    report=dict(status='error',console='vb',suite='bitstrings',expected_cases=total,
                observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],
                rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 vb bitstrings\n','wrong suite')
            require(f.readline()==f'INFO full={int(full)} groups={len(domain)}\n','wrong domain')
            for g in domain:
                name=g[0]; count=cases(g)
                require(f.readline()==f'BEGIN {name} cases={count} words=14\n','missing/reordered group')
                group=dict(id=name,observed_cases=0,mismatches=0); report['groups'].append(group)
                for a,b,so,do,length,flags in inputs(g):
                    line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{112}\n',line),'missing/malformed observation')
                    got=tuple(int(line[n:n+8],16) for n in range(6,118,8)); want=expected(g[1],a,b,so,do,length,flags)
                    report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                    if got!=want:
                        report['mismatches']+=1; group['mismatches']+=1
                        if len(report['examples'])<8:
                            report['examples'].append(dict(group=name,left=f'{a:08x}',right=f'{b:08x}',source_offset=so,destination_offset=do,length=length,flags=flags,
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
    env.update(HEADLESS='1',FRAMES='500000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=1800)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    baseline=build(False); image=artifacts/'control.vb'; image.write_bytes(baseline)
    result=artifacts/'control.result'; run(emulator,image,result)
    report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive control failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    rom=build(False,wrong_mov=True); image=artifacts/'wrong-mov.vb'; image.write_bytes(rom)
    path=artifacts/'wrong-mov.result'; run(emulator,image,path); invalid=evaluate(path,rom,False)
    require(invalid['status']=='fail' and all(g['mismatches']>0 if g['id'].startswith('mov_') else g['mismatches']==0
        for g in invalid['groups']),'executed MOV-to-XOR mutation not isolated/rejected: '+json.dumps(invalid))
    (artifacts/'wrong-mov.json').write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN or_byte','BEGIN and_byte',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_bitstrings')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'bitstrings.vb').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('VB controls: executed MOV-to-XOR mutant FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'bitstrings.result'; run(args.emulator,HERE/'bitstrings.vb',result)
    report=evaluate(result,rom); (args.artifacts/'bitstrings.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB arithmetic bit strings: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} register snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
