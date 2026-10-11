#!/usr/bin/env python3
"""Independent integer grading of actual DP register and RAM streams."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build
from oracle import expected as grade_case,frontend
SUITE="controller_cached_reads"
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]

def require(ok,message):
    if not ok:raise ValueError(message)

def expected(rom,short=False):
    for sample in range(16 if short else 16384):yield grade_case(sample)

def evaluate(path,rom,short=False):
    count=16 if short else 16384
    words=16
    width=16
    report=dict(status="error",suite=SUITE,expected_cases=count,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding="ascii") as f:
            require(f.readline()==f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n","wrong identity")
            require(f.readline()==f"VELOCE 1 n64 {SUITE}\n","wrong suite")
            require(f.readline()==f"BEGIN digital cases={count} words={words} chunks=7,9\n","wrong domain")
            for index,want in enumerate(expected(rom,short)):
                captured=[]
                for chunk in (7,9):
                    row=f.readline();require(re.fullmatch(r"REG r=[0-9a-f]{"+str(chunk*width)+r"}\n",row),"missing/malformed chunk")
                    captured.extend(int(row[n:n+width],16) for n in range(6,6+chunk*width,width))
                got=tuple(captured)
                report["observed_cases"]+=1;report["graded_cases"]+=1
                if got!=want:
                    report["mismatches"]+=1
                    if len(report["examples"])<8:report["examples"].append(dict(index=index,expected=list(want),observed=list(got)))
            require(f.readline()==f"LOG complete cases={count}\n","missing completion")
            require(f.readline()=="END observations_complete\n","false END")
            require(re.fullmatch(r"#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n",f.readline()),"invalid trailer")
            require(not f.read(),"trailing data")
            report["status"]="fail" if report["mismatches"] else "pass"
    except (ValueError,OSError,UnicodeError) as e:report["detail"]=str(e)
    return report

def run(emulator,image,result,count):
    env=os.environ.copy()
    for key in ("INPUT","INPUT_FILE","DEBUG","TRACE","TRACE_FILE","VELOCE_TEST_RESETS"):env.pop(key,None)
    schedule=result.with_suffix(".input.txt");schedule.write_text(','.join(f"{frame}:{frontend(frame//2^(16383 if frame%2 else 0)):x}" for frame in range(count*2)))
    env.update(HEADLESS="1",FRAMES=str(count*2+5),INPUT_FILE=str(schedule.resolve()),VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT="1")
    p=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=90)
    require(p.returncode==0,p.stdout+p.stderr);require(f"Loaded {count*2} input events" in p.stderr,"input schedule not consumed");result.with_suffix(".log").write_text(p.stdout+p.stderr)

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--emulator",type=Path,default=ROOT/"build/bin/veloce");parser.add_argument("--artifacts",type=Path,default=ROOT/f"build/artifacts/owned/n64_{SUITE}")
    args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    def grade(name,rom,short=False,content=None):
        image=args.artifacts/(name+".z64");result=args.artifacts/(name+".result")
        if content is None:image.write_bytes(rom);run(args.emulator,image,result,16 if short else 16384)
        else:result.write_text(content)
        report=evaluate(result,rom,short);(args.artifacts/(name+".json")).write_text(json.dumps(report,indent=2)+"\n");return report,result
    rom=build();require(rom==(HERE/(SUITE+".z64")).read_bytes(),"archive differs")
    baseline,path=grade(SUITE,rom);require(baseline["status"]=="pass",json.dumps(baseline))
    short=build(16)
    control,path=grade("control",short,True);require(control["status"]=="pass",json.dumps(control))
    badrom=build(16,wrong_capture=True)
    bad,_=grade("executed-mutant",badrom,True)
    require(bad["status"]=="fail" and bad["mismatches"]==16,"executed mutant escaped")
    content=path.read_text();rows=[line for line in content.splitlines() if line.startswith("REG r=")][:2];row=rows[0]
    words=16
    width=16
    for field in range(words):
        target=rows[0 if field<7 else 1];n=6+(field if field<7 else field-7)*width;changed=target[:n]+format(int(target[n:n+width],16)^1,f"0{width}x")+target[n+width:]
        bad,_=grade(f"field-{field}",short,True,content.replace(target,changed,1));require(bad["status"]=="fail" and bad["mismatches"]==1,"field escaped")
    count=16
    for name,broken in (("missing-row",content.replace(row+"\n","",1)),("bad-row",content.replace(row,"REG r=bad",1)),("wrong-domain",content.replace(f"cases={count} words={words}",f"cases={count+1} words={words}",1)),("false-pass",content.replace("END observations_complete","END 0/0")),("missing-trailer",content[:content.index("#VELOCE end ")]),("unterminated",content.rstrip("\n")),("trailing",content+"extra\n")):
        bad,_=grade(name,short,True,broken);require(bad["status"]=="error",name+" accepted")
    print(f"N64 {SUITE}: PASS {baseline['graded_cases']}/{baseline['expected_cases']} rows; executed mutant, {words} field and 7 stream controls rejected")
    return 0
if __name__=="__main__":sys.exit(main())
