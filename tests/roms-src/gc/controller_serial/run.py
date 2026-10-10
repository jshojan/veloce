#!/usr/bin/env python3
"""Independent host oracle for actual GameCube serial-controller register files."""
import argparse,hashlib,json,os,re,subprocess,sys,zlib
from pathlib import Path
from build import build
CONSOLE="gc"
CORE="GameCube"
EXT="vgc"
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]

def require(ok,message):
    if not ok:raise ValueError(message)

def expected(sample):
    # Independent enumeration in physical button order, not stimulus/CPU imports.
    physical=(256,512,1024,2048,4096,1,2,4,8,16,32,64)
    digital=128+sum(bit for n,bit in enumerate(physical) if sample&(1<<n))
    low=0x80800000+(255<<8 if sample&2048 else 0)+(255 if sample&1024 else 0)
    return (digital*65536+0x8080,low,0xc0030800,0,0x00030800,0x090000cc,0xc0010300,0,
            0x400300a5,0x00080000,0x400300a5,0x00080800,0x400300a5,0x00080808,0,0x20030806)

def evaluate(path,rom,cases=4096):
    report=dict(status="error",console=CONSOLE,suite="controller_serial",expected_cases=cases,observed_cases=0,graded_cases=0,mismatches=0,examples=[],rom_sha256=hashlib.sha256(rom).hexdigest(),result_file=str(path))
    try:
        with path.open(encoding="ascii") as f:
            require(f.readline()==f"#VELOCE 1 core={CORE} rom_crc32={zlib.crc32(rom):08x} channels=ppcdebug\n","wrong identity")
            require(f.readline()==f"VELOCE 1 {CONSOLE} controller_serial\n","wrong suite")
            require(f.readline()==f"BEGIN digital cases={cases} words=16\n","wrong domain")
            for sample in range(cases):
                row=f.readline();require(re.fullmatch(r"REG r=[0-9a-f]{128}\n",row),"missing/malformed row")
                got=tuple(int(row[n:n+8],16) for n in range(6,134,8));want=expected(sample)
                report["observed_cases"]+=1;report["graded_cases"]+=1
                if got!=want:
                    report["mismatches"]+=1
                    if len(report["examples"])<8:report["examples"].append(dict(sample=sample,expected=list(want),observed=list(got)))
            require(f.readline()==f"LOG complete cases={cases}\n","missing completion")
            require(f.readline()=="END observations_complete\n","false END")
            require(re.fullmatch(r"#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n",f.readline()),"invalid trailer")
            require(not f.read(),"trailing data")
            report["status"]="fail" if report["mismatches"] else "pass"
    except (ValueError,OSError,UnicodeError) as e:report["detail"]=str(e)
    return report

def run(emulator,image,result,schedule,cases):
    env=os.environ.copy()
    for key in ("INPUT","INPUT_FILE","DEBUG","TRACE","TRACE_FILE","VELOCE_TEST_RESETS"):env.pop(key,None)
    env.update(HEADLESS="1",FRAMES=str(cases+1),INPUT_FILE=str(schedule.resolve()),VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT="1")
    p=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=120)
    require(p.returncode==0,p.stdout+p.stderr)
    result.with_suffix(".log").write_text(p.stdout+p.stderr)

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--emulator",type=Path,default=ROOT/"build/bin/veloce");parser.add_argument("--artifacts",type=Path,default=ROOT/f"build/artifacts/owned/{CONSOLE}_controller")
    args=parser.parse_args();args.artifacts.mkdir(parents=True,exist_ok=True)
    # ABI virtual input positions in physical A/B/X/Y/Start/D-pad/Z/R/L order.
    virtual=(0,1,2,3,6,10,11,9,8,20,5,4)
    schedule=args.artifacts/"input.txt"
    schedule.write_text(",".join(f"{sample}:{sum(1<<bit for n,bit in enumerate(virtual) if sample&(1<<n)):x}" for sample in range(4096)))
    def grade(name,rom,cases=4096,content=None):
        image=args.artifacts/(name+"."+EXT);result=args.artifacts/(name+".result")
        if content is None:image.write_bytes(rom);run(args.emulator,image,result,schedule,cases)
        else:result.write_text(content)
        report=evaluate(result,rom,cases);(args.artifacts/(name+".json")).write_text(json.dumps(report,indent=2)+"\n");return report,result
    rom=build();require(rom==(HERE/("controller_serial."+EXT)).read_bytes(),"archive differs")
    report,result=grade("controller_serial",rom)
    require(report["status"]=="pass",json.dumps(report))
    short=build(16);control,path=grade("control",short,16);require(control["status"]=="pass",json.dumps(control))
    mutant,_=grade("wrong-command",build(16,wrong_command=True),16)
    require(mutant["status"]=="fail" and mutant["mismatches"]==16,"executed command mutant escaped")
    content=path.read_text();row=next(line for line in content.splitlines() if line.startswith("REG r="))
    for field in range(16):
        n=6+field*8;changed=row[:n]+f"{int(row[n:n+8],16)^1:08x}"+row[n+8:]
        bad,_=grade(f"field-{field}",short,16,content.replace(row,changed,1));require(bad["status"]=="fail" and bad["mismatches"]==1,"field escaped")
    for name,broken in (("missing-row",content.replace(row+"\n","",1)),("bad-row",content.replace(row,"REG r=bad",1)),("wrong-domain",content.replace("cases=16 words=16","cases=15 words=16",1)),("false-pass",content.replace("END observations_complete","END 0/0")),("missing-trailer",content[:content.index("#VELOCE end ")]),("unterminated",content.rstrip("\n")),("trailing",content+"extra\n")):
        bad,_=grade(name,short,16,broken);require(bad["status"]=="error",name+" accepted")
    print(f"{CORE} controller: PASS 4096/4096 digital snapshots (65536 words); executed mutant rejected; 16 field and 7 stream mutations rejected")
    return 0
if __name__=="__main__":sys.exit(main())
