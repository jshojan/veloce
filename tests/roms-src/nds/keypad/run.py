#!/usr/bin/env python3
"""Grade original guest DS keypad reads with an independent digital input oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
# ABI positions in physical DS bit order: A/B/Select/Start/Right/Left/Up/Down/R/L/X/Y.
BUTTONS=(0,1,7,6,11,10,8,9,5,4,2,3)
# Reserved/unknown register bits are captured but not hardware-scored.
MASKS=(0x3ff,0xff,3,0x3ff,0xff,3,0xcb,0xcb,0,0xffffffff)
def require(ok,message):
    if not ok: raise ValueError(message)
def expected(sample):
    gray=sample^(sample>>1)
    key=1023-(gray%1024); xy=0x48|(3-gray//1024)
    return (key,key%256,key//256,key,key%256,key//256,xy,xy,0,sample+1)
def evaluate(path,rom,cases=4096):
    report=dict(status='error',console='nds',suite='keypad',expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,unscored_masks=[f'{~mask&0xffffffff:08x}' for mask in MASKS],examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=DS rom_crc32={zlib.crc32(rom):08x} channels=dsdebug\n','wrong file identity')
            require(f.readline()=='VELOCE 1 ds keypad\n','wrong suite')
            require(f.readline()==f'BEGIN digital cases={cases} words=10\n','wrong domain')
            for sample in range(cases):
                row=f.readline(); require(re.fullmatch(r'REG r=[0-9a-f]{80}\n',row),'missing/malformed row')
                got=tuple(int(row[n:n+8],16) for n in range(6,86,8)); want=expected(sample)
                report['observed_cases']+=1; report['graded_cases']+=1
                if any((a^b)&mask for a,b,mask in zip(got,want,MASKS)):
                    report['mismatches']+=1
                    if len(report['examples'])<8: report['examples'].append(dict(sample=sample,expected=[f'{v:08x}' for v in want],observed=[f'{v:08x}' for v in got]))
            require(f.readline()==f'LOG complete cases={cases}\n','missing completion')
            require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n',f.readline()),'missing/ungraded trailer')
            require(not f.read(),'trailing data'); report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e: report['detail']=str(e)
    return report
def schedule(path,cases):
    require(len({n^(n>>1) for n in range(4096)})==4096,'Gray domain not complete')
    path.write_text(','.join(f'{n}:{sum(1<<bit for index,bit in enumerate(BUTTONS) if (n^(n>>1))&(1<<index)):x}' for n in range(cases)))
def run(emulator,image,result,input_file,cases):
    env=os.environ.copy()
    for key in ('INPUT','INPUT_FILE','DEBUG','TRACE','TRACE_FILE','VELOCE_TEST_RESETS'): env.pop(key,None)
    env.update(HEADLESS='1',FRAMES=str(cases+5),INPUT_FILE=str(input_file.resolve()),VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT='1')
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=180)
    require(proc.returncode==0,proc.stdout+proc.stderr)
    require(f'Loaded {cases} input events' in proc.stderr,'input file not consumed')
def controls(emulator,artifacts):
    short=artifacts/'control-input.txt'; schedule(short,16)
    baseline=build(16); image=artifacts/'control.nds'; image.write_bytes(baseline)
    path=artifacts/'control.result'; run(emulator,image,path,short,16); report=evaluate(path,baseline,16)
    require(report['status']=='pass','reduced positive failed: '+json.dumps(report)); (artifacts/'control.json').write_text(json.dumps(report,indent=2)+'\n')
    mutant=build(16,wrong_polarity=True); image=artifacts/'wrong-polarity.nds'; image.write_bytes(mutant)
    result=artifacts/'wrong-polarity.result'; run(emulator,image,result,short,16); invalid=evaluate(result,mutant,16)
    require(invalid['status']=='fail' and invalid['mismatches']==16,'executed polarity mutation escaped'); (artifacts/'wrong-polarity.json').write_text(json.dumps(invalid,indent=2)+'\n')
    content=path.read_text(); row=next(line for line in content.splitlines() if line.startswith('REG r='))
    for word,mask in enumerate(MASKS):
        if not mask: continue
        n=6+word*8; bit=mask&-mask; changed=row[:n]+f'{int(row[n:n+8],16)^bit:08x}'+row[n+8:]
        result=artifacts/f'field-{word}.result'; result.write_text(content.replace(row,changed,1))
        invalid=evaluate(result,baseline,16); require(invalid['status']=='fail' and invalid['mismatches']==1,'defined field escaped grade')
        (artifacts/f'field-{word}.json').write_text(json.dumps(invalid,indent=2)+'\n')
    for word,bit in ((0,1024),(2,4),(6,4),(7,4),(8,1)):
        n=6+word*8; changed=row[:n]+f'{int(row[n:n+8],16)^bit:08x}'+row[n+8:]
        result=artifacts/f'reserved-{word}.result'; result.write_text(content.replace(row,changed,1))
        invalid=evaluate(result,baseline,16); require(invalid['status']=='pass','unknown/reserved bit scored as hardware')
        (artifacts/f'reserved-{word}.json').write_text(json.dumps(invalid,indent=2)+'\n')
    for name,broken in (
        ('missing-row',content.replace(row+'\n','',1)),('malformed-row',content.replace(row,'REG r=bad',1)),
        ('wrong-domain',content.replace('cases=16 words=10','cases=15 words=10',1)),
        ('false-pass',content.replace('END observations_complete','END 0/0')),
        ('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated-trailer',content.rstrip('\n')),('trailing-data',content+'extra\n')):
        result=artifacts/(name+'.result'); result.write_text(broken); invalid=evaluate(result,baseline,16)
        require(invalid['status']=='error',name+' accepted'); (artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
def main():
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--emulator',type=Path,default=ROOT/'build/bin/veloce'); parser.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/nds_keypad')
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    rom=build(); require((HERE/'keypad.nds').read_bytes()==rom,'archive differs'); controls(args.emulator,args.artifacts)
    input_file=args.artifacts/'input.txt'; schedule(input_file,4096); path=args.artifacts/'keypad.result'; run(args.emulator,HERE/'keypad.nds',path,input_file,4096)
    report=evaluate(path,rom); (args.artifacts/'keypad.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"DS keypad: {report['status'].upper()} {report['graded_cases']}/4096 ten-word snapshots; mismatches={report['mismatches']}")
    require(report['status']=='pass',json.dumps(report)); return 0
if __name__=='__main__': sys.exit(main())
