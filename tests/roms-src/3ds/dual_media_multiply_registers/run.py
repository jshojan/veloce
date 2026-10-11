#!/usr/bin/env python3
"""Independent dual signed halfword multiplication oracle for actual ARM11 dual multiply registers."""
import argparse, hashlib, json, os, re, subprocess, sys, time, zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
COUNT=12582912
SHORT=1536
def require(ok,message):
    if not ok: raise ValueError(message)
def groups():
    for kind in range(3):
        for subtract in (False,True):
            for exchange in (False,True):
                for right in (2147516416,2147450880,4294901761,65537):
                    for flags in (0,31):yield kind,subtract,exchange,right,flags
def signed(value,bits):
    return int.from_bytes(value.to_bytes(bits//8,'little'),'little',signed=True)
def expected(short=False):
    values=(0,1,127,128,255,32768,65534,65535) if short else range(65536)
    for kind,subtract,exchange,right,flags in groups():
        ins={0:0xe704f810,1:0xe704a810,2:0xe7454810}[kind]+int(subtract)*64+int(exchange)*32
        psw=(flags<<27)+((15 if flags else 0)<<16)+223
        seed_lo=2147483647 if flags else 4294967295;seed_hi=2147483648 if flags else 1
        b=[signed(right%65536,16),signed(right//65536,16)]
        if exchange:b.reverse()
        for value in values:
            left=value+2147483648
            products=[signed(value,16)*b[0],-32768*b[1]]
            delta=products[0]+(-products[1] if subtract else products[1])
            captured_psw=psw
            if kind==2:
                result=(seed_hi*4294967296+seed_lo+delta)%18446744073709551616
                low=result%4294967296;high=result//4294967296
            else:
                result=delta+(signed(seed_lo,32) if kind else 0)
                low=result%4294967296;high=seed_hi
                if not -2147483648<=result<=2147483647:captured_psw|=134217728
            yield (low,high,captured_psw,left,right,ins,psw,seed_lo,seed_hi,2309737967)
def evaluate(path,rom,short=False):
    count=SHORT if short else COUNT
    report=dict(status='error',console='3ds',suite='dual_media_multiply_registers',expected_cases=count,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    start=time.monotonic(); pattern=re.compile(r'REG r=[0-9a-f]{80}\n')
    try:
        with path.open(encoding='ascii') as stream:
            require(stream.readline()==f'#VELOCE 1 core=3DS rom_crc32={zlib.crc32(rom):08x} channels=3dsdebug\n','wrong file identity')
            require(stream.readline()=='VELOCE 1 3ds dual_media_multiply_registers\n','wrong suite')
            for core in range(2):
                require(stream.readline()==f'BEGIN arm11_{core} cases={count//2} words=10\n','wrong core domain')
                for index,want in enumerate(expected(short)):
                    line=stream.readline();require(pattern.fullmatch(line),'missing/malformed row')
                    got=tuple(int(line[n:n+8],16) for n in range(6,86,8));report['observed_cases']+=1;report['graded_cases']+=1
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
    parser=argparse.ArgumentParser();parser.add_argument('--controls-only',action='store_true');parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/3ds_dual_media_multiply')
    args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    inputs=(args.emulator,ROOT/'build/bin/cores/3ds.so',HERE/'build.py',HERE/'run.py',HERE/'dual_media_multiply_registers.v3ds',HERE/'recipe.json',HERE.parent/'reversal_registers/build.py',ROOT/'cores/nds/src/arm.cpp')
    fingerprints={str(path.resolve()):hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
    def grade(name,rom,short=False,execute=True,content=None):
        image=args.artifacts/(name+'.v3ds');result=args.artifacts/(name+'.result')
        if execute: image.write_bytes(rom);run(args.emulator,image,result)
        else: result.write_text(content)
        report=evaluate(result,rom,short);(args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,result
    baseline=build(short=True);positive,path=grade('control',baseline,True);require(positive['status']=='pass',json.dumps(positive))
    for name,option in (('wrong-result','wrong_result'),('wrong-flags','wrong_flags'),('wrong-accumulator','wrong_acc'),('wrong-exchange','wrong_exchange'),('wrong-subtract','wrong_subtract'),('wrong-source','wrong_source')):
        report,_=grade(name,build(short=True,**{option:True}),True)
        require(report['status']=='fail' and report['mismatches']>0,'executed mutant escaped: '+json.dumps(report))
        if option in ('wrong_result','wrong_flags'):require(report['mismatches']==SHORT,'result/flag mutant escaped cases')
        print(f'{name}: {report["mismatches"]}/{SHORT} mismatches',flush=True)
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for field in range(10):
        n=6+field*8;changed=row[:n]+f'{int(row[n:n+8],16)^1:08x}'+row[n+8:]
        report,_=grade(f'field-{field}',baseline,True,False,content.replace(row,changed,1));require(report['status']=='fail' and report['mismatches']==1,'field escaped')
    for name,broken in (('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=768 words=10','cases=767 words=10',1)),('false-pass',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        report,_=grade(name,baseline,True,False,broken);require(report['status']=='error',name+' accepted')
    if args.controls_only:
        require(all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in fingerprints.items()),'capture provenance changed')
        print('3DS: executed controls and strict stream rejection passed',flush=True);return 0
    rom=build();require((HERE/'dual_media_multiply_registers.v3ds').read_bytes()==rom,'archive differs')
    print(f'3DS: running original {COUNT}-case ARM11 corpus',flush=True);report,_=grade('dual_media_multiply_registers',rom)
    print(f"3DS dual multiply: {report['status'].upper()} {report['graded_cases']}/{COUNT}; mismatches={report['mismatches']}",flush=True)
    require(report['status']=='pass',json.dumps(report))
    require(all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in fingerprints.items()),'capture provenance changed')
    (args.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n');return 0
if __name__=='__main__':sys.exit(main())
