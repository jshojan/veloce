#!/usr/bin/env python3
"""Grade actual CPU-store and DMA RSP IMEM rewrite observations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib

from build import build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SUITE = "rsp_imem_rewrite"
CASES = 2560
WORDS = 4


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(emulator, image, result):
    env = os.environ.copy()
    for key in ("INPUT", "INPUT_FILE", "DEBUG", "TRACE", "TRACE_FILE", "VELOCE_TEST_RESETS"):
        env.pop(key, None)
    env.update(HEADLESS="1", FRAMES="10000", VELOCE_TEST_OUT=str(result.resolve()),
               VELOCE_TEST_EXIT="1")
    completed = subprocess.run([str(emulator.resolve()), str(image.resolve())], cwd=ROOT,
                               env=env, capture_output=True, text=True, timeout=30)
    require(completed.returncode == 0, completed.stdout + completed.stderr)
    result.with_suffix(".log").write_text(completed.stdout + completed.stderr)


def evaluate(path, rom):
    lines = path.read_text(encoding="ascii").splitlines()
    require(len(lines) == CASES + 6, "missing or extra register-output lines")
    require(lines[:3] == [
        f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug",
        f"VELOCE 1 n64 {SUITE}", f"BEGIN data cases={CASES} words={WORDS}"],
        "wrong ROM identity or declared domain")
    mismatches = []
    for index, line in enumerate(lines[3:3 + CASES]):
        require(re.fullmatch(r"REG r=[0-9a-f]{32}", line) is not None,
                f"malformed register row {index}")
        actual = tuple(int(line[6 + offset:14 + offset], 16) for offset in (0, 8, 16, 24))
        value = (index & 255) if index < 512 else ((index - 512) & 255)
        slot = 0 if index < 512 else (index - 512) & 1023
        expected = (value, 0x24020000 | value, 3, (slot * 4 + 12) & 4092)
        if actual != expected:
            mismatches.append({"mode": "cpu-store" if (index < 256 or 512 <= index < 1536) else "sp-dma",
                               "slot": slot, "value": value, "expected": expected, "actual": actual})
    require(lines[3 + CASES:5 + CASES] == [
        f"LOG complete cases={CASES}", "END observations_complete"],
        "missing or false completion marker")
    require(re.fullmatch(r"#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1",
                         lines[-1]) is not None, "invalid file-sink trailer")
    return mismatches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path,
                        default=ROOT / "build/artifacts/owned/n64_rsp_imem_rewrite")
    args = parser.parse_args()
    original = build()
    require((HERE / "rsp_imem_rewrite.z64").read_bytes() == original,
            "checked-in image differs from original source build")
    args.artifacts.mkdir(parents=True, exist_ok=True)
    image = args.artifacts / "control.z64"
    image.write_bytes(original)
    result = args.artifacts / "control.result"
    run(args.emulator, image, result)
    require(not evaluate(result, original), "RSP IMEM control register mismatch")

    mutant = build(wrong_instruction=True)
    image = args.artifacts / "executed-mutant.z64"
    image.write_bytes(mutant)
    result = args.artifacts / "executed-mutant.result"
    run(args.emulator, image, result)
    wrong = evaluate(result, mutant)
    require(len(wrong) == CASES, "executed wrong-instruction ROM escaped oracle")

    disarmed = build(no_dma=True)
    image = args.artifacts / "disarmed-dma.z64"
    image.write_bytes(disarmed)
    result = args.artifacts / "disarmed-dma.result"
    run(args.emulator, image, result)
    skipped = evaluate(result, disarmed)
    dma_mismatches = sum(row["mode"] == "sp-dma" for row in skipped)
    require(len(skipped) == dma_mismatches == 1279,
            "disarmed SP DMA escaped oracle or changed CPU-store rows")

    control = (args.artifacts / "control.result").read_text()
    first = next(line for line in control.splitlines() if line.startswith("REG r="))
    corrupt = args.artifacts / "corrupt.result"
    corrupt.write_text(control.replace(first, first[:6] + "xyz" + first[9:], 1))
    try:
        evaluate(corrupt, original)
    except AssertionError:
        pass
    else:
        raise AssertionError("malformed register capture escaped oracle")

    report = {"suite": SUITE, "status": "pass", "cases": CASES,
              "actual_register_words": CASES * WORDS,
              "cpu_store_cases": 1280, "sp_dma_cases": 1280,
              "imem_aligned_slots_per_path": 1024,
              "executed_mutant_mismatches": len(wrong),
              "disarmed_dma_mismatches": dma_mismatches,
              "rom_sha256": hashlib.sha256(original).hexdigest()}
    (args.artifacts / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"N64 RSP IMEM rewrite: {CASES} cases / {CASES * WORDS} actual words pass; "
          "executed mutants and malformed capture rejected")


if __name__ == "__main__":
    sys.exit(main())
