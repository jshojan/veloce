#!/usr/bin/env python3
"""Independent arithmetic grading of actual PI/handoff register streams."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build
SUITE="ipl3_handoff"
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]

def require(ok,message):
    if not ok:raise ValueError(message)

def expected(rom,short=False):
    if SUITE=="ipl3_handoff":
        yield (0,1,0,63,1,0xffffffffa4001ff0,0xffffffffa4001550,0x34000000,64,18,7,3)
        return
    base=len(rom)-196608
    # Independent arithmetic byte oracle; no CPU or builder data helpers used.
    def half(offset):
        def byte(n):return (91+37*(n%256)+11*(n//256))%256
        return byte(offset)*256+byte(offset+1)
    for offset in (0,2,65534,65536):
        for dram in (0x700000,0x700008):
            for mi in (0,1):
                for length in range(2,33 if short else 65537,2):
                    cart=0x10000000+base+offset
                    yield (half(offset),half(offset+length-2),0xa55a,0xa55a,(dram+length+7)//8*8,cart+length,
                           127,8,16,mi*16,mi*1024,0,0,0,length,cart)

def evaluate(path,rom,short=False):
    cases=1 if SUITE=="ipl3_handoff" else 256 if short else 524288
    words=12 if SUITE=="ipl3_handoff" else 16
    width=16 if SUITE=="ipl3_handoff" else 8
    report=dict(status="error",suite=SUITE,expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding="ascii") as f:
            require(f.readline()==f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n","wrong identity")
            require(f.readline()==f"VELOCE 1 n64 {SUITE}\n","wrong suite")
            require(f.readline()==f"BEGIN {'cold' if SUITE=='ipl3_handoff' else 'aligned'} cases={cases} words={words}\n","wrong domain")
            for index,want in enumerate(expected(rom,short)):
                row=f.readline();require(re.fullmatch(r"REG r=[0-9a-f]{"+str(words*width)+r"}\n",row),"missing/malformed row")
                got=tuple(int(row[n:n+width],16) for n in range(6,6+words*width,width))
                report["observed_cases"]+=1;report["graded_cases"]+=1
                if got!=want:
                    report["mismatches"]+=1
                    if len(report["examples"])<8:report["examples"].append(dict(index=index,expected=list(want),observed=list(got)))
            require(f.readline()==f"LOG complete cases={cases}\n","missing completion")
            require(f.readline()=="END observations_complete\n","false END")
            require(re.fullmatch(r"#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n",f.readline()),"invalid trailer")
            require(not f.read(),"trailing data")
            report["status"]="fail" if report["mismatches"] else "pass"
    except (ValueError,OSError,UnicodeError) as e:report["detail"]=str(e)
    return report

def run(emulator,image,result):
    env=os.environ.copy()
    for key in ("INPUT","INPUT_FILE","DEBUG","TRACE","TRACE_FILE","VELOCE_TEST_RESETS"):env.pop(key,None)
    env.update(HEADLESS="1",FRAMES="50000",VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT="1")
    p=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=600)
    require(p.returncode==0,p.stdout+p.stderr);result.with_suffix(".log").write_text(p.stdout+p.stderr)

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--emulator",type=Path,default=ROOT/"build/bin/veloce");parser.add_argument("--artifacts",type=Path,default=ROOT/f"build/artifacts/owned/n64_{SUITE}")
    args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,content=None):
        image=args.artifacts/(name+".z64");result=args.artifacts/(name+".result")
        if content is None:image.write_bytes(rom);run(args.emulator,image,result)
        else:result.write_text(content)
        report=evaluate(result,rom,short);(args.artifacts/(name+".json")).write_text(json.dumps(report,indent=2)+"\n");return report,result
    rom=build();require(rom==(HERE/(SUITE+".z64")).read_bytes(),"archive differs")
    baseline,path=grade(SUITE,rom);require(baseline["status"]=="pass",json.dumps(baseline))
    short=build() if SUITE=="ipl3_handoff" else build(short=True)
    control,path=grade("control",short,True);require(control["status"]=="pass",json.dumps(control))
    badrom=build(wrong_seed=True) if SUITE=="ipl3_handoff" else build(short=True,wrong_length=True)
    bad,_=grade("executed-mutant",badrom,True)
    require(bad["status"]=="fail" and bad["mismatches"]==(1 if SUITE=="ipl3_handoff" else 256),"executed mutant escaped")
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith("REG r="))
    words=12 if SUITE=="ipl3_handoff" else 16
    width=16 if SUITE=="ipl3_handoff" else 8
    for field in range(words):
        n=6+field*width;changed=row[:n]+format(int(row[n:n+width],16)^1,f"0{width}x")+row[n+width:]
        bad,_=grade(f"field-{field}",short,True,content.replace(row,changed,1));require(bad["status"]=="fail" and bad["mismatches"]==1,"field escaped")
    cases=1 if SUITE=="ipl3_handoff" else 256
    for name,broken in (("missing-row",content.replace(row+"\n","",1)),("bad-row",content.replace(row,"REG r=bad",1)),("wrong-domain",content.replace(f"cases={cases} words={words}",f"cases={cases+1} words={words}",1)),("false-pass",content.replace("END observations_complete","END 0/0")),("missing-trailer",content[:content.index("#VELOCE end ")]),("unterminated",content.rstrip("\n")),("trailing",content+"extra\n")):
        bad,_=grade(name,short,True,broken);require(bad["status"]=="error",name+" accepted")
    print(f"N64 {SUITE}: PASS {baseline['graded_cases']}/{baseline['expected_cases']} rows; executed mutant, {words} field and 7 stream controls rejected")
    return 0
if __name__=="__main__":sys.exit(main())
