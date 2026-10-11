#!/usr/bin/env python3
"""Check independent TMEM texel oracles against deliberate production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "row-mask": ("uint32_t(t)&255", "uint32_t(t)&127"),
    "odd-row": ("(t&1)?4u:0u", "(t&1)?0u:0u"),
    "split-bank": ("|1024", "|0"),
    "blue-expansion": ("b=(v>>1)&31", "b=(v>>2)&31"),
    "nibble": ("(s&1)?packed&15:packed>>4", "(s&1)?packed>>4:packed&15"),
    "palette-address": ("index&255", "index&127"),
    "palette-index-format": ("kind==3", "kind==2"),
    "yuv-bias": ("int32_t(v)-128", "int32_t(v)-127"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_texel_fetch")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_texel_fetch.hpp").read_text()
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
        (directory / "rdp_texel_fetch.hpp").write_text(changed)
        binary = directory / "test"
        # A failed compilation is never counted as an oracle rejection.
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"),
            str(ROOT / "tests/cpp/n64_rdp_texel_fetch_test.cpp"), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("canonical banks and aliases PASS\n")
        rejected = result.returncode == 1 and result.stderr == "texel fetch mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 texel fetch: baseline passes; all eight production faults rejected")


if __name__ == "__main__":
    main()
