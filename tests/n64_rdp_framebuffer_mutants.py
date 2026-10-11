#!/usr/bin/env python3
"""Require independent framebuffer tests to reject executed production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "read-expansion": ("rdp_framebuffer.hpp", "((word>>8)&248)", "(((word>>8)&248)|(word>>13))"),
    "coverage-read-high": ("rdp_framebuffer.hpp", "(word&1)<<2", "(word&1)<<1"),
    "red-packing": ("rdp_framebuffer.hpp", "rgb[0]>>3", "rgb[0]>>2"),
    "coverage-write-high": ("rdp_framebuffer.hpp", "coverage>>2", "coverage>>1"),
    "rgba32-green-hidden": ("rdp_framebuffer.hpp", "(rgb[1]&1)*3", "(rgb[0]&1)*3"),
    "fill16-hidden": ("rdp_fill.hpp", "(value&1)*3", "(value&1)*1"),
    "fill32-second-hidden": ("rdp_fill.hpp", "(color&1)*3", "((color>>16)&1)*3"),
    "triangle-history-direction": ("rdp_triangle_fill.hpp", "hidden,triangle.flip)", "hidden,!triangle.flip)"),
    "triangle-delayed-flush": ("rdp_triangle_fill.hpp", "hidden->finish_rdp_bytes()", "(void)hidden"),
}



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_rdp_framebuffer")
    args = parser.parse_args()
    sources = {name:(ROOT / "cores/n64/src" / name).read_text() for name in ("rdp_framebuffer.hpp", "rdp_fill.hpp", "rdp_triangle_fill.hpp")}
    reports = []
    for name in ("baseline", *FAULTS):
        changed = dict(sources)
        if name != "baseline":
            header, old, new = FAULTS[name]
            if old not in sources[header]:
                raise ValueError(f"Fault seam changed: {name}")
            changed[header] = sources[header].replace(old, new)
        directory = (args.artifacts / name).resolve()
        directory.mkdir(parents=True, exist_ok=True)
        for header, content in changed.items():(directory / header).write_text(content)
        binary = directory / "test"
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"), str(ROOT / "tests/cpp/n64_rdp_framebuffer_test.cpp"),
            "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("delayed history and atomic bounds PASS\n")
        rejected = result.returncode == 1 and result.stderr.startswith("framebuffer mismatch at line ")
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256={name:hashlib.sha256(content.encode()).hexdigest() for name,content in sources.items()},results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 RDP framebuffer: baseline passes; all nine production faults rejected")


if __name__ == "__main__":
    main()
