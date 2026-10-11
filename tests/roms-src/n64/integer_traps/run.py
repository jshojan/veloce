#!/usr/bin/env python3
"""Independent arithmetic/exception grading of actual native trap RAM files."""
import argparse,hashlib,itertools,json,struct,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
U64=(1<<64)-1
SOURCES=(0,1,2,32767,32768,2147483647,2147483648,4294967295,4294967296,9223372036854775807,9223372036854775808,18446744073709551615,18446744073709551614,6148914691236517205,12297829382473034410,18364758544493064720)
REGISTER_CODES=(48,49,50,51,52,54)
IMMEDIATE_CODES=(8,9,10,11,12,14)
ROW=struct.Struct('>12Q')

def require(ok,message):
    if not ok:raise ValueError(message)
def signed(value):return value-(1<<64) if value>=1<<63 else value
def relation(kind,a,b):
    return (signed(a)>=signed(b),a>=b,signed(a)<signed(b),a<b,a==b,a!=b)[kind]
def cases(short=False):return 6*(4 if short else 1024)*16*16*2+6*(16 if short else 65536)*16*2

def inputs(short=False):
    for kind in range(6):
        for code in range(4 if short else 1024):
            for a,b,placement in itertools.product(SOURCES,SOURCES,range(2)):
                yield ((18<<21)|(19<<16)|(code<<6)|REGISTER_CODES[kind],a,b,placement,relation(kind,a,b))
    for kind in range(6):
        for imm in (range(0,65536,4369) if short else range(65536)):
            right=(imm if imm<32768 else imm-65536)&U64
            for a,placement in itertools.product(SOURCES,range(2)):
                yield ((1<<26)|(18<<21)|(IMMEDIATE_CODES[kind]<<16)|imm,a,SOURCES[-1],placement,relation(kind,a,right))

def expected(short=False):
    for index,(tag,a,b,placement,trap) in enumerate(inputs(short)):
        cause=((0xffffffff80000000 if placement else 0)|52) if trap else 0
        yield (tag,a,b,placement,int(trap),cause,0xffffffffa0030000 if trap else 0,130 if trap else 0,128,SOURCES[-2],SOURCES[-1],index+1)

def evaluate(path,rom,short=False):
    count=cases(short);report=dict(status='error',expected_cases=count,observed_cases=0,graded_words=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest())
    try:
        require(path.stat().st_size==32+count*ROW.size+24,'wrong stream length/termination')
        with path.open('rb') as file:
            require(file.read(8)==b'V64TRAP1','wrong stream magic');header=struct.unpack('>3Q',file.read(24))
            require(header==(count,12,zlib.crc32(rom)),'wrong ROM identity or domain')
            for index,want in enumerate(expected(short)):
                actual=ROW.unpack(file.read(ROW.size));report['observed_cases']+=1;report['graded_words']+=12
                if actual!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8:report['examples'].append(dict(index=index,actual=list(actual),expected=list(want)))
            require(file.read(8)==b'TRAPDONE','missing actual guest completion');frames,instructions=struct.unpack('>2Q',file.read(16))
            require(frames==count+1 and instructions>count and not file.read(),'wrong trailer or trailing data')
        report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,struct.error) as error:report['detail']=str(error)
    return report

def sha_file(path):
    digest=hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()

def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);p.add_argument('--short-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    archive=HERE/'integer_traps.z64';require(build()==archive.read_bytes(),'archive differs from generator')
    fingerprints={str(x.resolve()):hashlib.sha256(x.read_bytes()).hexdigest() for x in (a.capture,a.core,archive)}
    def execute(name,rom,short):
        image=a.artifacts/(name+'.z64');result=a.artifacts/(name+'.registers');image.write_bytes(rom)
        run=subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(image.resolve()),str(result.resolve()),str(cases(short))],capture_output=True,text=True,timeout=1200)
        (a.artifacts/(name+'.log')).write_text(run.stdout+run.stderr);require(run.returncode==0,run.stdout+run.stderr)
        report=evaluate(result,rom,short);report['register_sha256']=sha_file(result);(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,result
    if not a.short_only:
        baseline,_=execute('integer_traps',build(),False);require(baseline['status']=='pass',json.dumps(baseline))
    control,path=execute('control',build(short=True),True);require(control['status']=='pass',json.dumps(control))
    mutant,_=execute('executed-opposite',build(short=True,wrong_condition=True),True);require(mutant['status']=='fail' and mutant['mismatches']>0,'executed mutant escaped')
    content=path.read_bytes();first=ROW.unpack(content[32:32+ROW.size])
    for field in range(12):
        changed=list(first);changed[field]^=1;broken=content[:32]+ROW.pack(*changed)+content[32+ROW.size:];out=a.artifacts/f'field-{field}.registers';out.write_bytes(broken)
        report=evaluate(out,build(short=True),True);require(report['status']=='fail' and report['mismatches']==1,'ungraded output field')
    for name,broken in [('missing-row',content[:-96]),('truncated',content[:-1]),('trailing',content+b'!'),('identity',content[:31]+bytes([content[31]^1])+content[32:]),('domain',content[:15]+bytes([content[15]^1])+content[16:]),('false-footer',content[:-24]+b'FAKEPASS'+content[-16:]),('order',content[:32]+content[128:224]+content[32:128]+content[224:])]:
        out=a.artifacts/(name+'.registers');out.write_bytes(broken);report=evaluate(out,build(short=True),True)
        require(report['status']==('fail' if name=='order' else 'error'),'damaged stream accepted: '+name)
    require(all(hashlib.sha256(Path(x).read_bytes()).hexdigest()==sha for x,sha in fingerprints.items()),'executables/archive changed during observation')
    (a.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n')
    print(f"N64 integer traps PASS: {cases(a.short_only)} cases / {cases(a.short_only)*12} actual words; executed opposite-condition, twelve fields and seven stream controls rejected")
if __name__=='__main__':main()
