#!/usr/bin/env python3
"""Check independent tile coordinate oracles against deliberate production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "reciprocal-exception": ("segment==6?14979", "segment==6?14980"),
    "normalization": ("16384u>>shift", "8192u>>shift"),
    "negative-floor": ("value/divisor-(value%divisor<0)", "value/divisor"),
    "zero-w": ("if(!w || (w&32768))", "if(w&32768)"),
    "negative-w": ("if(!w || (w&32768))", "if(!w)"),
    "overflow-mask": ("29-reciprocal.shift", "28-reciprocal.shift"),
    "coordinate-width": ("uint32_t(scaled)&131071", "uint32_t(scaled)&65535"),
    "positive-saturation": ("if(value&262144)return 32767", "if(value&262144)return 32768"),
    "underflow-precedence": ("if(value&262144)return 32767", "if((value&262144) && !(value&131072))return 32767"),
}



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_texture_divider")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_texture_divider.hpp").read_text()
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
        (directory / "rdp_texture_divider.hpp").write_text(changed)
        binary = directory / "test"
        # A failed compilation is never counted as an oracle rejection.
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"),
            str(ROOT / "tests/cpp/n64_rdp_texture_divider_test.cpp"), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("reciprocal goldens PASS\n")
        rejected = result.returncode == 1 and result.stderr == "texture divider mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 texture dividers: baseline passes; all nine production faults rejected")


if __name__ == "__main__":
    main()
