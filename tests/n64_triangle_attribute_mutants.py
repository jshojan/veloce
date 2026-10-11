#!/usr/bin/env python3
"""Check independent interpolation oracles against deliberate production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "load-subrow": ("bool(t.edge[1][1]&0x80000000)==t.flip", "bool(t.edge[1][1]&0x80000000)!=t.flip"),
    "row-step": ("unsigned(int32_t(scan)-top)", "unsigned(int32_t(scan)-top+1)"),
    "fraction": ("(edge.major>>8)&255", "(edge.major>>9)&255"),
    "pixel-quantization": ("c[1]&~31u", "c[1]"),
    "coverage-full": ("cv.count==8", "cv.count==7"),
    "gradient-sign": ("lane==7?22:13", "lane==7?22:14"),
    "depth-saturation": ("range==2?0x3ffff", "range==2?0"),
    "depth-normalization": ("if(sum==1)return 3", "if(sum==1)return 2"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_triangle_attributes")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_triangle_attributes.hpp").read_text()
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
        (directory / "rdp_triangle_attributes.hpp").write_text(changed)
        binary = directory / "test"
        # A failed compilation is never counted as an oracle rejection.
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"),
            str(ROOT / "tests/cpp/n64_rdp_triangle_attributes_test.cpp"), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("atomic bounds PASS\n")
        rejected = result.returncode == 1 and result.stderr == "triangle attributes mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 triangle attributes: baseline passes; all eight production faults rejected")


if __name__ == "__main__":
    main()
