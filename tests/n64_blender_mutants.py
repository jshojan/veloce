#!/usr/bin/env python3
"""Check independent tile coordinate oracles against deliberate production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "second-factor": ("second*(b+1)", "second*b"),
    "force-precision": ("(sum>>5)&255", "(sum>>6)&255"),
    "quotient-carry": ("if(next&16)", "if(next&8)"),
    "full-alpha": ("color[3]==255?256:color[3]", "color[3]"),
    "alpha-threshold": ("color[3]<(blend&255)", "color[3]<=(blend&255)"),
    "selected-inverse": ("(alpha^255)", "(color[3]^255)"),
    "memory-weight": ("a&=60", "a&=31"),
    "partial-reject": ("color[3]>=255", "color[3]>255"),
    "retained-memory": ("if(mode&64)memory_rgb=memory>>8", "memory_rgb=(mode&64)?memory>>8:0"),
    "coverage-alpha": ("count=(scaled>>5)&15", "count=(scaled>>4)&15"),
}



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_blender")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_blender.hpp").read_text()
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
        (directory / "rdp_blender.hpp").write_text(changed)
        binary = directory / "test"
        # A failed compilation is never counted as an oracle rejection.
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"),
            str(ROOT / "tests/cpp/n64_rdp_blender_test.cpp"), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("state rejection PASS\n")
        rejected = result.returncode == 1 and result.stderr == "blender mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 blenders: baseline passes; all ten production faults rejected")


if __name__ == "__main__":
    main()
