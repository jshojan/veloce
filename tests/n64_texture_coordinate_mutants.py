#!/usr/bin/env python3
"""Check independent tile coordinate oracles against deliberate production faults."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAULTS = {
    "signed-input": ("divide(signed16(raw),1u<<shift)", "divide(int32_t(raw),1u<<shift)"),
    "fractional-origin": ("coordinate-=int32_t(low*8)", "coordinate-=int32_t(low*4)"),
    "implicit-clamp": ("tile.clamp(t) || !tile.mask(t)", "tile.clamp(t)"),
    "clamp-precedence": ("if(maximum)", "if(maximum && coordinate>=0)"),
    "mask-limit": ("std::min(tile.mask(t),10u)", "std::min(tile.mask(t),9u)"),
    "t-wrap": ("t?(result.texel&255):result.texel", "result.texel"),
    "mirror": ("if(wrap)", "if(!wrap)"),
    "negative-floor": ("(uint32_t(-value)+divisor-1)/divisor", "uint32_t(-value)/divisor"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="c++")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/component/n64_texture_coordinates")
    args = parser.parse_args()
    source = (ROOT / "cores/n64/src/rdp_texture_coordinates.hpp").read_text()
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
        (directory / "rdp_texture_coordinates.hpp").write_text(changed)
        binary = directory / "test"
        # A failed compilation is never counted as an oracle rejection.
        compiled = subprocess.run([
            args.compiler, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "-I" + str(directory), "-I" + str(ROOT / "cores/n64/src"),
            str(ROOT / "tests/cpp/n64_rdp_texture_coordinates_test.cpp"), "-o", str(binary),
        ], capture_output=True, text=True, timeout=60)
        (directory / "compile.log").write_text(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            raise RuntimeError(f"Compilation failed: {name}; see {directory / 'compile.log'}")
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (directory / "execution.log").write_text(result.stdout + result.stderr)
        accepted = result.returncode == 0 and result.stdout.endswith("wrap goldens PASS\n")
        rejected = result.returncode == 1 and result.stderr == "texture coordinate mismatch\n"
        if (name == "baseline" and not accepted) or (name != "baseline" and not rejected):
            raise ValueError(f"Unexpected oracle result: {name}; see {directory / 'execution.log'}")
        reports.append(dict(name=name,returncode=result.returncode,status="pass" if name=="baseline" else "rejected"))
    report = dict(status="pass",production_header_sha256=hashlib.sha256(source.encode()).hexdigest(),results=reports)
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print("N64 texture coordinates: baseline passes; all eight production faults rejected")


if __name__ == "__main__":
    main()
