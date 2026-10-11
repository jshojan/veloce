#!/usr/bin/env python3
"""Require independent depth tests to reject executed production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
 "high-depth-bit": ("rdp_primitive_depth.hpp", "0x7fffffffu", "0xffffffffu"),
 "depth-scale": ("rdp_primitive_depth.hpp", "(value>>16)*8", "(value>>16)*4"),
 "raw-delta": ("rdp_primitive_depth.hpp", "return uint16_t(value)", "return uint16_t(value)|1"),
 "state-reserved": ("rdp_primitive_depth.hpp", "state>0x7fffffffu", "state>0xffffffffu"),
 "command-dispatch": ("dp_interface.hpp", "case 46:primitive_depth.command(word)", "case 46:primitive_depth.command(word>>16)"),
 "pixel-source": ("rdp_triangle_shade.hpp", "primitive_mode?primitive->z():pixel.z", "pixel.z"),
 "delta-source": ("rdp_triangle_shade.hpp", "primitive_mode?primitive->delta():attributes.delta_z", "attributes.delta_z"),
}




def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_rdp_primitive_depth")
    args = parser.parse_args()
    headers={p.name:p.read_text() for p in (ROOT / "cores/n64/src").glob("*.hpp")}
    source="".join(headers.values())
    reports = []
    for name in ("baseline", *FAULTS):
        changed = dict(headers)
        if name != "baseline":
            header, old, new = FAULTS[name]
            if old not in changed[header]:raise ValueError(f"Fault seam changed: {name}")
            changed[header]=changed[header].replace(old,new)
        directory = (args.artifacts / name).resolve()
        directory.mkdir(parents=True, exist_ok=True)
        for header,content in changed.items():(directory/header).write_text(content)
        binary = directory / "test"
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"), str(ROOT / "tests/cpp/n64_rdp_primitive_depth_test.cpp"),
            "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("reset/state rejection PASS\n")
        rejected = result.returncode == 1 and result.stderr == "primitive depth mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 depth: baseline passes; all seven production faults rejected")


if __name__ == "__main__":
    main()
