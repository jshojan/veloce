#!/usr/bin/env python3
"""Independent decimal arithmetic/status oracle for actual guest captures."""
import argparse,json,os,re,subprocess,zlib
from pathlib import Path
from build import build

def expected(case):
    a,b,c=case
    # Complement addition in radix 16; decide carry before decimal correction.
    # This host model uses addition, independently of the core's subtract path.
    carry=c;digits=[]
    for left,right in zip(f'{a:04x}'[::-1],f'{b:04x}'[::-1]):
        total=int(left,16)+(15-int(right,16))+carry
        carry=int(total>=16)
        digits.append((total if carry else total-6)%16)
    result=sum(digit<<(4*n) for n,digit in enumerate(digits))
    binary=(a-b-(1-c))&65535
    p=0x1c | int(a-b-(1-c)>=0) | (2 if result==0 else 0) | (0x80 if result&0x8000 else 0) | (0x40 if (a^b)&(a^binary)&0x8000 else 0)
    # Independent integer-decimal cross-check for the valid-BCD subset.
    if all(((v>>s)&15)<10 for v in (a,b) for s in (0,4,8,12)):
        decimal=lambda v: sum(((v>>(4*n))&15)*10**n for n in range(4))
        value=(decimal(a)-decimal(b)-(1-c))%10000
        encoded=sum(int(digit)<<(4*n) for n,digit in enumerate(str(value).zfill(4)[::-1]))
        assert result==encoded
    if case==(0xab1d,0xf1e2,1):assert (result,p)==(0x59db,0x1c)
    return result,p

def evaluate(path,rom,cases):
    lines=path.read_text().splitlines();assert lines[0]==f'#VELOCE 1 core=SNES rom_crc32={zlib.crc32(rom):08x} channels=port,blargg6000,stp,sram,spcport'
    assert lines[1:3]==['VELOCE 1 snes decimal_sbc',f'INFO cases={len(cases)}'];assert len(lines)==len(cases)+5
    assert lines[-2]=='LOG observations_complete' and re.fullmatch(r'#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1',lines[-1])
    failures=[]
    for n,case in enumerate(cases):
        m=re.fullmatch(r'REG id=(\d{4}) a=([0-9a-f]{4}) p=([0-9a-f]{2})',lines[n+3]);assert m and int(m[1])==n
        got=(int(m[2],16),int(m[3],16));want=expected(case)
        if got!=want:failures.append({'id':n,'input':case,'got':got,'expected':want})
    return {'status':'fail' if failures else 'pass','cases':len(cases),'mismatches':failures}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--emulator',type=Path,required=True);ap.add_argument('--artifacts',type=Path,required=True);a=ap.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    for name,mutation in [('decimal_sbc',False),('negative',True)]:
        rom,cases=build(mutation);image=a.artifacts/(name+'.sfc');image.write_bytes(rom);out=image.with_suffix('.result');env=os.environ.copy()
        for k in ('DEBUG','TRACE','TRACE_FILE','INPUT','SAVE_SCREENSHOT','VELOCE_TEST_RESETS'):env.pop(k,None)
        env.update(HEADLESS='1',FRAMES='6000',VELOCE_TEST_OUT=str(out.resolve()),VELOCE_TEST_EXIT='1')
        p=subprocess.run([str(a.emulator.resolve()),str(image.resolve())],env=env,cwd=Path(__file__).resolve().parents[4],capture_output=True,text=True,timeout=180)
        assert p.returncode==0,p.stdout+p.stderr
        report=evaluate(out,rom,cases);out.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
        assert report['status']==('fail' if mutation else 'pass'),report
        if mutation: assert len(report['mismatches'])==1 and report['mismatches'][0]['id']==8192,report
    rom,cases=build();original=(a.artifacts/'decimal_sbc.result').read_text();controls={}
    for name,edit in [('missing',lambda lines:lines[:3]+lines[4:]),
                      ('reordered',lambda lines:lines[:3]+[lines[4],lines[3]]+lines[5:]),
                      ('truncated',lambda lines:lines[:-1])]:
        path=a.artifacts/(name+'.result');path.write_text('\n'.join(edit(original.splitlines()))+'\n')
        try: evaluate(path,rom,cases)
        except AssertionError: controls[name]='measurement_error'
        else: raise AssertionError('damaged capture accepted: '+name)
    (a.artifacts/'controls.json').write_text(json.dumps(controls,indent=2)+'\n')
    print('8193 original decimal SBC pairs PASS; executed capture-bit mutant and damaged streams rejected')
if __name__=='__main__':main()
