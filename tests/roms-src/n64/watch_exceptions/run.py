#!/usr/bin/env python3
"""Independent physical-watch and suppressed-access oracle for actual guest RAM."""
import argparse,hashlib,json,struct,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
SOURCE=1234605616436508552
ROW=struct.Struct('>14Q')
def require(ok,message):
    if not ok:raise ValueError(message)
def inputs(short=False):
    for physical in range(0,2048 if short else 8388608,8):
        for op in (32,43):
            for placement in range(2):yield op,physical,physical|3,128,placement,True,False
    for op in (36,40):
        for flags in range(4):
            for match in (True,False):
                for exl in (False,True):
                    for lane in range(8):
                        for placement in range(2):yield op,6291456+lane,(6291456 if match else 6291464)|flags,128+(2 if exl else 0),placement,bool(match and not exl and flags&(2 if op==36 else 1)),True
def cases(short=False):return (256 if short else 1048576)*4+512
def evaluate(path,rom,short=False):
    report=dict(status='error',cases=cases(short),observed=0,mismatches=0,examples=[])
    try:
        with path.open('rb') as f:
            require(f.read(32)==b'V64WTCH1'+struct.pack('>3Q',cases(short),14,zlib.crc32(rom)),'identity/domain mismatch')
            for sequence,data in enumerate(inputs(short),1):
                op,physical,watchlo,status,placement,trap,control=data;raw=f.read(112);require(len(raw)==112,'incomplete RAM row');actual=list(ROW.unpack(raw));tag=0xffffffff00000000|(op<<26)|(4<<21)|(19<<16)
                require(actual[:5]==[tag,physical,watchlo,status,placement] and actual[-1]==sequence,'input/order mismatch')
                cause=92 if not placement else 0xffffffff8000005c
                result=90 if control and op==36 and not trap else SOURCE
                readback=(136 if op==40 and not trap else 90) if control else 0
                want=[int(trap),cause if trap else 0,0xffffffffa0030000 if trap else 0,130 if trap else 0,128 if trap else status,result,watchlo,readback]
                report['observed']+=1
                if actual[5:13]!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8:report['examples'].append(dict(sequence=sequence,actual=actual[5:13],expected=want))
            raw=f.read(24);require(len(raw)==24 and raw[:8]==b'WTCHDONE','missing completion');frames,attempts=struct.unpack('>2Q',raw[8:]);require(frames==cases(short)+2 and attempts>frames,'completion counters');require(not f.read(),'trailing bytes')
        report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError) as e:report['detail']=str(e)
    return report
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
    return h.hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    require(build()==(HERE/'watch_exceptions.z64').read_bytes(),'archive differs from original source')
    fingerprints={str(p.resolve()):sha(p) for p in (a.capture,a.core,HERE/'watch_exceptions.z64')}
    def execute(name,short=False,mutant=False):
        rom=build(short,mutant);image=a.artifacts/(name+'.z64');image.write_bytes(rom);out=a.artifacts/(name+'.registers');r=subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(image.resolve()),str(out.resolve()),str(cases(short))],capture_output=True,text=True,timeout=1200);(a.artifacts/(name+'.log')).write_text(r.stdout+r.stderr);require(r.returncode==0,r.stdout+r.stderr);report=evaluate(out,rom,short);report['register_sha256']=sha(out);(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,out
    baseline,_=execute('watch');require(baseline['status']=='pass',json.dumps(baseline))
    mutant,_=execute('executed-cop1',True,True);require(mutant['status']=='fail' and mutant['mismatches']==cases(True),'executed wrong-exception control escaped')
    short,path=execute('short',True);require(short['status']=='pass',json.dumps(short));raw=path.read_bytes();rom=build(True)
    for field in range(5,13):
        changed=bytearray(raw);changed[32+field*8+7]^=1;out=a.artifacts/f'field-{field}.registers';out.write_bytes(changed);report=evaluate(out,rom,True);require(report['status']=='fail' and report['mismatches']==1,'ungraded output field')
    for name,changed in [('identity',b'X'+raw[1:]),('missing-row',raw[:32]+raw[144:]),('completion',raw[:-24]),('trailing',raw+b'X'),('order',raw[:32]+raw[144:256]+raw[32:144]+raw[256:]),('short-footer',raw[:-1])]:
        out=a.artifacts/(name+'.registers');out.write_bytes(changed);require(evaluate(out,rom,True)['status']=='error','invalid stream accepted '+name)
    require(all(sha(Path(path))==value for path,value in fingerprints.items()),'executable/archive changed during observations');(a.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n')
    print('4,194,816 Watch records / 58,727,424 RAM/CP0 words PASS; executed wrong exception, eight fields and six malformed streams rejected')
if __name__=='__main__':main()
