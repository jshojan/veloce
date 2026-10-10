#!/usr/bin/env python3
"""Independent host grading of complete original PowerPC condition-register streams."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
CONSOLE='gc'
CORE='GameCube'
EXT='vgc'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=42108928
SHORT=864
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    values=(0,4294967295) if short else (0,1,2,15,16,127,128,255,32767,32768,2147483647,2147483648,4294967294,4294967295,1431655765,2309737967)
    fields=(0,7) if short else range(8); flags=(0,7) if short else range(8)
    seeds=(0,4294967295) if short else (0,4294967295,2779096485,305419896)
    full_seeds=(0,4294967295,2779096485,305419896)
    signed=lambda x:x if x<2147483648 else x-4294967296
    def comparison(a,b,field,seed,xer):
        nibble=(8 if a<b else 4 if a>b else 2)+(xer//2147483648)
        factor=16**(7-field); return seed-(seed//factor%16)*factor+nibble*factor
    for logical in (False,True):
        for a in values:
            for b in values:
                for field in fields:
                    for flag in flags:
                        for seed in seeds:
                            xer=flag*536870912+127; ins=0x7c000000+field*8388608+3*65536+10*2048+(64 if logical else 0)
                            cr=comparison(a if logical else signed(a),b if logical else signed(b),field,seed,xer)
                            yield cr,xer,a,b,ins,seed,xer,305419896
    for logical in (False,True):
        for a in values:
            for field in fields:
                for so in range(2):
                    seed=full_seeds[(field+so)%4]; xer=so*2147483648+1610612863
                    for immediate in (range(65520,65536) if short else range(65536)):
                        b=immediate if logical or immediate<32768 else immediate-65536
                        ins=(0x28000000 if logical else 0x2c000000)+field*8388608+3*65536+immediate
                        cr=comparison(a if logical else signed(a),b,field,seed,xer)
                        yield cr,xer,a,610839776,ins,seed,xer,305419896
    for xo,truth in ((257,8),(129,4),(289,9),(225,7),(33,1),(449,14),(417,13),(193,6)):
        for seed in seeds:
            for flag in flags:
                xer=flag*536870912+127
                for encoding in range(16 if short else 32768):
                    dst=encoding//1024; srca=encoding//32%32; srcb=encoding%32
                    a=seed//(2**(31-srca))%2; b=seed//(2**(31-srcb))%2
                    bit=truth//(2**(2*a+b))%2; factor=2**(31-dst)
                    cr=seed-(seed//factor%2)*factor+bit*factor
                    yield cr,xer,324508639,610839776,0x4c000000+encoding*2048+xo*2,seed,xer,305419896
    for dst in fields:
        for src in fields:
            for seed in seeds:
                for flag in flags:
                    xer=flag*536870912+127; factor=16**(7-dst); nibble=seed//(16**(7-src))%16
                    cr=seed-(seed//factor%16)*factor+nibble*factor
                    yield cr,xer,324508639,610839776,0x4c000000+dst*8388608+src*262144,seed,xer,305419896
    for dst in fields:
        for flag in flags:
            for byte_count in ((0,127) if short else range(128)):
                for seed in seeds:
                    xer=flag*536870912+byte_count; factor=16**(7-dst)
                    cr=seed-(seed//factor%16)*factor+(flag*2)*factor
                    yield cr,byte_count,324508639,610839776,0x7c000400+dst*8388608,seed,xer,305419896

def evaluate(path,rom,short=False):
    cases=SHORT if short else COUNT
    report=dict(status='error',console=CONSOLE,suite='condition_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    started=time.monotonic(); row_pattern=re.compile(r'REG r=[0-9a-f]{64}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} condition_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN conditions cases={cases} words=8\n','wrong domain')
            for index,want in enumerate(expected(short)):
                row=stream.readline(); require(row_pattern.fullmatch(row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,70,8))
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
    parser=argparse.ArgumentParser(); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_conditions')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True); require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',864),('wrong-signed','wrong_signed',68),('wrong-clear','wrong_clear',8)):
        mutant,_=grade(name,build(short=True,**{option:True}),True)
        require(mutant['status']=='fail' and mutant['mismatches']==failures,'executed mutation escaped: '+json.dumps(mutant))
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(8):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=864 words=8','cases=863 words=8',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    rom=build(); require((HERE/('condition_registers.'+EXT)).read_bytes()==rom,'archive differs')
    print(f'{CORE}: running original {COUNT}-case condition corpus',flush=True)
    report,_=grade('condition_registers',rom)
    print(f"{CORE} conditions: {report['status'].upper()} {report['graded_cases']}/{COUNT} eight-word observations; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
