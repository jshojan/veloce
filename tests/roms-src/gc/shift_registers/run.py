#!/usr/bin/env python3
"""Independent host grading of complete original PowerPC word shift/unary streams."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
CONSOLE='gc'
CORE='GameCube'
EXT='vgc'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=4416528
SHORT=3856
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False,register_only=False):
    values=(2147483649,4294967295) if short else (0,1,2,15,16,127,128,255,32767,32768,2147483647,2147483648,4294967294,4294967295,1431655765,2309737967)
    seeds=(0,4294967295,2779096485,305419896)
    flags=(0,7) if short else range(8)
    def row(xo,a,b,seed,xer,rc,shift_field=0):
        wantxer=xer
        if xo in (954,922):
            modulus=256 if xo==954 else 65536; low=a%modulus
            result=(low-(modulus if low>=modulus//2 else 0))%4294967296
        elif xo==26: result=32-a.bit_length()
        else:
            shift=shift_field if xo==824 else b%64; divisor=2**shift
            if xo==24: result=(a*divisor)%4294967296
            elif xo==536: result=a//divisor
            else:
                signed=a if a<2147483648 else a-4294967296
                result=(signed//divisor)%4294967296
                carry=int(signed<0 and signed%divisor!=0)
                wantxer=xer-(xer//536870912%2)*536870912+carry*536870912
        nibble=(2 if result==0 else 8 if result>=2147483648 else 4)+xer//2147483648
        cr=seed%268435456+nibble*268435456 if rc else seed
        field=10 if xo in (24,536,792) else shift_field if xo==824 else 0
        ins=0x7c000000+3*2097152+11*65536+field*2048+xo*2+rc
        return result,cr,wantxer,a,b,ins,seed,xer,305419896
    for xo in (24,536,792):
        for ai,a in enumerate(values):
            for hi,h in enumerate((0,64) if short else (0,64,2147483648,4294967232)):
                for flag in flags:
                    for rc in range(2):
                        seed=seeds[(ai+hi+flag+rc)%4]; xer=flag*536870912+127
                        for shift in range(64): yield row(xo,a,h+shift,seed,xer,rc)
    if register_only: return
    for ai,a in enumerate(values):
        for flag in flags:
            for rc in range(2):
                seed=seeds[(ai+flag+rc)%4]; xer=flag*536870912+127
                for shift in range(32): yield row(824,a,610839776,seed,xer,rc,shift)
    for xo,uppers in ((954,(0,305419776,2147483648,4294967040)),(922,(0,305397760,2147483648,4294901760))):
        for hi,h in enumerate(uppers[:2] if short else uppers):
            for flag in flags:
                for rc in range(2):
                    seed=seeds[(hi+flag+rc)%4]; xer=flag*536870912+127
                    start=(112 if xo==954 else 32752) if short else 0; length=32 if short else (256 if xo==954 else 65536)
                    for low in range(start,start+length): yield row(xo,h+low,610839776,seed,xer,rc)
    patterns=(0,1,2147483648,4294967295) if short else (0,)+tuple(2**bit for bit in range(32))+tuple(2**(bit+1)-1 for bit in range(32))
    for index,a in enumerate(patterns):
        for flag in flags:
            for rc in range(2): yield row(26,a,610839776,seeds[(index+flag+rc)%4],flag*536870912+127,rc)

def evaluate(path,rom,short=False,register_only=False):
    cases=3072 if register_only else SHORT if short else COUNT
    report=dict(status='error',console=CONSOLE,suite='shift_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    started=time.monotonic(); row_pattern=re.compile(r'REG r=[0-9a-f]{72}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} shift_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN shifts cases={cases} words=9\n','wrong domain')
            for index,want in enumerate(expected(short,register_only)):
                row=stream.readline(); require(row_pattern.fullmatch(row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,78,8))
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
    parser=argparse.ArgumentParser(); parser.add_argument('--controls-only',action='store_true'); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_shifts')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None,register_only=False):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short,register_only); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True); require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',3856),('wrong-carry','wrong_carry',640),('wrong-count','wrong_count',1280)):
        mutant,_=grade(name,build(short=True,**{option:True}),True,register_only=option=='wrong_count')
        require(mutant['status']=='fail' and mutant['mismatches']==failures,'executed mutation escaped: '+json.dumps(mutant))
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(9):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=3856 words=9','cases=3855 words=9',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    if args.controls_only:
        print(f'{CORE}: executed controls and strict stream rejection passed',flush=True); return 0
    rom=build(); require((HERE/('shift_registers.'+EXT)).read_bytes()==rom,'archive differs')
    print(f'{CORE}: running original {COUNT}-case shift corpus',flush=True)
    report,_=grade('shift_registers',rom)
    print(f"{CORE} shifts: {report['status'].upper()} {report['graded_cases']}/{COUNT} nine-word observations; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
