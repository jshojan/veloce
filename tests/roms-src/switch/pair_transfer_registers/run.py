#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent byte/extension/address oracle for actual AArch64 paired-transfer captures."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
COUNT=2490368;SHORT=1520
def require(ok,message):
    if not ok:raise ValueError(message)
def expected(short=False):
    for value in ((0,127,128,255) if short else range(256)):
        seed=int.from_bytes(bytes([value])*8,'little');other=int.from_bytes(bytes([255-value])*8,'little')
        for kind in range(5):
            for mode in range(4):
                if kind==4 and mode==0:continue
                for imm in ((0,1,63,64,127) if short else range(128)):
                    for base in (8,31):
                        for flags in (0,15):
                            size=8 if kind in (1,3) else 4
                            offset=(imm if imm<64 else imm-128)*size
                            ins={0:0x28000000,1:0xa8000000,2:0x28400000,3:0xa8400000,4:0x68400000}[kind]+mode*8388608+imm*32768+1024+base*32
                            a=int.from_bytes(seed.to_bytes(8,'little')[:size],'little');b=int.from_bytes(other.to_bytes(8,'little')[:size],'little')
                            if kind<2:r0,r1=seed,other
                            elif kind==4:r0=int.from_bytes(a.to_bytes(4,'little'),'little',signed=True)%2**64;r1=int.from_bytes(b.to_bytes(4,'little'),'little',signed=True)%2**64
                            else:r0,r1=a,b
                            updated=0x300800+(offset if mode in (1,3) else 0)
                            yield (r0,r1,updated,a,b,flags*2**28,seed,other,ins,0x300800)
def evaluate(path,rom,short=False):
    count=SHORT if short else COUNT;report=dict(status='error',expected_cases=count,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest())
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=Switch rom_crc32={zlib.crc32(rom):08x} channels=vnxdebug\n','wrong identity')
            require(f.readline()=='VELOCE 1 switch pair_transfer_registers\n','wrong suite')
            require(f.readline()==f'BEGIN pairs cases={count} words=10\n','wrong domain')
            for index,want in enumerate(expected(short)):
                line=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{160}\n',line),'malformed/missing row')
                got=tuple(int(line[n:n+16],16) for n in range(6,166,16));report['graded_cases']+=1
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8:report['examples'].append(dict(index=index,expected=want,observed=got))
            require(report['graded_cases']==count,'oracle domain incomplete')
            require(f.readline()==f'LOG complete cases={count}\n','missing completion');require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'invalid trailer');require(not f.read(),'trailing data');report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--controls-only',action='store_true');parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/switch_pair_transfer');args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    inputs=(args.emulator,ROOT/'build/bin/cores/switch.so',HERE/'build.py',HERE/'run.py',HERE/'recipe.json',HERE/'pair_transfer_registers.nso',HERE.parent/'arithmetic_registers/build.py',HERE.parent/'ccmp_registers/build.py',ROOT/'cores/switch/src/aarch64.cpp',ROOT/'cores/switch/src/machine.cpp')
    hashes={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    env=os.environ.copy()
    for name in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'):env.pop(name,None)
    def grade(name,rom,short=False,content=None):
        image=args.artifacts/(name+'.nso');path=args.artifacts/(name+'.result')
        if content is None:
            image.write_bytes(rom);settings=dict(env,HEADLESS='1',FRAMES='3000',VELOCE_TEST_OUT=str(path.resolve()),VELOCE_TEST_EXIT='1')
            proc=subprocess.run([str(args.emulator.resolve()),str(image.resolve())],cwd=ROOT,env=settings,capture_output=True,text=True,timeout=300)
            (args.artifacts/(name+'.log')).write_text(proc.stdout+proc.stderr);require(proc.returncode==0,proc.stdout+proc.stderr)
        else:path.write_text(content)
        report=evaluate(path,rom,short);(args.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,path
    short=build(short=True);good,path=grade('control',short,True);require(good['status']=='pass',json.dumps(good))
    for name,option,count in [('wrong-result','wrong_result',SHORT),('wrong-flags','wrong_flags',SHORT),('wrong-offset','wrong_offset',SHORT),('wrong-sign','wrong_sign',240),('wrong-store','wrong_store',640)]:
        bad,_=grade(name,build(short=True,**{option:True}),True);require(bad['status']=='fail' and bad['mismatches']==count,'executed mutant escaped '+json.dumps(bad));print(f'{name}: {count}/{SHORT} mismatches',flush=True)
    content=path.read_text();row=next(x for x in content.splitlines() if x.startswith('REG r='))
    for field in range(10):
        at=6+16*field;changed=row[:at]+format(int(row[at:at+16],16)^1,'016x')+row[at+16:];r,_=grade(f'field-{field}',short,True,content.replace(row,changed,1));require(r['status']=='fail' and r['mismatches']==1,'field mutation escaped')
    for name,broken in [('missing-row',content.replace(row+'\n','',1)),('malformed',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=1520 words=10','cases=1519 words=10')),('false-END',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing',content+'extra\n')]:
        r,_=grade(name,short,True,broken);require(r['status']=='error',name+' escaped')
    if not args.controls_only:
        rom=build();require(rom==(HERE/'pair_transfer_registers.nso').read_bytes(),'archive differs');good,_=grade('pair_transfer_registers',rom);require(good['status']=='pass',json.dumps(good));print(f'Switch paired transfers: {COUNT} cases / {COUNT*10} actual words; zero mismatches PASS',flush=True)
    require(all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items()),'capture provenance changed')
    (args.artifacts/'provenance.json').write_text(json.dumps(hashes,indent=2)+'\n');print('Executed controls, ten fields and seven stream controls PASS',flush=True)
if __name__=='__main__':sys.exit(main())
