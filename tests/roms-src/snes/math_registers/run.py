#!/usr/bin/env python3
"""Grade observed SNES multiplication/division with an independent integer oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib

from build import DIVIDENDS, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def evaluate(path, operands, rom):
    report = {"console": "snes", "suite": "math_registers", "status": "error",
              "expected_cases": operands * operands + len(DIVIDENDS) * operands,
              "observed_cases": 0, "mismatches": 0, "groups": [], "examples": [],
              "rom_sha256": hashlib.sha256(rom).hexdigest(), "result_file": str(path),
              "coverage": {"multiplication": f"{operands}x{operands} unsigned operands",
                           "division": {"dividends": list(DIVIDENDS), "divisors": operands},
                           "timing": "settled outputs only; latency/intermediate values unverified"}}
    try:
        lines = path.read_text(encoding="ascii").splitlines()
        expected_header = f"#VELOCE 1 core=SNES rom_crc32={zlib.crc32(rom):08x} channels=port,blargg6000,stp,sram,spcport"
        require(lines[:5] == [expected_header, "VELOCE 1 snes math_registers",
                              f"INFO operands={operands} dividends={len(DIVIDENDS)}", "REG probe=56",
                              "REG map=565aa5"],
                "wrong core, ROM identity, channel, or ROM header/probe")
        cursor = 5
        for group_id, dividend in [("multiply", None)] + [(f"div_{n}", n) for n in DIVIDENDS]:
            require(lines[cursor] == f"BEGIN {group_id}", "missing or reordered group")
            cursor += 1
            count = operands * operands if dividend is None else operands
            group = {"id": group_id, "expected_cases": count, "observed_cases": 0, "mismatches": 0}
            report["groups"].append(group)
            for index in range(count):
                line = lines[cursor]
                cursor += 1
                if dividend is None:
                    lhs, rhs = divmod(index, operands)
                    require(re.fullmatch(r"REG mul=[0-9a-f]{8}", line), "malformed multiplication row")
                    got = (int(line[8:12], 16), int(line[12:], 16))
                    expected = (lhs * rhs, rhs)  # Multiplication also sets RDDIV to WRMPYB
                    inputs = {"multiplicand": lhs, "multiplier": rhs}
                else:
                    require(re.fullmatch(r"REG div=[0-9a-f]{8}", line), "malformed division row")
                    got = (int(line[8:12], 16), int(line[12:], 16))
                    expected = divmod(dividend, index) if index else (65535, dividend)
                    inputs = {"dividend": dividend, "divisor": index}
                if got != expected:
                    report["mismatches"] += 1
                    group["mismatches"] += 1
                    if len(report["examples"]) < 12:
                        report["examples"].append({"group": group_id, **inputs,
                                                   "expected": expected, "observed": got})
                group["observed_cases"] += 1
                report["observed_cases"] += 1
        require(lines[cursor] == f"LOG complete cases={report['expected_cases']}",
                "missing or wrong completion count")
        require(len(lines) == cursor + 2 and lines[-1].startswith("#VELOCE end "),
                "missing trailer or extra observations")
        trailer = dict(token.split("=", 1) for token in lines[-1].split()[2:])
        require(trailer.get("reason") == "terminator" and trailer.get("status") == "-1",
                "ROM did not finish through unknown-status hint")
        report["frames_used"] = int(trailer["frames"])
        report["cycles_used"] = int(trailer["cycles"])
        report["status"] = "fail" if report["mismatches"] else "pass"
    except (OSError, UnicodeError, ValueError, IndexError, KeyError) as exc:
        report["detail"] = str(exc)
    return report


def run_rom(emulator, rom_path, result_path):
    env = os.environ.copy()
    for name in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(name, None)
    env.update(HEADLESS="1", FRAMES="12000", VELOCE_TEST_OUT=str(result_path.resolve()),
               VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], cwd=ROOT,
                          env=env, capture_output=True, text=True, timeout=180)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def negative_controls(emulator, artifacts):
    # Write the multiplier to WRDIVB instead of WRMPYB. The host's multiplication
    # expectation stays unchanged, while observed hardware bytes must fail.
    mutant = bytearray(build(16))
    offset = mutant.index(bytes((0xA5, 1, 0x8D, 0x03, 0x42))) + 3
    mutant[offset] = 0x06
    mutant = bytes(mutant)
    rom_path = artifacts / "negative-control.sfc"
    result_path = artifacts / "negative-control.result"
    rom_path.write_bytes(mutant)
    run_rom(emulator, rom_path, result_path)
    report = evaluate(result_path, 16, mutant)
    require(report["status"] == "fail" and report["groups"][0]["mismatches"] > 0,
            f"changed hardware stimulus did not fail: {report}")
    require(all(g["mismatches"] == 0 for g in report["groups"][1:]), "unexpected division failure")
    (artifacts / "negative-control.json").write_text(json.dumps(report, indent=2) + "\n")
    content = result_path.read_text()
    row = next(line for line in content.splitlines() if line.startswith("REG mul="))
    for name, broken in (
        ("missing-row", content.replace(row + "\n", "", 1)),
        ("malformed-row", content.replace(row, "REG mul=not_hex", 1)),
        ("reordered-group", content.replace("BEGIN div_0", "BEGIN div_1", 1)),
        ("truncated-file", content[:content.index("LOG complete")]),
    ):
        path = artifacts / f"{name}.result"
        path.write_text(broken)
        require(evaluate(path, 16, mutant)["status"] == "error", f"{name} accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/snes_math")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    rom = build()
    require(len(rom) == 0x8000, "ROM is not exactly 32 KiB")
    rom_path = HERE / "math_registers.sfc"
    require(rom_path.read_bytes() == rom, "archived ROM differs from source rebuild")
    require(sum(rom) & 65535 == int.from_bytes(rom[0x7FDE:0x7FE0], "little"), "invalid ROM checksum")
    negative_controls(args.emulator, args.artifacts)
    result_path = args.artifacts / "math_registers.result"
    run_rom(args.emulator, rom_path, result_path)
    report = evaluate(result_path, 256, rom)
    (args.artifacts / "math_registers.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"SNES math: {report['status'].upper()}, {report['observed_cases']}/{report['expected_cases']} "
          f"cases, {report['mismatches']} mismatches; artifacts: {args.artifacts}")
    if report.get("detail"):
        print(report["detail"])
    for example in report["examples"]:
        print(json.dumps(example))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
        print(f"SNES math harness error: {exc}", file=sys.stderr)
        sys.exit(1)
