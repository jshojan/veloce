#!/usr/bin/env python3
"""Grade actual production ABI pixel bytes against independent guest-ramp arithmetic."""
import argparse,hashlib,json,re,subprocess,zlib
from pathlib import Path
from build import build
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
def require(ok,message):
    if not ok:raise ValueError(message)
def evaluate(path,rom):
    report=dict(status="error",expected_pixels=131072,observed_pixels=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding="ascii") as f:
            require(f.readline()==f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=abi-video\n","wrong identity")
            for index,mode in enumerate((2,3)):
                require(f.readline()==f"FRAME index={index} mode={mode} width=256 height=256\n","wrong frame domain")
                for n in range(65536):
                    row=f.readline();require(re.fullmatch(r"REG r=[0-9a-f]{8}\n",row),"missing/malformed pixel")
                    if mode==2:
                        channels=(n//2048%32,n//64%32,n//2%32)
                        want=tuple(c*8+c//4 for c in channels)+(255,)
                    else:want=(n%256,n//256,(n*37)%256,255)
                    got=tuple(int(row[6+c*2:8+c*2],16) for c in range(4));report["observed_pixels"]+=1
                    if got!=want:
                        report["mismatches"]+=1
                        if len(report["examples"])<8:report["examples"].append(dict(mode=mode,pixel=n,expected=want,observed=got))
            require(f.readline()=="LOG complete frames=2 pixels=131072\n","wrong completion")
            require(f.readline()=="END observations_complete\n","false END");require(not f.read(),"trailing data")
            report["status"]="fail" if report["mismatches"] else "pass"
    except (ValueError,OSError,UnicodeError) as e:report["detail"]=str(e)
    return report
def main():
    p=argparse.ArgumentParser();p.add_argument("--capture",type=Path,default=ROOT/"build/bin/n64_video_capture");p.add_argument("--core",type=Path,default=ROOT/"build/bin/cores/n64.so");p.add_argument("--artifacts",type=Path,default=ROOT/"build/artifacts/owned/n64_vi_video")
    args=p.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,content=None):
        image=args.artifacts/(name+".z64");result=args.artifacts/(name+".result")
        if content is None:
            image.write_bytes(rom);run=subprocess.run([str(args.capture.resolve()),str(args.core.resolve()),str(image.resolve()),str(result.resolve())],capture_output=True,text=True,timeout=180)
            require(run.returncode==0,run.stdout+run.stderr);result.with_suffix(".log").write_text(run.stdout+run.stderr)
        else:result.write_text(content)
        report=evaluate(result,rom);(args.artifacts/(name+".json")).write_text(json.dumps(report,indent=2)+"\n");return report,result
    rom=build();require(rom==(HERE/"vi_framebuffer.z64").read_bytes(),"archive differs")
    baseline,path=grade("video",rom);require(baseline["status"]=="pass",json.dumps(baseline))
    mutant,_=grade("executed-pixel-mutant",build(wrong_pixel=True));require(mutant["status"]=="fail" and mutant["mismatches"]==131072,"executed pixel mutant escaped")
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith("REG r="))
    for channel in range(4):
        n=6+channel*2;changed=row[:n]+format(int(row[n:n+2],16)^1,"02x")+row[n+2:]
        bad,_=grade(f"channel-{channel}",rom,content.replace(row,changed,1));require(bad["status"]=="fail" and bad["mismatches"]==1,"channel escaped")
    for name,broken in (("missing-pixel",content.replace(row+"\n","",1)),("wrong-domain",content.replace("width=256","width=257",1)),("bad-row",content.replace(row,"REG r=bad",1)),("wrong-count",content.replace("pixels=131072","pixels=131073")),("false-end",content.replace("END observations_complete","END 0/0")),("trailing",content+"extra\n")):
        bad,_=grade(name,rom,broken);require(bad["status"]=="error",name+" accepted")
    print("N64 ABI video: PASS 131072/131072 RGBA16/RGBA32 pixels; executed pixel mutant, 4 channel and 6 framing controls rejected")
    return 0
if __name__=="__main__":raise SystemExit(main())
