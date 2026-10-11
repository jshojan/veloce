#!/usr/bin/env python3
"""Independent lane-arithmetic and GE selection oracle for actual ARM11 parallel/SEL registers."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=19922944
SHORT=2432
def require(ok,message):
    if not ok: raise ValueError(message)
def groups():
    for family in (1,2,3,5,6,7):
        for op in (0,1,2,3,4,7):
            for seed in ((0,) if op>=4 else (2147450880,4294901761)):
                for flags in (0,31):yield family,op,seed,flags,(15 if flags else 0)
    for ge in range(16):
        for flags in (0,31):yield 0,0,0,flags,ge
def expected(short=False):
    values=(0,1,127,128,255,32768,65534,65535) if short else range(65536)
    for family,op,seed,flags,ge in groups():
        ins=(0xe6004f18+family*1048576+op*32) if family else 0xe6804fb8
        psw=(flags<<27)+(ge<<16)+223
        for value in values:
            if family and op<4:left=value+(65535-value)*65536;right=seed
            else:
                x=value%256;y=value//256
                left=x*257+(255-x)*16842752;right=y*65537+(255-y)*16777472
            bits=16 if op<4 else 8;modulus=2**bits
            actual_psw=psw;result=0
            if family:
                status=0
                for index in range(32//bits):
                    other=(1-index) if op in (1,2) else index
                    a=int.from_bytes(left.to_bytes(4,'little')[index*bits//8:(index+1)*bits//8],'little',signed=family<4)
                    b=int.from_bytes(right.to_bytes(4,'little')[other*bits//8:(other+1)*bits//8],'little',signed=family<4)
                    subtract=op in (3,7) or (op==1 and index==0) or (op==2 and index==1)
                    arithmetic=a-b if subtract else a+b
                    if family in (1,5):
                        positive=(arithmetic>=0) if family==1 or subtract else (arithmetic>=modulus)
                        if positive:status+=(3 if bits==16 else 1)*2**(index*bits//8)
                        output=arithmetic
                    elif family in (2,6):output=max(-modulus//2 if family==2 else 0,min(arithmetic,modulus//2-1 if family==2 else modulus-1))
                    else:output=arithmetic//2
                    result+=(output%modulus)*2**(index*bits)
                if family in (1,5):actual_psw=(psw & ~983040)+status*65536
            def selection(psw):
                a=left.to_bytes(4,'little');b=right.to_bytes(4,'little')
                return int.from_bytes(bytes(a[i] if psw & 2**(16+i) else b[i] for i in range(4)),'little')
            if not family:result=selection(psw)
            yield (result,actual_psw,left,right,ins,psw,selection(actual_psw),actual_psw,2309737967)
def evaluate(path,rom,short=False):
    count=SHORT if short else COUNT
    report=dict(status='error',console='3ds',suite='parallel_registers',expected_cases=count,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    start=time.monotonic(); pattern=re.compile(r'REG r=[0-9a-f]{72}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core=3DS rom_crc32={zlib.crc32(rom):08x} channels=3dsdebug\n','wrong file identity')
            require(stream.readline()=='VELOCE 1 3ds parallel_registers\n','wrong suite')
            for core in range(2):
                require(stream.readline()==f'BEGIN arm11_{core} cases={count//2} words=9\n','wrong core domain')
                for index,want in enumerate(expected(short)):
                    line=stream.readline();require(pattern.fullmatch(line),'missing/malformed row')
                    got=tuple(int(line[n:n+8],16) for n in range(6,78,8));report['observed_cases']+=1;report['graded_cases']+=1
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
    parser=argparse.ArgumentParser();parser.add_argument('--controls-only',action='store_true');parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/3ds_parallel')
    args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    inputs=(args.emulator,ROOT/'build/bin/cores/3ds.so',HERE/'build.py',HERE/'run.py',HERE/'parallel_registers.v3ds',HERE/'recipe.json',HERE.parent/'reversal_registers/build.py',ROOT/'cores/nds/src/arm.cpp')
    fingerprints={str(path.resolve()):hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.v3ds');result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom);run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short);(args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,result
    baseline=build(short=True);positive,path=grade('control',baseline,True);require(positive['status']=='pass',json.dumps(positive))
    for name,option,failures in (('wrong-result','wrong_result',2432),('wrong-flags','wrong_flags',2432),('wrong-select','wrong_select',2432),('wrong-ge','wrong_ge',2432)):
        report,_=grade(name,build(short=True,**{option:True}),True);require(report['status']=='fail' and report['mismatches']==failures,'executed mutant escaped: '+json.dumps(report))
    wrong,_=grade('wrong-source',build(short=True,wrong_source=True),True)
    require(wrong['status']=='fail' and wrong['mismatches']>0,'executed wrong source escaped')
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(9):
        n=6+field*8;changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1));require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=1216 words=9','cases=1215 words=9',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken);require(report['status']=='error',name+' accepted')
    if args.controls_only:
        require(all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in fingerprints.items()),'capture provenance changed')
        print('3DS: executed controls and strict stream rejection passed',flush=True);return 0
    rom=build();require((HERE/'parallel_registers.v3ds').read_bytes()==rom,'archive differs')
    print(f'3DS: running original {COUNT}-case ARM11 corpus',flush=True);report,_=grade('parallel_registers',rom)
    print(f"3DS parallel: {report['status'].upper()} {report['graded_cases']}/{COUNT}; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report))
    require(all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in fingerprints.items()),'capture provenance changed')
    (args.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n');return 0
if __name__=='__main__':sys.exit(main())
