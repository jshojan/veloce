#!/usr/bin/env python3
"""Require independent hidden-memory tests to reject executed production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "packed-address": ("address/2%4", "address/2%2"),
    "byte-upper": ("(address&1)?unsigned(value&1):0", "unsigned(value&1)"),
    "halfword-replication": ("&1)*3", "&1)*1"),
    "store-halfword-order": ("8*(bytes-n-2)", "8*n"),
    "ebus-byte-lane": ("address%4==3", "address%4==2"),
    "ebus-word-order": ("+3-n%4", "+n%4"),
    "ebus-read-order": ("(halfword(address)<<2)|halfword(address+2)", "halfword(address)|(halfword(address+2)<<2)"),
    "depth-history": ("if(color)remember(address,hidden)", "if(!color)remember(address,hidden)"),
    "word-second-history": ("remember(address+2,second)", "remember(address+2,first)"),
    "history-lanes": ("address/2%8", "address/2%4"),
    "delayed-order": ("size_t(delayed_)<address", "size_t(delayed_)<=address"),
    "delayed-pair": ("low*3:(halfword(address)&2)|low", "low*1:(halfword(address)&2)|low"),
    "delay-flush": ("(halfword(address)&1)|(remembered(address)&2)", "(halfword(address)&1)"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_hidden_rdram")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/hidden_rdram.hpp").read_text()
    reports = []
    for name in ("baseline", *FAULTS):
        changed = source
        if name != "baseline":
            old, new = FAULTS[name]
            if old not in source:
                raise ValueError(f"Fault seam changed: {name}")
            changed = source.replace(old, new)
        directory = (args.artifacts / name).resolve()
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "hidden_rdram.hpp").write_text(changed)
        binary = directory / "test"
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), str(ROOT / "tests/cpp/n64_hidden_rdram_test.cpp"),
            "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("atomic rejection/replay PASS\n")
        rejected = result.returncode == 1 and result.stderr.startswith("hidden RDRAM mismatch at line ")
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 hidden RDRAM: baseline passes; all thirteen production faults rejected")


if __name__ == "__main__":
    main()
