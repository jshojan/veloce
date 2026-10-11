#!/usr/bin/env python3
"""Require independent depth tests to reject executed production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "compression-boundary": ("z<0x20000", "z<=0x20000"),
    "decompression-step": ("shifts={6,5,4,3,2,1,0,0}", "shifts={5,5,4,3,2,1,0,0}"),
    "delta-encoding": ("code|=n", "code=n"),
    "comparison-highest": ("return result;", "return value&1?1:result;"),
    "precision-minimum": ("16u>>precision", "8u>>precision"),
    "coplanar-encoding": ("memory_delta==32768", "memory_delta==16384"),
    "comparison-tolerance": ("tolerance=dz*8", "tolerance=dz*4"),
    "maximum-depth": ("previous==0x3ffff", "previous==0x3fffe"),
    "interpenetrating-coverage": ("(coefficient*coverage)>>3", "(coefficient*coverage)>>2"),
    "blend-shift-a": ("int(encoded)-int(out.stored_delta)", "int(out.stored_delta)-int(encoded)"),
    "store-delta-pair": ("encoded&3,false", "encoded>>2,false"),
    "depth-color-history": ("encoded&3,false", "encoded&3,true"),
}



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_rdp_depth")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_depth.hpp").read_text()
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
        (directory / "rdp_depth.hpp").write_text(changed)
        binary = directory / "test"
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"), str(ROOT / "tests/cpp/n64_rdp_depth_test.cpp"),
            "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("atomic bounds/history preservation PASS\n")
        rejected = result.returncode == 1 and result.stderr == "depth mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 depth: baseline passes; all twelve production faults rejected")


if __name__ == "__main__":
    main()
