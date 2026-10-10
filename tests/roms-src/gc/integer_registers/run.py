#!/usr/bin/env python3
"""Independent mathematical host grading of original PowerPC register streams."""
import argparse, hashlib, json, os, re, subprocess, sys, zlib
from pathlib import Path
from build import build
CONSOLE="gc"
CORE="GameCube"
EXT="vgc"
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    values=(0,1) if short else (0,1,2,15,16,127,128,255,32767,32768,2147483647,2147483648,4294967294,4294967295,1431655765,2309737967)
    signed=lambda x:x if x<2147483648 else x-4294967296
    for operation in ('add','addc','adde','subf','subfc','subfe'):
        for a in values:
            for b in values:
                for flags in range(8):
                    for oe in range(2):
                        for rc in range(2):
                            ca=flags&1; borrow=1-ca if operation=='subfe' else 0
                            extra=ca if operation=='adde' else 0
                            subtract=operation.startswith('sub')
                            wide=b-a-borrow if subtract else a+b+extra
                            math=signed(b)-signed(a)-borrow if subtract else signed(a)+signed(b)+extra
                            result=wide%4294967296
                            carry=ca if operation in ('add','subf') else int(wide>=0 if subtract else wide>=4294967296)
                            ov=(flags>>1)&1; so=(flags>>2)&1
                            if oe:
                                ov=int(not -2147483648<=math<=2147483647); so=so|ov
                            xer=(so<<31)|(ov<<30)|(carry<<29)|127
                            cr=0xa5a5a5a5
                            if rc: cr=(cr&0x0fffffff)|((2 if result==0 else 8 if result>=2147483648 else 4)|so)<<28
                            yield result,cr,xer,a,b,0x12345678
def evaluate(path,rom,short=False):
    cases=768 if short else 49152
    report=dict(status='error',console=CONSOLE,suite='integer_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} integer_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN arithmetic cases={cases} words=6\n','wrong domain')
            for index,want in enumerate(expected(short)):
                row=stream.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{48}\n',row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,54,8))
                report['observed_cases']+=1; report['graded_cases']+=1
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8: report['examples'].append(dict(index=index,expected=list(want),observed=list(got)))
            require(stream.readline()==f'LOG complete cases={cases}\n','missing completion')
            require(stream.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',stream.readline()),'missing/ungraded trailer')
            require(not stream.read(),'trailing data'); report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as error: report['detail']=str(error)
    return report
def run(emulator,image,path):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'): env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='200',VELOCE_TEST_OUT=str(path.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=90)
    require(proc.returncode==0,proc.stdout+proc.stderr)
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_integer')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True)
    require(positive['status']=='pass',json.dumps(positive))
    mutant,_=grade('wrong-result',build(short=True,wrong_result=True),True)
    require(mutant['status']=='fail' and mutant['mismatches']==768,'executed mutation escaped')
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(6):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=768 words=6','cases=767 words=6',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    rom=build(); require((HERE/('integer_registers.'+EXT)).read_bytes()==rom,'archive differs')
    report,_=grade('integer_registers',rom)
    print(f"{CORE}: {report['status'].upper()} {report['graded_cases']}/49152 six-word observations; mismatches={report['mismatches']}")
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
