#!/usr/bin/env python3
"""Independent arithmetic grading of actual TLB/mapped-memory register streams."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build
SUITE="tlb_translation"
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]

def require(ok,message):
    if not ok:raise ValueError(message)

def expected(rom,short=False):
    for shift in (0,) if short else range(0,13,2):
        page=4096*2**shift;mask=page*2-8192
        for asid in range(4 if short else 256):
            for pattern in range(32):
                valid=pattern%2;dirty=(pattern//2)%2;global0=(pattern//4)%2;global1=(pattern//8)%2;mismatch=pattern//16
                match=not mismatch or (global0 and global1)
                for half in range(2):
                    address=0x20000020+half*page
                    seeded=0x89abcdef if half and page<=0x400000 else 0x12345678
                    load_ok=match and valid;store_ok=load_ok and dirty
                    global_bit=global0*global1
                    yield(seeded if load_ok else 0,0 if load_ok else 8,0 if load_ok else address,
                          0 if store_ok else 4 if load_ok else 12,0 if store_ok else address,
                          0xdeadbeef if store_ok else seeded,0 if match else 0x80000000,
                          valid*2+dirty*4+16+global_bit,0x10000+valid*2+dirty*4+16+global_bit,mask,0x20000000+asid)

def evaluate(path,rom,short=False):
    cases=256 if short else 114688
    words=11
    width=8
    report=dict(status="error",suite=SUITE,expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding="ascii") as f:
            require(f.readline()==f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n","wrong identity")
            require(f.readline()==f"VELOCE 1 n64 {SUITE}\n","wrong suite")
            require(f.readline()==f"BEGIN data cases={cases} words={words}\n","wrong domain")
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
    short=build(short=True)
    control,path=grade("control",short,True);require(control["status"]=="pass",json.dumps(control))
    badrom=build(short=True,wrong_asid=True)
    bad,_=grade("executed-mutant",badrom,True)
    require(bad["status"]=="fail" and bad["mismatches"]==256,"executed mutant escaped")
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith("REG r="))
    words=11
    width=8
    for field in range(words):
        n=6+field*width;changed=row[:n]+format(int(row[n:n+width],16)^1,f"0{width}x")+row[n+width:]
        bad,_=grade(f"field-{field}",short,True,content.replace(row,changed,1));require(bad["status"]=="fail" and bad["mismatches"]==1,"field escaped")
    cases=256
    for name,broken in (("missing-row",content.replace(row+"\n","",1)),("bad-row",content.replace(row,"REG r=bad",1)),("wrong-domain",content.replace(f"cases={cases} words={words}",f"cases={cases+1} words={words}",1)),("false-pass",content.replace("END observations_complete","END 0/0")),("missing-trailer",content[:content.index("#VELOCE end ")]),("unterminated",content.rstrip("\n")),("trailing",content+"extra\n")):
        bad,_=grade(name,short,True,broken);require(bad["status"]=="error",name+" accepted")
    print(f"N64 {SUITE}: PASS {baseline['graded_cases']}/{baseline['expected_cases']} rows; executed mutant, {words} field and 7 stream controls rejected")
    return 0
if __name__=="__main__":sys.exit(main())
