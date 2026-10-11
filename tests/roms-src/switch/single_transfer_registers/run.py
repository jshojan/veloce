#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent byte/extension/address oracle for actual AArch64 single-transfer captures."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
COUNT=7188480;SHORT=3328
def require(ok,message):
    if not ok:raise ValueError(message)
def expected(short=False):
    for value in ((0,127,128,255) if short else range(256)):
        seed=int.from_bytes(bytes([value])*8,'little')
        for scale in range(4):
            for op in range(4):
                if not (op<2 or (op==2 and scale<3) or (op==3 and scale<2)):continue
                groups=[]
                for mode in range(4):
                    for imm in ((0,1,255,256,511) if short else range(512)):groups.append((mode,imm,31 if imm%2 else 8,15 if imm%4>=2 else 0,0,0,False))
                for imm in ((0,1,4095) if short else (0,1,2,7,255,256,2048,4095)):
                    for base in (8,31):
                        for flags in (0,15):groups.append((4,imm,base,flags,0,0,False))
                for option in (2,3,6,7):
                    for scaled in (False,True):
                        for index in ((0,1) if short else (0,1,255,18446744069414584321,18446744073709551615)):
                            for flags in (0,15):groups.append((5,0,8,flags,index,option,scaled))
                for mode,imm,base,flags,index,option,scaled in groups:
                    size=2**scale
                    if mode==4:offset=imm*size
                    elif mode==5:
                        raw=index.to_bytes(8,'little')
                        operand=int.from_bytes(raw[:4] if option in (2,6) else raw,'little',signed=option in (6,7))
                        offset=(operand*(size if scaled else 1))%2**64
                    else:offset=int.from_bytes((imm*128).to_bytes(2,'little'),'little',signed=True)//128
                    origin=(0x3c0800-offset)%2**64 if mode==5 else 0x3c0800
                    ins=0x38000000+scale*2**30+op*2**22+base*32
                    ins+=(2**24+imm*1024 if mode==4 else 2**21+2048+11*65536+option*8192+int(scaled)*4096 if mode==5 else imm*4096+mode*1024)
                    data=bytes([value])*size;memory=int.from_bytes(data,'little')
                    result=seed if op==0 else int.from_bytes(data,'little',signed=op>=2)%(2**32 if op==3 else 2**64)
                    updated=(origin+offset)%2**64 if mode in (1,3) else origin
                    yield (result,updated,memory,flags*2**28,seed,ins,index,origin)
def evaluate(path,rom,short=False):
    count=SHORT if short else COUNT;report=dict(status='error',expected_cases=count,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest())
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=Switch rom_crc32={zlib.crc32(rom):08x} channels=vnxdebug\n','wrong identity')
            require(f.readline()=='VELOCE 1 switch single_transfer_registers\n','wrong suite')
            require(f.readline()==f'BEGIN singles cases={count} words=8\n','wrong domain')
            for index,want in enumerate(expected(short)):
                line=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{128}\n',line),'malformed/missing row')
                got=tuple(int(line[n:n+16],16) for n in range(6,134,16));report['graded_cases']+=1
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8:report['examples'].append(dict(index=index,expected=want,observed=got))
            require(report['graded_cases']==count,'oracle domain incomplete')
            require(f.readline()==f'LOG complete cases={count}\n','missing completion');require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'invalid trailer');require(not f.read(),'trailing data');report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--controls-only',action='store_true');parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce');parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/switch_single_transfer');args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    inputs=(args.emulator,ROOT/'build/bin/cores/switch.so',HERE/'build.py',HERE/'run.py',HERE/'recipe.json',HERE/'single_transfer_registers.nso',HERE.parent/'arithmetic_registers/build.py',HERE.parent/'ccmp_registers/build.py',ROOT/'cores/switch/src/aarch64.cpp',ROOT/'cores/switch/src/machine.cpp')
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
    for name,option,count in [('wrong-result','wrong_result',SHORT),('wrong-flags','wrong_flags',SHORT),('wrong-offset','wrong_offset',1664),('wrong-sign','wrong_sign',640),('wrong-store','wrong_store',1024)]:
        bad,_=grade(name,build(short=True,**{option:True}),True);require(bad['status']=='fail' and bad['mismatches']==count,'executed mutant escaped '+json.dumps(bad));print(f'{name}: {count}/{SHORT} mismatches',flush=True)
    content=path.read_text();row=next(x for x in content.splitlines() if x.startswith('REG r='))
    for field in range(8):
        at=6+16*field;changed=row[:at]+format(int(row[at:at+16],16)^1,'016x')+row[at+16:];r,_=grade(f'field-{field}',short,True,content.replace(row,changed,1));require(r['status']=='fail' and r['mismatches']==1,'field mutation escaped')
    for name,broken in [('missing-row',content.replace(row+'\n','',1)),('malformed',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=3328 words=8','cases=3327 words=8')),('false-END',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing',content+'extra\n')]:
        r,_=grade(name,short,True,broken);require(r['status']=='error',name+' escaped')
    if not args.controls_only:
        rom=build();require(rom==(HERE/'single_transfer_registers.nso').read_bytes(),'archive differs');good,_=grade('single_transfer_registers',rom);require(good['status']=='pass',json.dumps(good));print(f'Switch single transfers: {COUNT} cases / {COUNT*8} actual words; zero mismatches PASS',flush=True)
    require(all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items()),'capture provenance changed')
    (args.artifacts/'provenance.json').write_text(json.dumps(hashes,indent=2)+'\n');print('Executed controls, eight fields and seven stream controls PASS',flush=True)
if __name__=='__main__':sys.exit(main())
