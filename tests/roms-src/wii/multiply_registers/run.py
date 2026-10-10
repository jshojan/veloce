#!/usr/bin/env python3
"""Independent host grading of complete original PowerPC multiply/divide streams."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
CONSOLE='wii'
CORE='Wii'
EXT='vwi'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=8421376
SHORT=384
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    values=(0,4294967295,2147483648) if short else (0,1,2,15,16,127,128,255,32767,32768,2147483647,2147483648,4294967294,4294967295,1431655765,2309737967)
    seeds=(0,4294967295,2779096485,305419896)
    flags=(0,7) if short else range(8)
    signed=lambda a:a if a<2147483648 else a-4294967296
    def row(ins,a,b,seed,xer,result,record,oe=False,overflow=False,defined=True):
        wantxer=xer
        if oe:
            wantxer=wantxer-(wantxer//1073741824%2)*1073741824+int(overflow)*1073741824
            if overflow and wantxer<2147483648: wantxer+=2147483648
        result%=4294967296
        nibble=(2 if result==0 else 8 if result>=2147483648 else 4)+wantxer//2147483648
        cr=seed%268435456+nibble*268435456 if record else seed
        wanted=(result,cr,wantxer,a,b,ins,seed,xer,305419896)
        masks=(4294967295 if defined else 0,536870911 if record and not defined else 4294967295)+(4294967295,)*7
        return wanted,masks,defined
    for xo in (75,11,235,491,459):
        for ai,a in enumerate(values):
            for bi,b in enumerate(values):
                defined=not (xo in (491,459) and (b==0 or (xo==491 and a==2147483648 and b==4294967295)))
                product=signed(a)*signed(b); overflow=False
                if xo==75: result=product//4294967296
                elif xo==11: result=(a*b)//4294967296
                elif xo==235: result=product; overflow=not -2147483648<=product<=2147483647
                elif not defined: result=0; overflow=True
                elif xo==459: result=a//b
                else:
                    result=abs(signed(a))//abs(signed(b))
                    if (signed(a)<0)!=(signed(b)<0): result=-result
                for flag in flags:
                    xer=flag*536870912+127
                    for oe in range(1 if xo in (75,11) else 2):
                        for rc in range(2):
                            ins=0x7c000000+11*2097152+3*65536+10*2048+oe*1024+xo*2+rc
                            yield row(ins,a,b,seeds[(ai+bi+flag+oe+rc)%4],xer,result,rc,oe,overflow,defined)
    for ai,a in enumerate(values):
        for flag in flags:
            xer=flag*536870912+127; seed=seeds[(ai+flag)%4]
            for encoded in (range(65520,65536) if short else range(65536)):
                imm=encoded if encoded<32768 else encoded-65536
                ins=7*67108864+11*2097152+3*65536+encoded
                yield row(ins,a,610839776,seed,xer,signed(a)*imm,False)

def evaluate(path,rom,short=False):
    cases=SHORT if short else COUNT
    report=dict(status='error',console=CONSOLE,suite='multiply_registers',expected_cases=cases,observed_cases=0,graded_cases=0,defined_result_cases=0,undefined_result_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    started=time.monotonic(); row_pattern=re.compile(r'REG r=[0-9a-f]{72}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} multiply_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN products cases={cases} words=9\n','wrong domain')
            for index,(want,masks,defined) in enumerate(expected(short)):
                row=stream.readline(); require(row_pattern.fullmatch(row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,78,8))
                report['observed_cases']+=1; report['graded_cases']+=1
                report['defined_result_cases' if defined else 'undefined_result_cases']+=1
                if any((actual^desired)&mask for actual,desired,mask in zip(got,want,masks)):
                    report['mismatches']+=1
                    if len(report['examples'])<8: report['examples'].append(dict(index=index,expected=list(want),observed=list(got),masks=list(masks),result_defined=defined))
                if not short and (index+1)%4000000==0:
                    print(f'{CORE}: graded {index+1}/{cases}; mismatches={report["mismatches"]}',flush=True)
            require(report['graded_cases']==cases,'oracle domain incomplete')
            require(report['undefined_result_cases']==(56 if short else 1056),'undefined division domain incomplete')
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
    parser=argparse.ArgumentParser(); parser.add_argument('--controls-only',action='store_true'); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_products')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True); require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',328),('wrong-high','wrong_high',32),('wrong-flags','wrong_flags',54)):
        mutant,_=grade(name,build(short=True,**{option:True}),True)
        require(mutant['status']=='fail' and mutant['mismatches']==failures,'executed mutation escaped: '+json.dumps(mutant))
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(9):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=384 words=9','cases=383 words=9',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    # Undefined quotient and CR0 LT/GT/EQ changes must remain outside the hardware score.
    rows=content.splitlines(keepends=True); changed=False; first_undefined=None; first_recorded_undefined=None
    for index,(_,masks,defined) in enumerate(expected(True)):
        if not defined:
            line=rows[index+3]; words=[int(line[n:n+8],16) for n in range(6,78,8)]
            words[0]=0xdec0adde
            if masks[1]!=0xffffffff:
                words[1]^=0xe0000000
                if first_recorded_undefined is None: first_recorded_undefined=index+3
            rows[index+3]='REG r='+''.join(f'{word:08x}' for word in words)+'\n'
            if first_undefined is None: first_undefined=index+3
            changed=True
    require(changed,'no undefined control cases')
    masked=''.join(rows); report,_=grade('undefined-results',baseline,True,False,masked)
    require(report['status']=='pass' and report['undefined_result_cases']==56,'undefined fields entered score')
    saved=rows[first_recorded_undefined]
    words=[int(saved[n:n+8],16) for n in range(6,78,8)]; words[1]^=0x10000000
    rows[first_recorded_undefined]='REG r='+''.join(f'{word:08x}' for word in words)+'\n'
    report,_=grade('undefined-defined-so',baseline,True,False,''.join(rows)); require(report['status']=='fail' and report['mismatches']==1,'defined CR0.SO escaped on undefined result')
    rows[first_recorded_undefined]=saved
    line=rows[first_undefined]; words=[int(line[n:n+8],16) for n in range(6,78,8)]; words[2]^=0x40000000
    rows[first_undefined]='REG r='+''.join(f'{word:08x}' for word in words)+'\n'
    report,_=grade('undefined-defined-flags',baseline,True,False,''.join(rows)); require(report['status']=='fail' and report['mismatches']==1,'defined flags escaped on undefined result')
    if args.controls_only:
        print(f'{CORE}: executed controls and strict stream rejection passed',flush=True); return 0
    rom=build(); require((HERE/('multiply_registers.'+EXT)).read_bytes()==rom,'archive differs')
    print(f'{CORE}: running original {COUNT}-case multiply/divide corpus',flush=True)
    report,_=grade('multiply_registers',rom)
    print(f"{CORE} products: {report['status'].upper()} {report['graded_cases']}/{COUNT} nine-word observations; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
