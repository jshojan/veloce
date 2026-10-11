#!/usr/bin/env python3
"""Check independent texel quad oracles against deliberate production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "row-carry": ("tile.line()*uint32_t(row1)", "tile.line()*(uint32_t(row1)&255)"),
    "chroma-stride": ("+(right?uint32_t(ds):0u)", "+(right?0u:0u)"),
    "palette-bank": ("corner^(upper_uv?3u:0u)", "corner^(upper_uv?0u:0u)"),
    "lane-remap": ("if(upper!=upper_uv", "if(upper==upper_uv"),
    "nearest-index": ("nearest?sample.s.texel:", "nearest?sample.s.neighbor:"),
    "yuv-nibble": ("(value>>4)*17", "(value>>4)*16"),
    "yuv-alpha": ("a=(y&15)|(packed&240)", "a=(y&15)|(packed&224)"),
    "right-luma": ("right?luma_at>>1:chroma_at", "right?chroma_at:chroma_at"),
    "palette-type": ("palette==Palette::ia", "palette==Palette::rgba"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_texel_quad")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_texel_quad.hpp").read_text()
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
        (directory / "rdp_texel_quad.hpp").write_text(changed)
        binary = directory / "test"
        # A failed compilation is never counted as an oracle rejection.
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"),
            str(ROOT / "tests/cpp/n64_rdp_texel_quad_test.cpp"), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("coupled banks and lanes PASS\n")
        rejected = result.returncode == 1 and result.stderr == "texel quad mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 texel quad: baseline passes; all nine production faults rejected")


if __name__ == "__main__":
    main()
