#!/usr/bin/env python3
"""Independent disabled-COP2 Cause/EPC/operand oracle for native RAM files."""
import argparse,hashlib,json,struct,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
VALUES=(0,1,2,32767,32768,2147483647,2147483648,4294967295,4294967296,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551614,6148914691236517205,12297829382473034410,18364758544493064720)
ROW=struct.Struct('>12Q')
def require(ok,message):
    if not ok:raise ValueError(message)
def inputs(short=False):
    offsets=range(0,65536,4369) if short else range(65536)
    for op in (50,54,58,62):
        for immediate in offsets:
            for address in VALUES:
                for placement in range(2):yield 0xffffffff00000000|op<<26|18<<21|19<<16|immediate,address,VALUES[-1],placement
    for rs in (0,1,2,4,5,6):
        for target in range(32):
            for cpreg in range(32):
                for placement in range(2):yield 18<<26|rs<<21|target<<16|cpreg<<11,VALUES[-1],VALUES[-1],placement
    for branch in range(4):
        for immediate in offsets:
            for placement in range(2):yield 18<<26|8<<21|branch<<16|immediate,VALUES[-1],VALUES[-1],placement
    for cofun in offsets:
        for placement in range(2):yield 18<<26|1<<25|cofun,VALUES[-1],VALUES[-1],placement
def cases(short=False):return 4*(16 if short else 65536)*16*2+6*32*32*2+5*(16 if short else 65536)*2
def evaluate(path,rom,short=False):
    report=dict(status='error',cases=cases(short),observed=0,mismatches=0,examples=[])
    try:
        with path.open('rb') as f:
            require(f.read(32)==b'V64COP21'+struct.pack('>3Q',cases(short),12,zlib.crc32(rom)),'identity/domain mismatch')
            for sequence,data in enumerate(inputs(short),1):
                ins,address,source,placement=data;raw=f.read(96);require(len(raw)==96,'incomplete RAM row');actual=list(ROW.unpack(raw))
                require(actual[:4]==list(data) and actual[-1]==sequence,'input/order mismatch')
                cause=0x2000002c if not placement else 0xffffffffa000002c
                want=[1,cause,0xffffffffa0030000,130,128,VALUES[-2],VALUES[-1]]
                report['observed']+=1
                if actual[4:11]!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<6:report['examples'].append(dict(sequence=sequence,actual=actual[4:11],expected=want))
            raw=f.read(24);require(len(raw)==24 and raw[:8]==b'COP2DONE','missing completion');frames,attempts=struct.unpack('>2Q',raw[8:]);require(frames==cases(short)+1 and attempts>frames,'completion counters');require(not f.read(),'trailing bytes')
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
    require(build()==(HERE/'cop2_unusable.z64').read_bytes(),'archive differs from original source')
    fingerprints={str(p.resolve()):sha(p) for p in (a.capture,a.core,HERE/'cop2_unusable.z64')}
    def execute(name,short=False,mutant=False):
        rom=build(short,mutant);image=a.artifacts/(name+'.z64');image.write_bytes(rom);out=a.artifacts/(name+'.registers');r=subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(image.resolve()),str(out.resolve()),str(cases(short))],capture_output=True,text=True,timeout=1200);(a.artifacts/(name+'.log')).write_text(r.stdout+r.stderr);require(r.returncode==0,r.stdout+r.stderr);report=evaluate(out,rom,short);report['register_sha256']=sha(out);(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,out
    baseline,_=execute('cop2');require(baseline['status']=='pass',json.dumps(baseline))
    mutant,_=execute('executed-cop1',True,True);require(mutant['status']=='fail' and mutant['mismatches']==cases(True),'executed wrong-coprocessor control escaped')
    short,path=execute('short',True);require(short['status']=='pass',json.dumps(short));raw=path.read_bytes();rom=build(True)
    for field in range(4,11):
        changed=bytearray(raw);changed[32+field*8+7]^=1;out=a.artifacts/f'field-{field}.registers';out.write_bytes(changed);report=evaluate(out,rom,True);require(report['status']=='fail' and report['mismatches']==1,'ungraded output field')
    for name,changed in [('identity',b'X'+raw[1:]),('missing-row',raw[:32]+raw[128:]),('completion',raw[:-24]),('trailing',raw+b'X'),('order',raw[:32]+raw[128:224]+raw[32:128]+raw[224:]),('short-footer',raw[:-1])]:
        out=a.artifacts/(name+'.registers');out.write_bytes(changed);require(evaluate(out,rom,True)['status']=='error','invalid stream accepted '+name)
    require(all(sha(Path(path))==value for path,value in fingerprints.items()),'executable/archive changed during observations');(a.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n')
    print('9,056,256 actual COP2 records / 108,675,072 RAM/CP0 words PASS; executed COP1 substitution, seven fields and six malformed streams rejected')
if __name__=='__main__':main()
