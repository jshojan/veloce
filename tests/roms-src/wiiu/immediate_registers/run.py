#!/usr/bin/env python3
"""Independent host byte-array oracle for original Wii U PowerPC register streams."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=25165824
SHORT=3072
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(core,short=False):
    values=(0,1,127,128,255,32767,32768,65535) if short else range(65536)
    sources=(0,1,2147483647,2147483648)
    seeds=(0,4294967295,2779096485,305419896)
    for op in (14,15):
        for ai,source in enumerate(sources):
            for ra in (0,3):
                for flags in range(8):
                    seed=seeds[(ai+ra+flags)%4];xer=flags*536870912+127
                    for encoding in values:
                        immediate=encoding if encoding<32768 else encoding-65536
                        addend=immediate if op==14 else immediate*65536
                        result=((source if ra else 0)+addend)%4294967296
                        ins=op*67108864+11*2097152+ra*65536+encoding
                        yield (result,seed,xer,source,ins,seed,xer,305419896,core)
def evaluate(path,rom,short=False):
    count=SHORT if short else COUNT
    report=dict(status='error',console='wiiu',suite='immediate_registers',expected_cases=count,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    start=time.monotonic(); pattern=re.compile(r'REG r=[0-9a-f]{72}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core=Wii_U rom_crc32={zlib.crc32(rom):08x} channels=wiiudebug\n','wrong file identity')
            require(stream.readline()=='VELOCE 1 wiiu immediate_registers\n','wrong suite')
            for core in range(3):
                require(stream.readline()==f'BEGIN cpu{core} cases={count//3} words=9\n','wrong CPU domain')
                for index,want in enumerate(expected(core,short)):
                    line=stream.readline();require(pattern.fullmatch(line),'missing/malformed row')
                    got=tuple(int(line[n:n+8],16) for n in range(6,78,8));report['observed_cases']+=1;report['graded_cases']+=1
                    if got!=want:
                        report['mismatches']+=1
                        if len(report['examples'])<8:report['examples'].append(dict(core=core,index=index,expected=list(want),observed=list(got)))
                    if not short and report['graded_cases']%4000000==0: print(f'Wii U: graded {report["graded_cases"]}/{count}; mismatches={report["mismatches"]}',flush=True)
                require(stream.readline()==f'BLOCK complete cpu{core}\n','wrong core completion')
            require(report['graded_cases']==count,'oracle domain incomplete')
            require(stream.readline()==f'LOG complete cases={count}\n','missing completion')
            require(stream.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',stream.readline()),'missing/ungraded trailer')
            require(not stream.read(),'trailing data');report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as error: report['detail']=str(error)
    report['grading_seconds']=time.monotonic()-start;return report
def run(emulator,image,path):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'): env.pop(key,None)
    env.update(HEADLESS='1',FRAMES='300000',VELOCE_TEST_OUT=str(path.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=1800)
    require(proc.returncode==0,proc.stdout+proc.stderr)
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--controls-only',action='store_true');parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/wiiu_immediates')
    args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.vwiiu');result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom);run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short);(args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,result
    baseline=build(short=True);positive,path=grade('control',baseline,True);require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',3072),('wrong-width','wrong_width',1536),('wrong-flags','wrong_flags',2304)):
        report,_=grade(name,build(short=True,**{option:True}),True);require(report['status']=='fail' and report['mismatches']==failures,'executed mutant escaped: '+json.dumps(report))
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(9):
        n=6+field*8;changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1));require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=1024 words=9','cases=1023 words=9',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken);require(report['status']=='error',name+' accepted')
    if args.controls_only:print('Wii U: executed controls and strict stream rejection passed',flush=True);return 0
    rom=build();require((HERE/'immediate_registers.vwiiu').read_bytes()==rom,'archive differs')
    print(f'Wii U: running original {COUNT}-case three-context PowerPC corpus',flush=True);report,_=grade('immediate_registers',rom)
    print(f"Wii U immediate: {report['status'].upper()} {report['graded_cases']}/{COUNT}; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report));return 0
if __name__=='__main__':sys.exit(main())
