#!/usr/bin/env python3
"""Independent grading of actual plugin RAM; no guest-computed expected answers."""
import argparse,hashlib,json,struct,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
MASK=(1<<64)-1
VALUES=(0,9223372034707292160,9223372041149743103,18364758544493064720)
ROW=struct.Struct('>10Q')
def require(ok,message):
    if not ok:raise ValueError(message)
def cases(short=False):return 2*(16 if short else 65536)*4*2
def sign_word(x):return x+(0xffffffff00000000 if x>=0x80000000 else 0)
def expected(short=False):
    sequence=0
    for width in range(2):
        for offset in (range(0,65536,4369) if short else range(65536)):
            for initial in VALUES:
                replacement=MASK-initial;loaded=initial if width else sign_word(initial//(1<<32))
                for failure in range(2):
                    sequence+=1;stored=loaded if failure else (replacement if width else sign_word(replacement%(1<<32)))
                    yield [sequence,width,offset,initial,replacement,failure,loaded,1-failure,stored,2048]
def evaluate(path,rom,short=False):
    result=dict(status='error',cases=cases(short),observed=0,mismatches=0,examples=[])
    try:
        with path.open('rb') as f:
            require(f.read(32)==b'V64LINK1'+struct.pack('>3Q',cases(short),10,zlib.crc32(rom)),'identity/domain mismatch')
            for want in expected(short):
                row=f.read(80);require(len(row)==80,'missing/short RAM record');actual=list(ROW.unpack(row))
                require(actual[:6]==want[:6],'input/sequence/domain mismatch');result['observed']+=1
                if actual[6:]!=want[6:]:
                    result['mismatches']+=1
                    if len(result['examples'])<6:result['examples'].append(dict(actual=actual,expected=want))
            footer=f.read(24);require(len(footer)==24 and footer[:8]==b'LINKDONE','completion missing')
            frames,attempts=struct.unpack('>2Q',footer[8:]);require(frames==cases(short)+2 and attempts>frames,'actual completion counters')
            require(not f.read(),'trailing bytes')
        result['status']='fail' if result['mismatches'] else 'pass'
    except (ValueError,OSError) as e:result['detail']=str(e)
    return result
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
    return h.hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    require(build()==(HERE/'linked_memory.z64').read_bytes(),'archive differs from original source')
    fingerprints={str(x.resolve()):sha(x) for x in (a.capture,a.core,HERE/'linked_memory.z64')}
    def execute(name,short=False,mutant=False):
        rom=build(short,mutant);image=a.artifacts/(name+'.z64');image.write_bytes(rom);out=a.artifacts/(name+'.registers')
        r=subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(image.resolve()),str(out.resolve()),str(cases(short))],capture_output=True,text=True,timeout=900)
        (a.artifacts/(name+'.log')).write_text(r.stdout+r.stderr);require(r.returncode==0,r.stdout+r.stderr)
        report=evaluate(out,rom,short);report['register_sha256']=sha(out);(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,out
    baseline,path=execute('linked-memory');require(baseline['status']=='pass',json.dumps(baseline))
    mutant,_=execute('executed-omitted-eret',True,True);require(mutant['status']=='fail' and mutant['mismatches']==128,'executed missing-ERET control escaped')
    # Use a short real successful file for field/transport fault controls.
    short,path=execute('short',True);require(short['status']=='pass',json.dumps(short));raw=path.read_bytes();rom=build(True)
    for field in range(6,10):
        changed=bytearray(raw);changed[32+field*8+7]^=1;out=a.artifacts/f'field-{field}.registers';out.write_bytes(changed);report=evaluate(out,rom,True);require(report['status']=='fail' and report['mismatches']==1,'ungraded actual field')
    for name,changed in [('identity',b'X'+raw[1:]),('missing-row',raw[:32]+raw[112:]),('completion',raw[:-24]),('trailing',raw+b'X'),('order',raw[:32]+raw[112:192]+raw[32:112]+raw[192:]),('short-footer',raw[:-1])]:
        out=a.artifacts/(name+'.registers');out.write_bytes(changed);require(evaluate(out,rom,True)['status']=='error','malformed stream accepted '+name)
    require(all(sha(Path(path))==digest for path,digest in fingerprints.items()),'executables/archive changed during capture')
    (a.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n')
    print('1,048,576 actual linked-memory RAM records / 10,485,760 words PASS; executed missing ERET, four output fields and six malformed streams rejected')
if __name__=='__main__':main()
