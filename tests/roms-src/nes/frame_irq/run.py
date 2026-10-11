#!/usr/bin/env python3
"""Independent integer oracle for original guest frame IRQ status pairs."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import zlib
from build import build

def expected(control,parity,target):
    if control&0xc0: return [0,0]
    delay=4 if parity else 3
    events=[delay+period*29830+edge for period in range(3) for edge in (29828,29829,29830)]
    return [0x40 if any(e<=target for e in events) else 0,
            0x40 if any(target<e<=target+7 for e in events) else 0]

def evaluate(path,rom,cases):
    lines=path.read_text().splitlines()
    assert lines[0]==f'#VELOCE 1 core=NES rom_crc32={zlib.crc32(rom[16:]):08x} channels=port,blargg6000'
    assert lines[1:3]==['VELOCE 1 nes frame_irq','INFO cases=80']
    assert len(lines)==85 and lines[-2]=='LOG observations_complete'
    assert re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1',lines[-1]),lines[-1]
    mismatches=[]
    for n,case in enumerate(cases):
        m=re.fullmatch(r'REG id=(\d{3}) status=([0-9a-f]{2}),([0-9a-f]{2})',lines[3+n]);assert m and int(m[1])==n
        got=[int(m[2],16),int(m[3],16)];want=expected(*case)
        if got!=want:mismatches.append({'id':n,'case':case,'got':got,'expected':want})
    return {'cases':len(cases),'observations':2*len(cases),'mismatches':mismatches,'status':'fail' if mismatches else 'pass'}

def main():
    a=argparse.ArgumentParser();a.add_argument('--emulator',type=Path,required=True);a.add_argument('--artifacts',type=Path,required=True);args=a.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    def execute(name,mutation=False):
        rom,cases=build(mutation);r=args.artifacts/(name+'.nes');r.write_bytes(rom);out=r.with_suffix('.result')
        env=os.environ.copy()
        for k in ('TRACE','TRACE_FILE','INPUT','DEBUG','SAVE_SCREENSHOT','VELOCE_TEST_RESETS'):env.pop(k,None)
        env.update(HEADLESS='1',FRAMES='1000',VELOCE_TEST_OUT=str(out.resolve()),VELOCE_TEST_EXIT='1')
        proc=subprocess.run([str(args.emulator.resolve()),str(r.resolve())],env=env,cwd=Path(__file__).resolve().parents[4],capture_output=True,text=True,timeout=90)
        assert proc.returncode==0,proc.stdout+proc.stderr
        report=evaluate(out,rom,cases);out.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');return report,out
    report,out=execute('frame_irq');assert report['status']=='pass',report
    mutant,_=execute('negative',True);assert mutant['status']=='fail',mutant
    controls={'executed_mutant':mutant['status']}
    for name,change in [('missing',lambda s:s.replace('REG id=000 status=00,00\n','')),('changed',lambda s:s.replace('REG id=000 status=00,00','REG id=000 status=40,00'))]:
        p=args.artifacts/(name+'.result');p.write_text(change(out.read_text()))
        try: bad=evaluate(p,*build())
        except AssertionError:
            controls[name]='measurement_error';continue
        assert bad['status']!='pass', 'damaged capture was accepted'
        controls[name]=bad['status']
    (args.artifacts/'controls.json').write_text(json.dumps(controls,indent=2)+'\n')
    print('80 original frame IRQ cases / 160 actual status reads PASS; executed mutation and malformed capture controls rejected')
if __name__=='__main__':main()
