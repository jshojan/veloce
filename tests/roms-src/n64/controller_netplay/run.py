#!/usr/bin/env python3
"""Strict grading of CPU-produced four-port SI/PIF observations."""
import argparse,json,hashlib,re,subprocess,zlib
from pathlib import Path
from build import build
from oracle import expected
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def require(ok,text):
    if not ok:raise ValueError(text)
def evaluate(path,rom,count):
    report=dict(status='error',expected_cases=count,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest())
    try:
        with path.open(encoding='ascii') as f:
            require(f.readline()==f'#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n','wrong identity')
            require(f.readline()=='VELOCE 1 n64 controller_netplay\n','wrong suite')
            require(f.readline()==f'BEGIN data cases={count} words=6\n','wrong domain')
            for raw in range(count):
                row=f.readline();require(re.fullmatch(r'REG r=[0-9a-f]{96}\n',row),'malformed/missing row')
                got=tuple(int(row[n:n+16],16) for n in range(6,102,16));want=expected(raw);report['graded_cases']+=1
                if got!=want:
                    report['mismatches']+=1
                    if len(report['examples'])<8:report['examples'].append(dict(index=raw,expected=want,observed=got))
            require(f.readline()==f'LOG complete cases={count}\n','missing completion')
            require(f.readline()=='END observations_complete\n','false END')
            require(re.fullmatch(r'#VELOCE end reason=terminator frames='+str(count)+r' cycles=\d+ status=-1\n',f.readline()),'invalid trailer')
            require(not f.read(),'trailing data');report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report
def main():
    p=argparse.ArgumentParser();p.add_argument('--probe',type=Path,default=ROOT/'build/bin/n64_controller_probe');p.add_argument('--core',type=Path,default=ROOT/'build/bin/cores/n64.so');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/n64_controller_netplay');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,count,content=None):
        image=a.artifacts/(name+'.z64');out=a.artifacts/(name+'.result')
        if content is None:
            image.write_bytes(rom);r=subprocess.run([str(a.probe.resolve()),str(a.core.resolve()),str(image.resolve()),str(out.resolve()),str(count)],capture_output=True,text=True,timeout=180);require(r.returncode==0,r.stdout+r.stderr)
        else:out.write_text(content)
        result=evaluate(out,rom,count);(a.artifacts/(name+'.json')).write_text(json.dumps(result,indent=2)+'\n');return result,out
    rom=build();require(rom==(HERE/'controller_netplay.z64').read_bytes(),'archive differs')
    good,_=grade('controller_netplay',rom,16384);require(good['status']=='pass',json.dumps(good))
    short=build(short=True);control,path=grade('control',short,16);require(control['status']=='pass',json.dumps(control))
    bad,_=grade('executed-mutant',build(short=True,wrong_capture=True),16);require(bad['status']=='fail' and bad['mismatches']==16,'executed mutant escaped')
    content=path.read_text();row=next(x for x in content.splitlines() if x.startswith('REG r='))
    for field in range(6):
        at=6+16*field;changed=row[:at]+format(int(row[at:at+16],16)^1,'016x')+row[at+16:];r,_=grade(f'field-{field}',short,16,content.replace(row,changed,1));require(r['status']=='fail' and r['mismatches']==1,'field escaped')
    for name,broken in [('missing-row',content.replace(row+'\n','',1)),('bad-row',content.replace(row,'REG r=bad',1)),('wrong-domain',content.replace('cases=16 words=6','cases=17 words=6')),('false-END',content.replace('END observations_complete','END 0/0')),('missing-trailer',content[:content.index('#VELOCE end ')]),('unterminated',content.rstrip('\n')),('trailing',content+'extra\n')]:
        r,_=grade(name,short,16,broken);require(r['status']=='error',name+' accepted')
    print('N64 controller guest: 16384 cases / 98304 words PASS; executed, six field and seven stream controls rejected')
if __name__=='__main__':main()
