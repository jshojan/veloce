#!/usr/bin/env python3
"""Independent byte-array oracle for complete original PowerPC memory captures."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
CONSOLE='gc'
CORE='GameCube'
EXT='vgc'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=12586368
SHORT=1968
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    patterns=(0,1,2147450880,2147549183,4294967295,2309737967,1431677610,305419896)
    if short: patterns=(0,1,4294967295,2309737967)
    # Separate instruction metadata; no builder stimulus iterator or CPU code imported.
    forms=((32,4,0,1,0,0,0),(33,4,0,1,1,0,0),(34,1,0,1,0,0,0),(35,1,0,1,1,0,0),
           (36,4,0,0,0,0,0),(37,4,0,0,1,0,0),(38,1,0,0,0,0,0),(39,1,0,0,1,0,0),
           (40,2,0,1,0,0,0),(41,2,0,1,1,0,0),(42,2,0,1,0,1,0),(43,2,0,1,1,1,0),
           (44,2,0,0,0,0,0),(45,2,0,0,1,0,0),
           (23,4,1,1,0,0,0),(55,4,1,1,1,0,0),(87,1,1,1,0,0,0),(119,1,1,1,1,0,0),
           (151,4,1,0,0,0,0),(183,4,1,0,1,0,0),(215,1,1,0,0,0,0),(247,1,1,0,1,0,0),
           (279,2,1,1,0,0,0),(311,2,1,1,1,0,0),(343,2,1,1,0,1,0),(375,2,1,1,1,1,0),
           (407,2,1,0,0,0,0),(439,2,1,0,1,0,0),
           (534,4,1,1,0,0,1),(790,2,1,1,0,0,1),(662,4,1,0,0,0,1),(918,2,1,0,0,0,1))
    seeds=(0,4294967295,2779096485,305419896)
    for fi,(code,width,indexed,load,update,sign,reverse) in enumerate(forms):
        for pi,source in enumerate(patterns):
            memory=((source*256)%4294967296+source//16777216)^2309737967
            for bank,alias in enumerate((0,2147483648,3221225472)):
                indexes=(0,-4,4,-32768,32764,-2147483648,-width,2147483644) if indexed else (-32768,)
                if short: indexes=(0,4) if indexed else (-8,)
                for ii,index in enumerate(indexes):
                    cr=seeds[(fi+pi+bank+ii)%4]; xer=((fi+pi+bank+ii)%8)*536870912+127
                    target=alias+1048576
                    displacements=(index,) if indexed else range(index,index+(16 if short else 65536),width)
                    for d in displacements:
                        instruction=(31*67108864+11*2097152+3*65536+10*2048+code*2) if indexed else code*67108864+11*2097152+3*65536+d%65536
                        initial=memory.to_bytes(4,'big'); data=list(initial)
                        result=source
                        if load:
                            selected=initial[:width]
                            result=int.from_bytes(selected,'little' if reverse else 'big')
                            if sign and result>=32768: result+=4294901760
                        else:
                            selected=(source%(256**width)).to_bytes(width,'little' if reverse else 'big')
                            data[:width]=selected
                        yield (result,(target if update else target-d)%4294967296,d%4294967296,cr,xer,int.from_bytes(bytes(data),'big'),324508639,610839776,instruction,source)

def evaluate(path,rom,short=False):
    cases=SHORT if short else COUNT
    report=dict(status='error',console=CONSOLE,suite='memory_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    started=time.monotonic(); row_pattern=re.compile(r'REG r=[0-9a-f]{80}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} memory_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN transfers cases={cases} words=10\n','wrong domain')
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
    parser=argparse.ArgumentParser(); parser.add_argument('--controls-only',action='store_true'); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/f'build/artifacts/owned/{CONSOLE}_memory')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True); require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',SHORT),('wrong-sign','wrong_sign',120),('wrong-update','wrong_update',768)):
        mutant,_=grade(name,build(short=True,**{option:True}),True)
        require(mutant['status']=='fail' and mutant['mismatches']==failures,'executed mutation escaped: '+json.dumps(mutant))
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(10):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=1968 words=10','cases=1967 words=10',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    if args.controls_only:
        print(f'{CORE}: executed controls and strict stream rejection passed',flush=True); return 0
    rom=build(); require((HERE/('memory_registers.'+EXT)).read_bytes()==rom,'archive differs')
    print(f'{CORE}: running original {COUNT}-case integer memory corpus',flush=True)
    report,_=grade('memory_registers',rom)
    print(f"{CORE} memory: {report['status'].upper()} {report['graded_cases']}/{COUNT} ten-word observations; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
