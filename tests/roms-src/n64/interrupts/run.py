#!/usr/bin/env python3
"""Grade actual CP0 and SI interrupt observations with an independent gate oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build, domain

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]

def require(ok, message):
    if not ok: raise ValueError(message)


def evaluate(path, rom, cases=65536):
    report = dict(status="error", suite="interrupts", expected_cases=cases,
                  observed_cases=0, graded_cases=0, mismatches=0, examples=[],
                  interrupt_entries=0, expected_interrupt_entries=0,
                  rom_sha256=hashlib.sha256(rom).hexdigest(), result_file=str(path))
    try:
        with path.open(encoding="ascii") as f:
            require(f.readline() == f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n", "wrong file identity")
            require(f.readline() == "VELOCE 1 n64 interrupts\n", "wrong suite")
            require(f.readline() == f"BEGIN gates cases={cases} words=8\n", "wrong domain")
            _, layout = build(cases, metadata=True)
            for sample, (mask,control,software,si,timer,mi) in enumerate(domain()):
                if sample == cases: break
                line = f.readline()
                require(re.fullmatch(r"REG r=[0-9a-f]{128}\n", line), "missing/malformed observation")
                got = tuple(int(line[6+n*16:22+n*16],16) for n in range(8))
                pending = software | (4 if si and mi else 0) | (128 if timer else 0)
                taken = control == 1 and bool(mask & pending)
                report["expected_interrupt_entries"] += int(taken)
                report["interrupt_entries"] += int(got[0] == 1)
                cause = pending << 8
                status = mask << 8 | control | (2 if taken else 0)
                epc = layout["probe"] | 0xffffffff00000000 if taken else 0x44444444
                want = (int(taken), 0 if taken else 0x1234,cause,status,epc,
                        0x1000 if si else 0,2 if si else 0,2 if mi else 0)
                report["observed_cases"] += 1; report["graded_cases"] += 1
                if got != want:
                    report["mismatches"] += 1
                    if len(report["examples"]) < 8:
                        report["examples"].append(dict(sample=sample, expected=[f"{v:016x}" for v in want], observed=[f"{v:016x}" for v in got]))
            require(f.readline() == f"LOG complete cases={cases}\n", "missing completion")
            require(f.readline() == "END observations_complete\n", "false END")
            trailer = f.readline()
            require(re.fullmatch(r"#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n", trailer), "missing trailer")
            require(not f.read(), "trailing data")
            report["status"] = "fail" if report["mismatches"] else "pass"
    except (ValueError, OSError, UnicodeError) as e: report["detail"] = str(e)
    return report


def run(emulator,image,result):
    env=os.environ.copy()
    for key in ("INPUT","INPUT_FILE","DEBUG","TRACE","TRACE_FILE","VELOCE_TEST_RESETS"):
        env.pop(key,None)
    env.update(HEADLESS="1",FRAMES="1000",VELOCE_TEST_OUT=str(result.resolve()),VELOCE_TEST_EXIT="1")
    proc=subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,
                        capture_output=True,text=True,timeout=120)
    require(proc.returncode==0,proc.stdout+proc.stderr)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator",type=Path,default=ROOT/"build/bin/veloce")
    parser.add_argument("--artifacts",type=Path,default=ROOT/"build/artifacts/owned/n64_interrupts")
    args=parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    rom=build(); require((HERE/"interrupts.z64").read_bytes()==rom,"archive differs")
    result=args.artifacts/"interrupts.result"
    run(args.emulator,HERE/"interrupts.z64",result)
    report=evaluate(result,rom)
    (args.artifacts/"interrupts.json").write_text(json.dumps(report,indent=2)+'\n')
    require(report["status"]=="pass",json.dumps(report))
    mutant=build(64,wrong_register=True); image=args.artifacts/"negative.z64"; image.write_bytes(mutant)
    result=args.artifacts/"negative.result"; run(args.emulator,image,result)
    rejected=evaluate(result,mutant,64)
    require(rejected["status"]=="fail" and rejected["mismatches"]==63,"wrong CP0 register accepted")
    (args.artifacts/"negative.json").write_text(json.dumps(rejected,indent=2)+'\n')
    content=result.read_text(); row=next(line for line in content.splitlines() if line.startswith("REG r="))
    for name,broken in (
        ("missing-row",content.replace(row+'\n','',1)), ("malformed-row",content.replace(row,'REG r=bad',1)),
        ("wrong-domain",content.replace('cases=64 words=8','cases=63 words=8',1)),
        ("false-pass",content.replace('END observations_complete','END 0/0')),
        ("missing-trailer",content[:content.index('#VELOCE end ')]),
        ("unterminated-trailer",content.rstrip('\n')), ("trailing-data",content+'extra\n')):
        path=args.artifacts/(name+'.result'); path.write_text(broken)
        invalid=evaluate(path,mutant,64); require(invalid['status']=='error',name+' accepted')
        (args.artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    print("N64 interrupts: PASS 65536/65536 gates, 524288 observed register values; executed mutant FAIL; 7 damaged files ERROR")
    return 0

if __name__ == '__main__': sys.exit(main())
