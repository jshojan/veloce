#!/usr/bin/env python3
"""Require independent dither tests to reject executed production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
 "magic-matrix": ("rdp_dither.hpp", "magic={0,6,1,7", "magic={1,6,1,7"),
 "bayer-matrix": ("rdp_dither.hpp", "bayer={0,4,1,5", "bayer={0,5,1,5"),
 "field-row": ("rdp_dither.hpp", "field?1:0", "field?0:0"),
 "inverse-alpha": ("rdp_dither.hpp", "base^7", "base^6"),
 "disabled-rgb-alpha": ("rdp_dither.hpp", "rgb==0?magic[index]", "(rgb==0 || rgb==3)?magic[index]"),
 "round-threshold": ("rdp_dither.hpp", "(value&7)>seed", "(value&7)>=seed"),
 "round-saturation": ("rdp_dither.hpp", "std::min(255u", "std::min(248u"),
 "pixel-alpha": ("rdp_blender.hpp", "alpha256+alpha_dither", "alpha256"),
 "shade-alpha": ("rdp_blender.hpp", "shade_alpha+alpha_dither", "shade_alpha"),
 "coverage-order": ("rdp_blender.hpp", "alpha256*count+4", "(alpha256+alpha_dither)*count+4"),
 "coverage-alpha-select": ("rdp_blender.hpp", "std::min(255u,(mode&4096)?scaled:count*32)", "std::min(255u,((mode&4096)?scaled:count*32)+alpha_dither)"),
 "ignored-reserved": ("rdp_triangle_shade.hpp", "(1ull<<15)", "(0ull<<15)"),
 "inactive-texture": ("rdp_triangle_shade.hpp", ":(blender?(2047ull<<41):0)", ":0"),
 "pixel-alpha-stage": ("rdp_triangle_shade.hpp", "nullptr,seeds.alpha", "nullptr,0"),
 "pixel-rgb-stage": ("rdp_triangle_shade.hpp", "RdpDither::apply(result.rgb,seeds.rgb)", "(void)seeds.rgb"),
}




def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_rdp_dither")
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
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"), str(ROOT / "tests/cpp/n64_rdp_dither_test.cpp"),
            "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("triangles and ignored fields PASS\n")
        rejected = result.returncode == 1 and result.stderr == "dither mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 ordered dither: baseline passes; all fifteen production faults rejected")


if __name__ == "__main__":
    main()
