#!/usr/bin/env python3
"""Independent stack address/byte ordering oracle for actual guest captures."""
import argparse,json,os,re,subprocess,zlib
from pathlib import Path
from build import build

def expected(case):
    mode,op,sp,value,at=(case[k] for k in ('mode','op','sp','value','at'))
    memory=case['initial'].copy();count=3 if op in ('jsl','rtl') else 1 if op in ('plb','pha','pla') else 2
    pushing=op in ('pea','pei','per','phd','jsl','jsrx','pha');legacy=bool(mode and op in ('pha','pla'))
    if pushing:
        word=((at+3)&65535) if op=='jsl' else ((at+2)&65535) if op=='jsrx' else value
        data=[at>>16,word>>8,word&255] if op=='jsl' else [word>>8,word&255] if count==2 else [value&255]
        for n,v in enumerate(data):memory[(0x100|((sp-n)&255)) if legacy else sp-n]=v
    result=(sp-count if pushing else sp+count)&65535
    if mode:result=0x100|(result&255)
    dp=value if op in ('phd','pld') else 0
    db=(value&255) if op=='plb' else 0
    return [memory[a] for a in case['addresses']],result,dp,db

def evaluate(path,rom,cases):
    lines=path.read_text().splitlines();assert lines[0]==f'#VELOCE 1 core=SNES rom_crc32={zlib.crc32(rom):08x} channels=port,blargg6000,stp,sram,spcport'
    assert lines[1:3]==['VELOCE 1 snes stack_boundaries','INFO cases=5632'];assert len(lines)==len(cases)+5
    assert lines[-2]=='LOG observations_complete' and re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1',lines[-1])
    failures=[]
    for n,case in enumerate(cases):
        m=re.fullmatch(r'REG id=(\d{4}) ram=([0-9a-f]+) s=([0-9a-f]{4}) d=([0-9a-f]{4}) db=([0-9a-f]{2})',lines[n+3]);assert m and int(m[1])==n
        assert len(m[2])==2*len(case['addresses']), 'incomplete RAM observation'
        got=(list(bytes.fromhex(m[2])),int(m[3],16),int(m[4],16),int(m[5],16));want=expected(case)
        if got!=want:failures.append({'id':n,'op':case['op'],'mode':case['mode'],'sp':case['sp'],'got':got,'expected':want})
    return {'status':'fail' if failures else 'pass','cases':len(cases),'mismatches':failures}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--emulator',type=Path,required=True);ap.add_argument('--artifacts',type=Path,required=True);a=ap.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    for name,mutation in [('stack_boundaries',False),('negative',True)]:
        rom,cases=build(mutation);image=a.artifacts/(name+'.sfc');image.write_bytes(rom);out=image.with_suffix('.result');env=os.environ.copy()
        for k in ('DEBUG','TRACE','TRACE_FILE','INPUT','SAVE_SCREENSHOT','VELOCE_TEST_RESETS'):env.pop(k,None)
        env.update(HEADLESS='1',FRAMES='6000',VELOCE_TEST_OUT=str(out.resolve()),VELOCE_TEST_EXIT='1')
        p=subprocess.run([str(a.emulator.resolve()),str(image.resolve())],env=env,cwd=Path(__file__).resolve().parents[4],capture_output=True,text=True,timeout=180)
        assert p.returncode==0,p.stdout+p.stderr
        report=evaluate(out,rom,cases);out.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
        assert report['status']==('fail' if mutation else 'pass'),report
    rom,cases=build();original=(a.artifacts/'stack_boundaries.result').read_text();controls={}
    for name,edit in [('missing',lambda lines:lines[:3]+lines[4:]),
                      ('reordered',lambda lines:lines[:3]+[lines[4],lines[3]]+lines[5:]),
                      ('truncated',lambda lines:lines[:-1])]:
        path=a.artifacts/(name+'.result');path.write_text('\n'.join(edit(original.splitlines()))+'\n')
        try: evaluate(path,rom,cases)
        except AssertionError: controls[name]='measurement_error'
        else: raise AssertionError('damaged capture accepted: '+name)
    (a.artifacts/'controls.json').write_text(json.dumps(controls,indent=2)+'\n')
    print('5632 original CPU stack cases PASS; executed capture-bit mutant and damaged streams rejected')
if __name__=='__main__':main()
