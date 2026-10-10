#!/usr/bin/env python3
"""Independently grade observed V810 CAXI/port transaction registers from original guest code."""
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

def signed(value): return value if value<0x80000000 else value-0x100000000

def inputs(group):
    name,left,right,third=group
    for a in left:
        for b in right if name.startswith('caxi') else (0,):
            for c in third if name.startswith('caxi') else (0,):
                for lane in range(4):
                    for flags in range(16): yield a,b,c,lane,flags

def expected(name,a,b,c,lane,flags):
    address=0x05002000+lane
    if name.startswith('caxi'):
        compare=a if name=='caxi_replace' else b
        replacement=b if name=='caxi_replace' else c
        result=(compare-a)&MASK; mathematical=signed(compare)-signed(a)
        psw=0x8000|int(result==0)|int(result>=0x80000000)*2|int(not -0x80000000<=mathematical<0x80000000)*4|int(compare<a)*8
        memory=replacement if compare==a else a
        return (a,psw,memory,0xdeadbeef,0x76543210,replacement,address,a,b)
    width={'port_byte':1,'port_half':2,'port_word':4}[name]
    offset=lane//width*width
    memory=bytearray(bytes.fromhex('efcdab89')); output=a%(1<<(width*8))
    memory[offset:offset+width]=output.to_bytes(width,'little')
    value=int.from_bytes(memory[offset:offset+width],'little'); word=int.from_bytes(memory,'little')
    return (value,0x8000|flags,0x8000|flags,word,0xdeadbeef,0x76543210,a,address,a)

def references():
    domain=groups()
    require(sum(cases(g) for g in domain)==17055744,'full case count changed')
    require(set(domain[0][1])==set(n*0x01010101 for n in range(256)),'byte domain incomplete')
    for g in domain[4:]:
        require(len(g[1])==len(set(g[1]))==65536 and all(v%65536==v//65536 for v in g[1]),'halfword domain incomplete')
    require(expected('caxi_compare',0x80000000,0,123,3,15)[:3]==(0x80000000,0x800e,0x80000000),'borrow/overflow oracle reference failed')
    require(expected('caxi_compare',0x7fffffff,0x80000000,123,0,0)[:3]==(0x7fffffff,0x8004,0x7fffffff),'signed overflow oracle reference failed')
    require(expected('caxi_replace',0xffffffff,0x13579bdf,0,0,15)[:3]==(0xffffffff,0x8001,0x13579bdf),'replacement oracle reference failed')
    require(expected('port_half',0x80008000,0,0,3,15)[:4]==(0x8000,0x800f,0x800f,0x8000cdef),'unsigned half oracle reference failed')
    require(expected('port_byte',0xaaaaaaaa,0,0,1,0)[:4]==(0xaa,0x8000,0x8000,0x89abaaef),'byte lane oracle reference failed')


def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g) for g in domain)
    report=dict(status='error',console='vb',suite='memory_ops',expected_cases=total,
                observed_cases=0,graded_cases=0,mismatches=0,groups=[],examples=[],
                rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=VB rom_crc32={zlib.crc32(rom):08x} channels=v810debug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 vb memory_ops\n','wrong suite')
            require(f.readline()==f'INFO full={int(full)} groups={len(domain)}\n','wrong domain')
            for g in domain:
                name=g[0]; count=cases(g)
                require(f.readline()==f'BEGIN {name} cases={count} words=9\n','missing/reordered group')
                group=dict(id=name,observed_cases=0,mismatches=0); report['groups'].append(group)
                for a,b,c,lane,flags in inputs(g):
                    line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{72}\n',line),'missing/malformed observation')
                    got=tuple(int(line[n:n+8],16) for n in range(6,78,8)); want=expected(name,a,b,c,lane,flags)
                    report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                    if got!=want:
                        report['mismatches']+=1; group['mismatches']+=1
                        if len(report['examples'])<8:
                            report['examples'].append(dict(group=name,left=f'{a:08x}',right=f'{b:08x}',third=f'{c:08x}',lane=lane,flags=flags,
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
    env.update(HEADLESS='1',FRAMES='300000',VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=1200)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def controls(emulator,artifacts):
    baseline=build(False); image=artifacts/'control.vb'; image.write_bytes(baseline)
    result=artifacts/'control.result'; run(emulator,image,result)
    report=evaluate(result,baseline,False)
    require(report['status']=='pass','reduced positive control failed: '+json.dumps(report))
    (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,rom in (('wrong-caxi',build(False,wrong_caxi=True)),('wrong-port',build(False,wrong_port=True))):
        image=artifacts/(name+'.vb'); image.write_bytes(rom)
        path=artifacts/(name+'.result'); run(emulator,image,path); invalid=evaluate(path,rom,False)
        require(invalid['status']=='fail',name+' mutation accepted: '+json.dumps(invalid))
        require(all(g['mismatches']>0 if (g['id'].startswith('caxi') if name=='wrong-caxi' else g['id']=='port_half') else g['mismatches']==0 for g in invalid['groups']),'mutation affected unexpected groups')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN caxi_compare','BEGIN caxi_replace',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/vb_memory_ops')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'memory_ops.vb').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('VB controls: executed CAXI/port mutants FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'memory_ops.result'; run(args.emulator,HERE/'memory_ops.vb',result)
    report=evaluate(result,rom); (args.artifacts/'memory_ops.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"VB memory operations: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} register snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
