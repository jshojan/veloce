#!/usr/bin/env python3
"""Independent mode/exception oracle for actual original guest RAM records."""
import argparse,hashlib,json,struct,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROW=struct.Struct('>12Q')
def require(ok,why):
    if not ok:raise ValueError(why)
def tags(short=False):
    for fn in (20,22,23,28,29,30,31,44,45,46,47,56,58,59,60,62,63):
        shifts=range(32) if fn in (56,58,59,60,62,63) else (0,)
        for shift in shifts:yield ((4 if fn<56 else 0)<<21)|(5<<16)|((0 if fn in (28,29,30,31) else 6)<<11)|(shift<<6)|fn
    for op in (24,25,26,27,39,44,45,52,55,60,63):
        for imm in (range(0,65536,4369) if short else range(65536)):
            word=(op<<26)|(4<<21)|(5<<16)|imm
            yield word|((0xffffffff<<32) if word>=1<<31 else 0)
def count(short=False):return (203+11*(16 if short else 65536))*8
def expected(short=False):
    seq=0
    for tag in tags(short):
        for status in (0x10000008,0x100000a8,0x10000010,0x100000d0):
            for delay in range(2):
                seq+=1
                yield (tag,status,delay,1,(0xffffffff80000000 if delay else 0)|40,0x30000,status|2,128,0x1122334455667788,0x5555555555555555,0xaaaaaaaaaaaaaaaa,seq)
def evaluate(path,rom,short=False):
    cases=count(short);report={'status':'error','expected_cases':cases,'observed_cases':0,'graded_words':0,'mismatches':0,'examples':[],'rom_sha256':hashlib.sha256(rom).hexdigest()}
    try:
        require(path.stat().st_size==32+cases*96+24,'wrong stream length')
        with path.open('rb') as file:
            require(file.read(8)==b'V64TRAP1' and struct.unpack('>3Q',file.read(24))==(cases,12,zlib.crc32(rom)),'wrong ROM identity/domain')
            for index,want in enumerate(expected(short)):
                got=ROW.unpack(file.read(96));require(got[-1]==index+1,'out-of-order record');report['observed_cases']+=1;report['graded_words']+=12
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8:report['examples'].append({'index':index,'actual':got,'expected':want})
            require(file.read(8)==b'TRAPDONE','missing actual completion');frames,attempts=struct.unpack('>2Q',file.read(16))
            require(frames==cases+1 and attempts>cases and not file.read(),'wrong completion trailer')
        report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,struct.error) as e:report['detail']=str(e)
    return report
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);p.add_argument('--short-only',action='store_true');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    inputs=(a.capture,a.core,HERE/'build.py',HERE/'run.py',HERE/'doubleword_privilege.z64');fingerprints={str(path.resolve()):sha(path) for path in inputs}
    require(build()==(HERE/'doubleword_privilege.z64').read_bytes(),'archive does not reproduce')
    def run(name,rom,short):
        cartridge=a.artifacts/(name+'.z64');cartridge.write_bytes(rom);capture=a.artifacts/(name+'.registers')
        subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(cartridge.resolve()),str(capture.resolve()),str(count(short))],check=True)
        report=evaluate(capture,rom,short);(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,capture
    if not a.short_only:
        report,_=run('doubleword_privilege',build(),False);require(report['status']=='pass','full native corpus failed: '+str(report))
    rom=build(True);report,path=run('short',rom,True);require(report['status']=='pass','short native corpus failed: '+str(report));raw=path.read_bytes()
    report,_=run('executed-nop',build(True,True),True);require(report['status']=='fail' and report['mismatches']==count(True),'executed wrong instruction not rejected')
    for field in range(12):
        bad=bytearray(raw);bad[32+field*8+7]^=1;out=a.artifacts/f'field-{field}.registers';out.write_bytes(bad);r=evaluate(out,rom,True);require((r['status']=='error') if field==11 else (r['status']=='fail' and r['mismatches']==1),'ungraded field')
    for name,bad in [('identity',b'X'+raw[1:]),('missing-row',raw[:32]+raw[128:]),('completion',raw[:-24]),('trailing',raw+b'X'),('order',raw[:32]+raw[128:224]+raw[32:128]+raw[224:]),('short-footer',raw[:-1])]:
        out=a.artifacts/(name+'.registers');out.write_bytes(bad);require(evaluate(out,rom,True)['status']=='error','malformed stream accepted '+name)
    require(all(sha(Path(path))==h for path,h in fingerprints.items()),'binary/source/archive changed during execution')
    (a.artifacts/'provenance.json').write_text(json.dumps(fingerprints,indent=2)+'\n')
    print(f'{count(a.short_only):,} doubleword privilege records; executed NOP, 12 fields, six malformed streams and native checkpoint/rollback controls PASS')
if __name__=='__main__':main()
