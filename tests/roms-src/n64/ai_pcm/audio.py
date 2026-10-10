#!/usr/bin/env python3
"""Independent oracle for production-plugin PCM values recorded to register files."""
import argparse,hashlib,json,re,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def require(ok,message):
    if not ok:raise ValueError(message)
def evaluate(path,rom):
    report=dict(status="error",expected_codes=65536,distinct_codes=0,observed_pairs=0,mismatches=0,rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        previous=None;repetitions=0;seen=set()
        with path.open(encoding="ascii") as f:
            require(f.readline()==f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=abi-audio\n","wrong identity")
            for line in f:
                if line.startswith("LOG "):
                    require(line==f"LOG complete pairs={report['observed_pairs']}\n","wrong count")
                    require(f.readline()=="END observations_complete\n" and not f.read(),"missing END/trailing data");break
                require(re.fullmatch(r"REG r=[0-9a-f]{8}\n",line),"malformed PCM pair")
                left,right=int(line[6:10],16),int(line[10:14],16)
                report['observed_pairs']+=1
                if right!=65535-left:report['mismatches']+=1
                if previous is None:
                    if left!=0:report['mismatches']+=1
                elif left==previous:
                    repetitions+=1
                    if repetitions>1:report['mismatches']+=1
                elif left==previous+1:repetitions=0
                else:report['mismatches']+=1
                previous=left;seen.add(left)
            else:raise ValueError("missing completion")
        report['distinct_codes']=len(seen)
        if previous!=65535 or len(seen)!=65536:report['mismatches']+=1
        # Fixed 48 kHz host output versus NTSC crystal / 1024 source rate.
        # This bounds phase rounding; it does not certify physical CPU/DAC timing.
        ideal=65536*48000*1024//48681818
        if abs(report['observed_pairs']-ideal)>1:report['mismatches']+=1
        report['status']='fail' if report['mismatches'] else 'pass'
    except (ValueError,OSError,UnicodeError) as e:report['detail']=str(e)
    return report
def main():
    p=argparse.ArgumentParser();p.add_argument('--capture',type=Path,default=ROOT/'build/bin/n64_audio_capture');p.add_argument('--core',type=Path,default=ROOT/'build/bin/cores/n64.so');p.add_argument('--artifacts',type=Path,default=ROOT/'build/artifacts/owned/n64_ai_pcm_audio');a=p.parse_args();a.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,content=None):
        image=a.artifacts/(name+'.z64');path=a.artifacts/(name+'.result')
        if content is None:
            image.write_bytes(rom);r=subprocess.run([str(a.capture.resolve()),str(a.core.resolve()),str(image.resolve()),str(path.resolve())],capture_output=True,text=True,timeout=180);require(r.returncode==0,r.stdout+r.stderr)
        else:path.write_text(content)
        report=evaluate(path,rom);(a.artifacts/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n');return report,path
    rom=build();require(rom==(HERE/'ai_pcm.z64').read_bytes(),'archived image differs')
    good,path=grade('pcm',rom);require(good['status']=='pass',json.dumps(good))
    bad,_=grade('executed-sample-mutant',build(wrong_sample=True));require(bad['status']=='fail' and bad['mismatches']==bad['observed_pairs'],'sample mutant escaped')
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith('REG r='))
    changed=row[:-1]+('1' if row[-1]=='0' else '0')
    bad,_=grade('channel-field',rom,content.replace(row,changed,1));require(bad['status']=='fail' and bad['mismatches']==1,'channel field escaped')
    for name,broken in [('missing-row',content.replace(row+'\n','',1)),('malformed',content.replace(row,'REG r=bad',1)),('wrong-identity',content.replace('core=N64','core=bad',1)),('missing-end',content.replace('END observations_complete\n','')),('trailing',content+'extra\n')]:
        bad,_=grade(name,rom,broken);require(bad['status']=='error',name+' escaped')
    print(f"N64 PCM ABI: PASS {good['observed_pairs']} pairs; all {good['distinct_codes']} signed codes per channel; executed sample mutation and field/stream controls rejected")
    return 0
if __name__=='__main__':raise SystemExit(main())
