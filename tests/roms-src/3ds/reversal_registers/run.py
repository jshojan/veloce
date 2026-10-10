#!/usr/bin/env python3
"""Independent host byte-array oracle for original 3DS ARM11 register streams."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=25165824
SHORT=3072
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(short=False):
    values=(0,1,127,128,255,32768,65534,65535) if short else range(65536)
    for thumb in (False,True):
        for kind in range(3):
            ins=(0xba04+(0,64,192)[kind]) if thumb else (3871297328,3871297456,3875491760)[kind]
            for flags in range(32):
                psw=flags*134217728+223
                for low in values:
                    source=low+(65535-low)*65536; data=source.to_bytes(4,'little')
                    if kind==0: result=int.from_bytes(data[::-1],'little')
                    elif kind==1: result=int.from_bytes(bytes((data[1],data[0],data[3],data[2])),'little')
                    else: result=int.from_bytes(data[:2][::-1],'little',signed=True)%4294967296
                    yield (result,psw,source,ins,psw,305419896,2309737967)
def evaluate(path,rom,short=False):
    count=SHORT if short else COUNT
    report=dict(status='error',console='3ds',suite='reversal_registers',expected_cases=count,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    start=time.monotonic(); pattern=re.compile(r'REG r=[0-9a-f]{56}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core=3DS rom_crc32={zlib.crc32(rom):08x} channels=3dsdebug\n','wrong file identity')
            require(stream.readline()=='VELOCE 1 3ds reversal_registers\n','wrong suite')
            for core in range(2):
                require(stream.readline()==f'BEGIN arm11_{core} cases={count//2} words=7\n','wrong core domain')
                for index,want in enumerate(expected(short)):
                    line=stream.readline();require(pattern.fullmatch(line),'missing/malformed row')
                    got=tuple(int(line[n:n+8],16) for n in range(6,62,8));report['observed_cases']+=1;report['graded_cases']+=1
                    if got!=want:
                        report['mismatches']+=1
                        if len(report['examples'])<8:report['examples'].append(dict(core=core,index=index,expected=list(want),observed=list(got)))
                    if not short and report['graded_cases']%4000000==0: print(f'3DS: graded {report["graded_cases"]}/{count}; mismatches={report["mismatches"]}',flush=True)
                require(stream.readline()==f'BLOCK complete arm11_{core}\n','wrong core completion')
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
    parser=argparse.ArgumentParser();parser.add_argument('--controls-only',action='store_true');parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/3ds_reversal')
    args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.v3ds');result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom);run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short);(args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,result
    baseline=build(short=True);positive,path=grade('control',baseline,True);require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',3072),('wrong-half','wrong_half',1024),('wrong-signed','wrong_signed',1024),('wrong-flags','wrong_flags',3072)):
        report,_=grade(name,build(short=True,**{option:True}),True);require(report['status']=='fail' and report['mismatches']==failures,'executed mutant escaped: '+json.dumps(report))
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(7):
        n=6+field*8;changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1));require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=1536 words=7','cases=1535 words=7',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken);require(report['status']=='error',name+' accepted')
    if args.controls_only:print('3DS: executed controls and strict stream rejection passed',flush=True);return 0
    rom=build();require((HERE/'reversal_registers.v3ds').read_bytes()==rom,'archive differs')
    print(f'3DS: running original {COUNT}-case ARM11 corpus',flush=True);report,_=grade('reversal_registers',rom)
    print(f"3DS reversal: {report['status'].upper()} {report['graded_cases']}/{COUNT}; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report));return 0
if __name__=='__main__':sys.exit(main())
