#!/usr/bin/env python3
"""Independent host byte-array grading of original DSi RAM observations."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
CONSOLE='dsi'
CORE='DSi'
EXT='vdsi'
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=387072
SHORT=6048
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    ordinal=0
    seeds=(324508639,610839776,1437204735,2774204160)
    for mode in range(3):
        for bank in range(4):
            for store_alias in (33554432,201326592,218103808):
                for read_alias in (33554432,201326592,218103808):
                    for writer in (0,1):
                        for width in (1,2,4):
                            for lane in range(0,4,width):
                                address=store_alias+bank*4194304+65536+lane
                                for byte in range(4 if short else 256):
                                    ordinal+=1
                                    source=byte+(255-byte)*256+byte*65536+(255-byte)*16777216
                                    ram=[list(seed.to_bytes(4,'little')) for seed in seeds]
                                    if mode==2 or store_alias==33554432:
                                        selected=bank if mode==2 else 0
                                        ram[selected][lane:lane+width]=source.to_bytes(4,'little')[:width]
                                    if mode<2:
                                        results=[int.from_bytes(bytes(ram[0]),'little')]*4 if read_alias==33554432 else [0]*4
                                    else: results=[int.from_bytes(bytes(row),'little') for row in ram]
                                    yield tuple(results+results+[address,source,width,writer,2147483648+mode*16384,ordinal])

def evaluate(path,rom,short=False):
    cases=SHORT if short else COUNT
    report=dict(status='error',console=CONSOLE,suite='ram_registers',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    started=time.monotonic(); row_pattern=re.compile(r'REG r=[0-9a-f]{112}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            require(stream.readline()==f'VELOCE 1 {CONSOLE} ram_registers\n','wrong suite')
            require(stream.readline()==f'BEGIN mappings cases={cases} words=14\n','wrong domain')
            for index,want in enumerate(expected(short)):
                row=stream.readline(); require(row_pattern.fullmatch(row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,118,8))
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
    parser=argparse.ArgumentParser(); parser.add_argument('--controls-only',action='store_true'); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/dsi_ram')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.'+EXT); result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom); run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short); (args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n'); return report,result
    baseline=build(short=True); positive,path=grade('control',baseline,True); require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-bank','wrong_bank',1512),('wrong-width','wrong_width',1038),('wrong-mode','wrong_mode',4032)):
        report,_=grade(name,build(short=True,**{option:True}),True); require(report['status']=='fail' and report['mismatches']==failures,'executed mutation escaped: '+json.dumps(report))
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(14):
        n=6+field*8; changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1)); require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=6048 words=14','cases=6047 words=14',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken); require(report['status']=='error',name+' accepted')
    if args.controls_only:
        print('DSi: executed controls and strict stream rejection passed',flush=True); return 0
    rom=build(); require((HERE/'ram_registers.vdsi').read_bytes()==rom,'archive differs')
    print(f'DSi: running original {COUNT}-case RAM corpus',flush=True)
    report,_=grade('ram_registers',rom); print(f"DSi RAM: {report['status'].upper()} {report['graded_cases']}/{COUNT}; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
