#!/usr/bin/env python3
"""Independent host grading of complete original PowerPC word rotate/mask streams."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
CONSOLE='gc'
CORE='GameCube'
EXT='vgc'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=9437184
SHORT=3584
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    sources=(2147483649,2309737967) if short else (0,1,2147483647,2147483648,4294967295,1431655765,2863311530,2309737967)
    destinations=(0,4294967295,305419896,2779096485)
    seeds=(0,4294967295,2779096485,305419896)
    # Masks are circular walks; rotations use arithmetic division/multiplication.
    masks=[]
    for begin in range(32):
        for end in range(32):
            mask=0; bit=begin
            while True:
                mask+=2**(31-bit)
                if bit==end: break
                bit=(bit+1)%32
            masks.append(mask)
    def rows(op,ai,di,a,b,d,so,rc,encoding_start,length):
        seed=seeds[(ai+di+so+rc)%4]; xer=so*2147483648+1610612863
        for encoded in range(encoding_start,encoding_start+length):
            shift=b%32 if op==23 else encoded//1024
            factor=2**shift; rotation=(a*factor)%4294967296+a//(4294967296//factor)
            mask=masks[encoded%1024]; selected=rotation&mask
            result=selected+(d-(d&mask) if op==20 else 0)
            nibble=(2 if result==0 else 8 if result>=2147483648 else 4)+so
            cr=seed%268435456+nibble*268435456 if rc else seed
            ins=op*67108864+3*2097152+11*65536+(10*2048 if op==23 else 0)+encoded*2+rc
            yield result,cr,xer,a,b,d,ins,seed,xer,305419896
    for op in (20,21):
        for ai,a in enumerate(sources):
            for di,d in enumerate((destinations[:2] if short else destinations) if op==20 else destinations[:1]):
                for so in range(2):
                    for rc in range(2):
                        yield from rows(op,ai,di,a,610839776,d,so,rc,31*1024 if short else 0,64 if short else 32768)
    for ai,a in enumerate(sources):
        for shift in ((0,31) if short else range(32)):
            for high in ((0,32) if short else (0,32,2147483648,4294967264)):
                for so in range(2):
                    for rc in range(2):
                        yield from rows(23,ai,0,a,high+shift,0,so,rc,0,64 if short else 1024)

def evaluate(path,rom,short=False):
    cases=SHORT if short else COUNT
    report=dict(status='error',console=CONSOLE,suite='rotate_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    started=time.monotonic(); row_pattern=re.compile(r'REG r=[0-9a-f]{80}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} rotate_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN rotates cases={cases} words=10\n','wrong domain')
            for index,want in enumerate(expected(short)):
                row=stream.readline(); require(row_pattern.fullmatch(row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,86,8))
                report['observed_cases']+=1; report['graded_cases']+=1
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8: report['examples'].append(dict(index=index,expected=list(want),observed=list(got)))
                if not short and (index+1)%4000000==0:
                    print(f'{CORE}: graded {index+1}/{cases}; mismatches={report["mismatches"]}',flush=True)
            require(report['graded_cases']==cases,'oracle domain incomplete')
            require(stream.readline()==f'LOG complete cases={cases}\n','missing completion')
            require(stream.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',stream.readline()),'missing/ungraded trailer')
            require(not stream.read(),'trailing data'); report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as error: report['detail']=str(error)
    report['grading_seconds']=time.monotonic()-started; return report

def run(emulator,image,path):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'): env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='30000',VELOCE_TEST_OUT=str(path.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=1200)
    require(proc.returncode==0,proc.stdout+proc.stderr)
def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--controls-only',action='store_true'); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_rotates')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True); require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',3584),('wrong-mask','wrong_mask',1472),('wrong-insert','wrong_insert',496)):
        mutant,_=grade(name,build(short=True,**{option:True}),True)
        require(mutant['status']=='fail' and mutant['mismatches']==failures,'executed mutation escaped: '+json.dumps(mutant))
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(10):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=3584 words=10','cases=3583 words=10',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    if args.controls_only:
        print(f'{CORE}: executed controls and strict stream rejection passed',flush=True); return 0
    rom=build(); require((HERE/('rotate_registers.'+EXT)).read_bytes()==rom,'archive differs')
    print(f'{CORE}: running original {COUNT}-case rotate corpus',flush=True)
    report,_=grade('rotate_registers',rom)
    print(f"{CORE} rotates: {report['status'].upper()} {report['graded_cases']}/{COUNT} ten-word observations; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
