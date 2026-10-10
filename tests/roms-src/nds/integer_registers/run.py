#!/usr/bin/env python3
"""Independently grade observed dual-CPU DS ARM result/CPSR/source registers."""
import argparse
import hashlib
import json
from itertools import zip_longest
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


def expected(op,left,right,flags,cpu=7):
    q=bool(flags&1) if cpu==9 else False
    if cpu==9: flags>>=1
    carry=bool(flags&2); overflow=bool(flags&1)
    if op in ('sub','rsb','add','adc','sbc','rsc','cmp','cmn'):
        reverse=op in ('rsb','rsc'); a,b=(right,left) if reverse else (left,right)
        subtract=op in ('sub','rsb','sbc','rsc','cmp')
        extra=(0 if carry else 1) if op in ('sbc','rsc') else int(carry) if op=='adc' else 0
        wide=a-b-extra if subtract else a+b+extra
        mathematical=signed(a)-signed(b)-extra if subtract else signed(a)+signed(b)+extra
        result=wide&MASK; carry=wide>=0 if subtract else wide>MASK
        overflow=not -(1<<31)<=mathematical<(1<<31)
    else:
        if op in ('and','tst'): result=left&right
        elif op in ('eor','teq'): result=left^right
        elif op=='orr': result=left|right
        elif op=='mov': result=right
        elif op=='bic': result=left&(~right&MASK)
        else: result=~right&MASK
    psw=0xdf|(0x08000000 if q else 0)|(int(result>=(1<<31))<<31)|(int(result==0)<<30)|(int(carry)<<29)|(int(overflow)<<28)
    return (0xdeadbeef if op in ('tst','teq','cmp','cmn') else result,psw,left,right)


def references():
    domain=groups(); require(sum(cases(g,cpu) for g in domain for cpu in (9,7))==44238336,'declared count changed')
    for g in domain:
        values=PATTERNS if g[0].endswith('_boundary') else tuple(n*0x01010101 for n in range(256))
        require(g[4]==values and g[3]==((0,) if g[1] in ('mov','mvn') else values),'operand domain changed')
    for op,a,b,f,want in (
        ('add',0x7fffffff,1,0,(0x80000000,0x900000df,0x7fffffff,1)),
        ('adc',MASK,0,2,(0,0x600000df,MASK,0)),
        ('sub',0,1,0,(MASK,0x800000df,0,1)),
        ('rsb',1,0,0,(MASK,0x800000df,1,0)),
        ('sbc',0,0,0,(MASK,0x800000df,0,0)),
        ('rsc',0,0,2,(0,0x600000df,0,0)),
        ('cmp',0x80000000,1,0,(0xdeadbeef,0x300000df,0x80000000,1)),
        ('cmn',MASK,1,0,(0xdeadbeef,0x600000df,MASK,1)),
        ('tst',MASK,0,3,(0xdeadbeef,0x700000df,MASK,0)),
        ('mov',0,0,3,(0,0x700000df,0,0)),
        ('mvn',0,0,3,(MASK,0xb00000df,0,0)),
    ): require(expected(op,a,b,f)==want,'oracle reference failed: '+op)


    for q in (0,1):
        require(expected('add',0x7fffffff,1,q,9)==(0x80000000,0x900000df|(q<<27),0x7fffffff,1),'ARM9 Q reference failed')

def evaluate(path,rom,full=True):
    domain=groups(full); total=sum(cases(g,cpu) for g in domain for cpu in (9,7))
    report=dict(status='error',console='nds',suite='integer_registers',expected_cases=total,observed_cases=0,
        graded_cases=0,mismatches=0,groups=[],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            for cpu in (9,7):
                require(f.readline()==f'VELOCE 1 ds integer_registers cpu={cpu}\n','wrong CPU/suite order')
                require(f.readline()==f'INFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n','wrong domain')
                for name,op,_,left,right in domain:
                    require(f.readline()==f'BEGIN cpu{cpu}_{name} cases={cases((name,op,0,left,right),cpu)} words=4\n','missing/reordered group')
                    group=dict(id=f'cpu{cpu}_{name}',observed_cases=0,mismatches=0); report['groups'].append(group)
                    for a in left:
                        for b in right:
                            for flags in range(32 if cpu==9 else 16):
                                line=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{32}\n',line),'missing/malformed observation')
                                got=tuple(int(line[n:n+8],16) for n in (6,14,22,30)); want=expected(op,a,b,flags,cpu)
                                report['observed_cases']+=1; report['graded_cases']+=1; group['observed_cases']+=1
                                if got!=want:
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
    rom=build(False,word_output=False); image=artifacts/'legacy-byte.nds'; image.write_bytes(rom)
    path=artifacts/'legacy-byte.result'; run(emulator,image,path); legacy=evaluate(path,rom,False)
    require(legacy['status']=='pass','legacy byte transport failed')
    with result.open() as native, path.open() as byte:
        native_rows=(line for line in native if line.startswith('REG r='))
        byte_rows=(line for line in byte if line.startswith('REG r='))
        count=0
        for a,b in zip_longest(native_rows,byte_rows):
            require(a==b,'word and legacy-byte register observations differ'); count+=1
    require(count==report['graded_cases']==legacy['graded_cases'],'transport observation count differs')
    legacy['identical_word_transport_observations']=count
    (artifacts/'legacy-byte.json').write_text(json.dumps(legacy,indent=2)+'\n')
    rom=build(False,wrong_adc=True); image=artifacts/'wrong-adc.nds'; image.write_bytes(rom)
    path=artifacts/'wrong-adc.result'; run(emulator,image,path); invalid=evaluate(path,rom,False)
    require(invalid['status']=='fail' and all(g['mismatches']>0 if '_adc_' in g['id'] else g['mismatches']==0 for g in invalid['groups']),'ADC-to-ADD mutation not isolated/rejected: '+json.dumps(invalid))
    (artifacts/'wrong-adc.json').write_text(json.dumps(invalid,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('reordered-group',content.replace('BEGIN cpu9_and','BEGIN cpu7_and',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),
        ('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        path=artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,baseline,False); require(invalid['status']=='error',name+' accepted')
        (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce')
    parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_integer')
    parser.add_argument('--controls-only',action='store_true')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True); references()
    rom=build(); require((HERE/'integer_registers.nds').read_bytes()==rom,'archive differs')
    controls(args.emulator,args.artifacts)
    if args.controls_only: print('DS controls: word/legacy-byte observations identical; executed mutant FAIL; 7 damaged files ERROR'); return 0
    result=args.artifacts/'integer_registers.result'; run(args.emulator,HERE/'integer_registers.nds',result)
    report=evaluate(result,rom); (args.artifacts/'integer_registers.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"DS dual-CPU integer registers: {report['status'].upper()} {report['graded_cases']}/{report['expected_cases']} result/PSW/source snapshots; mismatches={report['mismatches']}")
    if report.get('detail'): print(report['detail'])
    for example in report['examples']: print(json.dumps(example))
    return 0 if report['status']=='pass' else 1

if __name__=='__main__': sys.exit(main())
