#!/usr/bin/env python3
"""Grade actual guest SI/PIF observations against an independent digital input oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
# Frontend virtual button indices and controller wire positions, in physical button order.
BUTTONS = (0,1,20,6,8,9,10,11,4,5,21,22,23,24)
WIRE = (15,14,13,12,11,10,9,8,5,4,3,2,1,0)


def require(ok, message):
    if not ok: raise ValueError(message)


def evaluate(path, rom, cases=16384):
    report = dict(status="error", suite="controller_serial", expected_cases=cases,
                  observed_cases=0, graded_cases=0, mismatches=0, examples=[],
                  rom_sha256=hashlib.sha256(rom).hexdigest(), result_file=str(path))
    try:
        with path.open(encoding="ascii") as f:
            require(f.readline() == f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n", "wrong file identity")
            require(f.readline() == "VELOCE 1 n64 controller_serial\n", "wrong suite")
            require(f.readline() == f"BEGIN digital cases={cases} words=8\n", "wrong domain")
            for sample in range(cases):
                line = f.readline()
                require(re.fullmatch(r"REG r=[0-9a-f]{128}\n", line), "missing/malformed observation")
                got = tuple(int(line[6+n*16:22+n*16],16) for n in range(8))
                buttons = sum(1 << bit for n,bit in enumerate(WIRE) if sample & (1 << n))
                packet = bytes([1,4,1,buttons >> 8,buttons & 255,0,0,1])
                want = (int.from_bytes(packet,"big"), 0x040100000000fe00, 0x010300050002fe00,
                        0x1000,2,2,0,0)
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


def run(emulator, image, result, schedule, cases):
    env = os.environ.copy()
    for key in ("INPUT", "INPUT_FILE", "DEBUG", "TRACE", "TRACE_FILE", "VELOCE_TEST_RESETS"):
        env.pop(key, None)
    env.update(HEADLESS="1", FRAMES=str(cases+5), INPUT_FILE=str(schedule.resolve()),
               VELOCE_TEST_OUT=str(result.resolve()), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()),str(image.resolve())],cwd=ROOT,env=env,capture_output=True,text=True,timeout=90)
    require(proc.returncode == 0, proc.stdout + proc.stderr)
    require(f"Loaded {cases} input events" in proc.stderr, "input file was not consumed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator",type=Path,default=ROOT/"build/bin/veloce")
    parser.add_argument("--artifacts",type=Path,default=ROOT/"build/artifacts/owned/n64_controller")
    args = parser.parse_args(); args.artifacts.mkdir(parents=True,exist_ok=True)
    rom = build(); require((HERE/"controller_serial.z64").read_bytes() == rom,"archive differs")
    schedule = args.artifacts/"input.txt"
    schedule.write_text(','.join(f"{n}:{sum(1 << bit for k,bit in enumerate(BUTTONS) if n & (1 << k)):x}" for n in range(16384)))
    result = args.artifacts/"controller_serial.result"
    run(args.emulator,HERE/"controller_serial.z64",result,schedule,16384)
    report = evaluate(result,rom)
    (args.artifacts/"controller_serial.json").write_text(json.dumps(report,indent=2)+"\n")
    require(report["status"] == "pass",json.dumps(report))
    mutant = build(16,wrong_command=True); image = args.artifacts/"negative.z64"; image.write_bytes(mutant)
    short = args.artifacts/"short-input.txt"
    short.write_text(','.join(schedule.read_text().split(',')[:16]))
    negative = args.artifacts/"negative.result"
    run(args.emulator,image,negative,short,16)
    rejected = evaluate(negative,mutant,16)
    require(rejected["status"] == "fail" and rejected["mismatches"] == 16,"executed wrong command accepted")
    (args.artifacts/"negative.json").write_text(json.dumps(rejected,indent=2)+"\n")
    content = negative.read_text(); row = next(line for line in content.splitlines() if line.startswith("REG r="))
    for name,broken in (
        ("missing-row",content.replace(row+'\n','',1)), ("malformed-row",content.replace(row,'REG r=bad',1)),
        ("wrong-domain",content.replace('cases=16 words=8','cases=15 words=8',1)),
        ("false-pass",content.replace('END observations_complete','END 0/0')),
        ("missing-trailer",content[:content.index('#VELOCE end ')]),
        ("unterminated-trailer",content.rstrip('\n')), ("trailing-data",content+'extra\n')):
        path = args.artifacts/(name+'.result'); path.write_text(broken)
        invalid = evaluate(path,mutant,16); require(invalid['status']=='error',name+' accepted')
        (args.artifacts/(name+'.json')).write_text(json.dumps(invalid,indent=2)+'\n')
    # The full run exceeds common per-environment-string limits. Check file failures and
    # legacy INPUT parsing separately, including 32-bit masks and optional 0x prefixes.
    env = os.environ.copy()
    for key in ("INPUT","INPUT_FILE","VELOCE_TEST_OUT","VELOCE_TEST_EXIT","VELOCE_TEST_RESETS"):
        env.pop(key,None)
    env.update(HEADLESS="1",FRAMES="1")
    failures = []
    for text in ("-1:0","2147483648:0","0:100000000","0:xyz","0:","0","0:1junk"):
        bad = args.artifacts/"bad-input.txt"; bad.write_text(text)
        proc = subprocess.run([str(args.emulator.resolve()),str(image.resolve())],cwd=ROOT,
                              env=dict(env,INPUT_FILE=str(bad.resolve())),capture_output=True,text=True,timeout=10)
        require(proc.returncode != 0 and "Invalid input schedule" in proc.stderr,"invalid schedule accepted: "+text)
        failures.append(text)
    for name,extra in (("missing",dict(INPUT_FILE=str((args.artifacts/"does-not-exist").resolve()))),
                       ("conflict",dict(INPUT="0:0",INPUT_FILE=str(short.resolve())))):
        proc = subprocess.run([str(args.emulator.resolve()),str(image.resolve())],cwd=ROOT,
                              env=dict(env,**extra),capture_output=True,text=True,timeout=10)
        require(proc.returncode != 0,name+" schedule accepted")
        failures.append(name)
    proc = subprocess.run([str(args.emulator.resolve()),str(image.resolve())],cwd=ROOT,
                          env=dict(env,INPUT="0:0xffffffff"),capture_output=True,text=True,timeout=10)
    require(proc.returncode == 0 and "Loaded 1 input events" in proc.stderr,"legacy INPUT mask rejected")
    (args.artifacts/"input-validation.json").write_text(json.dumps(dict(status="pass",rejected=failures,legacy_input="pass"),indent=2)+'\n')
    print("N64 digital controller: PASS 16384/16384 cases, 131072 observed register values; executed mutant FAIL; 7 damaged files ERROR")
    return 0

if __name__ == '__main__': sys.exit(main())
