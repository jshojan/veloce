#!/usr/bin/env python3
"""Independent host grading of complete original PowerPC integer logical streams."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
CONSOLE='gc'
CORE='GameCube'
EXT='vgc'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=6324224
SHORT=512
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    # Numeric truth tables operate on four independent byte pairs. No CPU imports.
    tables={}
    for truth in (8,4,14,13,6,7,1,9):
        table=bytearray(65536)
        for a in range(256):
            for b in range(256):
                table[a*256+b]=((~a&~b if truth&1 else 0)|(~a&b if truth&2 else 0)|(a&~b if truth&4 else 0)|(a&b if truth&8 else 0))&255
        tables[truth]=table
    def result(truth,a,b):
        table=tables[truth]
        return (table[(a>>24)*256+(b>>24)]<<24 | table[((a>>16)&255)*256+((b>>16)&255)]<<16 |
                table[((a>>8)&255)*256+((b>>8)&255)]<<8 | table[(a&255)*256+(b&255)])
    def record(r,seed,xer):
        nibble=(2 if r==0 else 8 if r>=2147483648 else 4)+xer//2147483648
        return seed%268435456+nibble*268435456
    values=(0,4294967295) if short else (0,1,2,15,16,127,128,255,32767,32768,2147483647,2147483648,4294967294,4294967295,1431655765,2309737967)
    seeds=(0,4294967295,2779096485,305419896)
    for xo,truth in ((28,8),(60,4),(444,14),(412,13),(316,6),(476,7),(124,1),(284,9)):
        for ai,a in enumerate(values):
            for bi,b in enumerate(values):
                seed=seeds[(ai+bi)%4]; r=result(truth,a,b)
                for flag in ((0,7) if short else range(8)):
                    xer=flag*536870912+127
                    for rc in range(2):
                        ins=0x7c000000+3*2097152+11*65536+10*2048+xo*2+rc
                        yield r,record(r,seed,xer) if rc else seed,xer,a,b,ins,seed,xer,305419896
    immediate_values=(0,4294967295) if short else (0,1,2147483647,2147483648,4294967295,1431655765,2863311530,2309737967)
    for op in range(24,30):
        for a in immediate_values:
            for so in range(2):
                seed=seeds[so+2]; xer=so*2147483648+1610612863
                for imm in (range(65520,65536) if short else range(65536)):
                    b=imm*(65536 if op%2 else 1); r=result(14 if op<26 else 6 if op<28 else 8,a,b)
                    ins=op*67108864+3*2097152+11*65536+imm
                    yield r,record(r,seed,xer) if op>=28 else seed,xer,a,610839776,ins,seed,xer,305419896

def evaluate(path,rom,short=False):
    cases=SHORT if short else COUNT
    report=dict(status='error',console=CONSOLE,suite='logical_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    started=time.monotonic(); row_pattern=re.compile(r'REG r=[0-9a-f]{72}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} logical_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN logical cases={cases} words=9\n','wrong domain')
            for index,want in enumerate(expected(short)):
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
    parser=argparse.ArgumentParser(); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_logical')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True); require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',512),('wrong-cr','wrong_cr',192),('wrong-xer','wrong_xer',512)):
        mutant,_=grade(name,build(short=True,**{option:True}),True)
        require(mutant['status']=='fail' and mutant['mismatches']==failures,'executed mutation escaped: '+json.dumps(mutant))
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(9):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=512 words=9','cases=511 words=9',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    rom=build(); require((HERE/('logical_registers.'+EXT)).read_bytes()==rom,'archive differs')
    print(f'{CORE}: running original {COUNT}-case logical corpus',flush=True)
    report,_=grade('logical_registers',rom)
    print(f"{CORE} logical: {report['status'].upper()} {report['graded_cases']}/{COUNT} nine-word observations; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
