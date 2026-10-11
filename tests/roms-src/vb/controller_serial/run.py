#!/usr/bin/env python3
"""Independently grade actual software-clocked Virtual Boy controller RAM words."""
import argparse,hashlib,json,os,re,subprocess,tempfile
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def require(ok,why):
    if not ok:raise ValueError(why)
def expected(sample):
    wire=2
    for n,bit in enumerate((2,3,5,4,12,13,11,10,9,8,6,15,14,7)):
        if sample&(1<<n):wire+=1<<bit
    return (wire%256,wire//256,205,wire%256,wire//256,205,0,2309737967)
def grade(path,domain,cases):
    content=path.read_text();lines=content.splitlines()
    require(content.endswith('\n') and len(lines)==cases+1 and lines[0]==f'VELOCE VB CONTROLLER 1 domain={domain} cases={cases} words=8','invalid controller stream')
    bad=0
    for sample,line in enumerate(lines[1:]):
        require(re.fullmatch(r'REG case='+str(sample)+r' r=[0-9a-f]{64}',line),'invalid controller row')
        words=tuple(int(line.split(' r=')[1][n:n+8],16) for n in range(0,64,8))
        bad+=sum(a!=b for a,b in zip(words,expected(sample)))
    require(not bad,'controller register oracle mismatch')
def main():
    p=argparse.ArgumentParser();p.add_argument('capture',type=Path);p.add_argument('core',type=Path);p.add_argument('artifacts',type=Path);a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    paths=[a.capture,a.core,HERE/'build.py',HERE/'recipe.json',HERE/'controller_serial.vb',Path(__file__)]+[ROOT/name for name in ('cores/vb/src/machine.cpp','cores/vb/src/machine.hpp','cores/vb/src/v810.cpp','cores/vb/src/v810.hpp','cores/vb/src/plugin.cpp','cores/vb/src/controller.hpp','tests/cpp/vb_controller_capture.cpp')]
    hashes={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    require(build()==(HERE/'controller_serial.vb').read_bytes(),'owned archive differs')
    env=os.environ.copy();env['SDL_VIDEODRIVER']='dummy'
    def capture(rom,result,cases,domain):
        run=subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(rom.resolve()),str(result.resolve()),str(cases),domain],env=env,capture_output=True,text=True,timeout=90)
        require(run.returncode==0,run.stdout+run.stderr)
    with tempfile.TemporaryDirectory(prefix='vb-pad-') as temporary:
        temporary=Path(temporary)
        for width in (1,2,4):
            for alias in range(4):
                domain=f'w{width}-a{alias}';rom=temporary/(domain+'.vb');rom.write_bytes(build(width,alias));result=a.artifacts/(domain+'.result');capture(rom,result,16384,domain);grade(result,domain,16384)
        for name,option in (('wrong-capture','wrong_capture'),('missing-clock','missing_clock')):
            rom=temporary/(name+'.vb');rom.write_bytes(build(**{option:True}));result=a.artifacts/(name+'.result');capture(rom,result,16,name)
            try:grade(result,name,16)
            except ValueError as e:require(str(e)=='controller register oracle mismatch','fault rejected for wrong reason')
            else:raise ValueError('executed faulty guest escaped')
            rows=result.read_text().splitlines()[1:];mismatches=0
            for sample,line in enumerate(rows):
                words=tuple(int(line.split(' r=')[1][n:n+8],16) for n in range(0,64,8));mismatches+=sum(x!=y for x,y in zip(words,expected(sample)))
            require(mismatches==(16 if name=='wrong-capture' else 48),'faulty guest domain changed')
        baseline=a.artifacts/'w1-a0.result';text=baseline.read_text();rows=text.splitlines()
        for field in range(8):
            altered=rows.copy();words=[altered[1].split(' r=')[1][n:n+8] for n in range(0,64,8)];words[field]=f'{int(words[field],16)^1:08x}';altered[1]='REG case=0 r='+''.join(words);result=a.artifacts/f'field-{field}.result';result.write_text('\n'.join(altered)+'\n')
            try:grade(result,'w1-a0',16384)
            except ValueError:pass
            else:raise ValueError('field mutation escaped')
        for name,content in (('truncated',text.rsplit('\n',2)[0]+'\n'),('header',text.replace('words=8','words=7')),('trailing',text+'extra\n'),('unterminated',text.rstrip('\n'))):
            result=a.artifacts/f'stream-{name}.result';result.write_text(content)
            try:grade(result,'w1-a0',16384)
            except ValueError:pass
            else:raise ValueError('invalid register stream escaped')
    require(all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest for p,digest in hashes.items()),'capture input changed')
    (a.artifacts/'controller.json').write_text(json.dumps({'status':'pass','snapshots':196608,'actual_32bit_words':1572864,'button_combinations':16384,'widths':[1,2,4],'mirrors':4,'executed_wrong_capture_rejected':16,'executed_missing_clock_rejected_words':48,'field_mutations_rejected':8,'invalid_streams_rejected':4,'input_sha256':hashes,'hardware_polling_timing':False,'partial_data_hardware_score':False},indent=2)+'\n')
    print('196608 native VB software controller snapshots / 1572864 actual RAM words PASS')
if __name__=='__main__':main()
