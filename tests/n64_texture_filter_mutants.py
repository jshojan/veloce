#!/usr/bin/env python3
"""Check independent texture filter oracles against deliberate production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "diagonal-selection": ("fraction+t>=32", "fraction+t>32"),
    "triangle-rounding": ("+16,32)", "+15,32)"),
    "chroma-parity": ("options.odd_s?16u:0u", "options.odd_s?0u:0u"),
    "center-divisor": ("+2,4)", "+2,3)"),
    "signed-feedback": ("int64_t(raw)-512", "int64_t(raw)"),
    "feedback-center": ("sum+=64*", "sum+=32*"),
    "result-width": ("&511", "&255"),
    "coefficient-oddness": ("coefficients[i]&511))+1", "coefficients[i]&511))+0"),
    "mixed-luma-selection": ("q[s+t>=32?3:0]", "q[uv+t>=32?3:0]"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_texture_filter")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_texture_filter.hpp").read_text()
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
        (directory / "rdp_texture_filter.hpp").write_text(changed)
        binary = directory / "test"
        # A failed compilation is never counted as an oracle rejection.
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"),
            str(ROOT / "tests/cpp/n64_rdp_texture_filter_test.cpp"), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("center goldens PASS\n")
        rejected = result.returncode == 1 and result.stderr == "texture filter mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 texture filter: baseline passes; all nine production faults rejected")


if __name__ == "__main__":
    main()
