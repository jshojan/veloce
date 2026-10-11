#!/usr/bin/env python3
"""Independent integer/byte-lane oracle for actual ARM11 extension registers."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=13631488
SHORT=1664
def require(ok,message):
    if not ok: raise ValueError(message)
def groups():
    for kind in (0,2,3,4,6,7):
        for accumulate in (False,True):
            for rotation in range(4):
                for flags in (0,31):yield False,kind,accumulate,rotation,flags
    for kind in (3,2,7,6):
        for flags in (0,31):yield True,kind,False,0,flags
def instruction(thumb,kind,accumulate,rotation):
    if thumb:return {3:0xb204,2:0xb244,7:0xb284,6:0xb2c4}[kind]
    bases={0:0xe68f4070,2:0xe6af4070,3:0xe6bf4070,4:0xe6cf4070,6:0xe6ef4070,7:0xe6ff4070}
    return bases[kind]-(0x70000 if accumulate else 0)+rotation*1024
def expected(short=False):
    values=(0,1,127,128,255,32768,65534,65535) if short else range(65536)
    for thumb,kind,accumulate,rotation,flags in groups():
        ins=instruction(thumb,kind,accumulate,rotation);psw=(flags<<27)|223
        seed=0x7fffffff if flags==0 else 0x80008000
        for value in values:
            if kind%4==0:shaped=(value%256)+((value//256)<<16)+0xa5005a00
            else:shaped=value+(65535-value)*65536
            data=shaped.to_bytes(4,'little');cut=(4-rotation)%4;source=int.from_bytes(data[cut:]+data[:cut],'little')
            def lane(index,width):
                raw=data[index:index+width];return int.from_bytes(raw,'little',signed=kind<4)
            if kind%4==0:
                lo=((seed%65536 if accumulate else 0)+lane(0,1))%65536
                hi=((seed//65536 if accumulate else 0)+lane(2,1))%65536
                result=lo+hi*65536
            else:result=((seed if accumulate else 0)+lane(0,2 if kind%4==3 else 1))%4294967296
            yield (result,psw,source,ins,psw,seed,2309737967)
def evaluate(path,rom,short=False):
    count=SHORT if short else COUNT
    report=dict(status='error',console='3ds',suite='extend_registers',expected_cases=count,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    start=time.monotonic(); pattern=re.compile(r'REG r=[0-9a-f]{56}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core=3DS rom_crc32={zlib.crc32(rom):08x} channels=3dsdebug\n','wrong file identity')
            require(stream.readline()=='VELOCE 1 3ds extend_registers\n','wrong suite')
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
    inputs=(args.emulator,ROOT/'build/bin/cores/3ds.so',HERE/'build.py',HERE/'run.py',HERE/'extend_registers.v3ds',HERE/'recipe.json',HERE.parent/'reversal_registers/build.py',ROOT/'cores/nds/src/arm.cpp')
    fingerprints={str(path.resolve()):hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.v3ds');result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom);run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short);(args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,result
    baseline=build(short=True);positive,path=grade('control',baseline,True);require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',1664),('wrong-flags','wrong_flags',1664)):
        report,_=grade(name,build(short=True,**{option:True}),True);require(report['status']=='fail' and report['mismatches']==failures,'executed mutant escaped: '+json.dumps(report))
    wrong,_=grade('wrong-source',build(short=True,wrong_source=True),True)
    require(wrong['status']=='fail' and wrong['mismatches']>0,'executed wrong source escaped')
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(7):
        n=6+field*8;changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1));require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=832 words=7','cases=831 words=7',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken);require(report['status']=='error',name+' accepted')
    if args.controls_only:
        require(all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in fingerprints.items()),'capture provenance changed')
        print('3DS: executed controls and strict stream rejection passed',flush=True);return 0
    rom=build();require((HERE/'extend_registers.v3ds').read_bytes()==rom,'archive differs')
    print(f'3DS: running original {COUNT}-case ARM11 corpus',flush=True);report,_=grade('extend_registers',rom)
    print(f"3DS extension: {report['status'].upper()} {report['graded_cases']}/{COUNT}; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report))
    require(all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in fingerprints.items()),'capture provenance changed')
    (args.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n');return 0
if __name__=='__main__':sys.exit(main())
