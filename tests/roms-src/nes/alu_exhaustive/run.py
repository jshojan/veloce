#!/usr/bin/env python3
"""Grade the original NES ADC/SBC corpus from raw register files.

The oracle uses integer addition/subtraction and the mathematical signed range;
it does not import emulator code or use the ROM's verdict. Artifacts contain
every observed A/P pair plus a JSON report with mismatches and exact coverage.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib

from build import GROUPS, build, group_name

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def expected_registers(op: str, lhs: int, rhs: int, carry: int, decimal: int) -> tuple[int, int]:
    """6502 binary arithmetic; the NES retains D but does not perform BCD."""
    signed_lhs = lhs if lhs < 128 else lhs - 256
    signed_rhs = rhs if rhs < 128 else rhs - 256
    if op == "adc":
        total = lhs + rhs + carry
        signed_total = signed_lhs + signed_rhs + carry
        carry_out = total >= 256
    elif op == "sbc":
        total = lhs - rhs - (1 - carry)
        signed_total = signed_lhs - signed_rhs - (1 - carry)
        carry_out = total >= 0  # C means no borrow for SBC
    else:
        raise ValueError(f"unknown arithmetic operation: {op}")
    result = total % 256
    # I is held set by the ROM. PHP exposes the synthetic B and unused bits.
    flags = 0x34 | (decimal << 3)
    flags |= 0x80 if result >= 128 else 0
    flags |= 0x02 if result == 0 else 0
    flags |= 0x40 if not -128 <= signed_total <= 127 else 0
    flags |= int(carry_out)
    return result, flags


def require(condition: bool, message: str):
    if not condition:
        raise ValueError(message)


def test_oracle():
    # Hand-calculated reference points: zero, carry, borrow, both directions
    # of signed overflow, and PHP's observed flags (not an internal B latch).
    examples = (
        ("adc", 0x00, 0x00, 0, 0x00, 0x36),
        ("adc", 0x7F, 0x01, 0, 0x80, 0xF4),
        ("adc", 0xFF, 0x01, 0, 0x00, 0x37),
        ("adc", 0x80, 0x80, 0, 0x00, 0x77),
        ("adc", 0xFF, 0x00, 1, 0x00, 0x37),
        ("sbc", 0x00, 0x00, 1, 0x00, 0x37),
        ("sbc", 0x00, 0x00, 0, 0xFF, 0xB4),
        ("sbc", 0x80, 0x01, 1, 0x7F, 0x75),
        ("sbc", 0x7F, 0xFF, 1, 0x80, 0xF4),
    )
    for op, lhs, rhs, carry, result, flags in examples:
        for decimal in (0, 1):
            require(expected_registers(op, lhs, rhs, carry, decimal) ==
                    (result, flags | (decimal << 3)), f"oracle reference failed: {op} {lhs} {rhs}")


def evaluate(path: Path, operands: int, rom: bytes) -> dict:
    report = {
        "console": "nes", "suite": "alu_exhaustive", "status": "error",
        "operands_per_input": operands, "expected_cases": len(GROUPS) * operands * operands,
        "observed_cases": 0, "mismatches": 0, "groups": [], "examples": [],
        "result_file": str(path), "rom_sha256": hashlib.sha256(rom).hexdigest(),
    }
    try:
        lines = path.read_text(encoding="ascii").splitlines()
        require(bool(lines), "empty result file")
        # The NES core's public identity is PRG+CHR, excluding the iNES header.
        expected_header = f"#VELOCE 1 core=NES rom_crc32={zlib.crc32(rom[16:]):08x} channels=port"
        require(lines[0] == expected_header, "wrong result header, core, ROM identity, or channel")
        require(lines[1:4] == ["VELOCE 1 nes alu_exhaustive",
                             f"INFO operands={operands} groups={len(GROUPS)}", "REG probe=56"],
                "missing ROM header or test-port probe")
        index = -1
        count = 0
        complete = False
        trailer = None
        row_cases = min(32, operands)
        for line in lines[4:]:
            if line.startswith("BEGIN "):
                require(not complete, "group after completion")
                require(index < 0 or count == operands * operands, "previous group is incomplete")
                index += 1
                require(index < len(GROUPS), "too many groups")
                op, carry, decimal = GROUPS[index]
                require(line == f"BEGIN {group_name(op, carry, decimal)}", "wrong or reordered group")
                count = 0
                report["groups"].append({"id": group_name(op, carry, decimal),
                                         "observed_cases": 0, "mismatches": 0})
            elif line.startswith("REG ap="):
                require(index >= 0 and not complete, "register data outside a group")
                encoded = line[7:]
                require(re.fullmatch(r"[0-9a-f]+", encoded) is not None and
                        len(encoded) == row_cases * 4, "malformed or truncated packed register row")
                require(count + row_cases <= operands * operands, "too many samples in group")
                data = bytes.fromhex(encoded)
                group = report["groups"][-1]
                for offset in range(0, len(data), 2):
                    lhs, rhs = divmod(count, operands)
                    got = (data[offset], data[offset + 1])
                    expected = expected_registers(op, lhs, rhs, carry, decimal)
                    if got != expected:
                        report["mismatches"] += 1
                        group["mismatches"] += 1
                        if len(report["examples"]) < 12:
                            report["examples"].append({"group": group["id"], "lhs": f"{lhs:02x}",
                                "rhs": f"{rhs:02x}", "expected_a": f"{expected[0]:02x}",
                                "observed_a": f"{got[0]:02x}", "expected_p": f"{expected[1]:02x}",
                                "observed_p": f"{got[1]:02x}"})
                    count += 1
                    group["observed_cases"] += 1
                    report["observed_cases"] += 1
            elif line.startswith("LOG complete "):
                require(not complete and index == len(GROUPS) - 1 and count == operands * operands,
                        "premature or duplicate completion")
                require(line == f"LOG complete cases={report['expected_cases']}", "wrong completion count")
                complete = True
            elif line.startswith("#VELOCE end "):
                require(complete and trailer is None, "trailer before completion or duplicate trailer")
                trailer = dict(token.split("=", 1) for token in line.split()[2:])
                require(trailer.get("reason") == "terminator" and trailer.get("status") == "-1",
                        "ROM did not complete through the unknown-status test-port hint")
            else:
                raise ValueError(f"unexpected line: {line[:80]}")
            require(trailer is None or line.startswith("#VELOCE end "), "data after trailer")
        require(complete and trailer is not None, "missing completion or trailer")
        require(report["observed_cases"] == report["expected_cases"], "missing register observations")
        report["status"] = "fail" if report["mismatches"] else "pass"
        report["frames_used"] = int(trailer["frames"])
        report["cycles_used"] = int(trailer["cycles"])
    except (OSError, UnicodeError, ValueError, KeyError) as exc:
        report["detail"] = str(exc)
    return report


def run_rom(emulator: Path, rom_path: Path, result_path: Path, frames: int = 12000):
    env = os.environ.copy()
    for name in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(name, None)
    env.update(HEADLESS="1", FRAMES=str(frames), VELOCE_TEST_OUT=str(result_path), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], env=env, cwd=ROOT,
                          capture_output=True, text=True, timeout=180)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def test_negative_controls(emulator: Path, artifacts: Path):
    # Change an executed ADC to SBC in a small ROM. The host's expected
    # operation is unchanged, so the raw observations must score FAIL.
    mutant = bytearray(build(16))
    offset = mutant.index(bytes((0xA5, 0, 0x28, 0x65, 1, 0x85, 2, 0x08))) + 3
    mutant[offset] = 0xE5
    mutant = bytes(mutant)
    rom_path = artifacts / "negative-control.nes"
    result_path = artifacts / "negative-control.result"
    rom_path.write_bytes(mutant)
    run_rom(emulator, rom_path, result_path, frames=200)
    report = evaluate(result_path, 16, mutant)
    require(report["status"] == "fail" and report["mismatches"] > 0,
            f"changed arithmetic opcode was not rejected: {report}")
    require(report["groups"][0]["mismatches"] > 0 and
            all(g["mismatches"] == 0 for g in report["groups"][1:]), "mutation affected unexpected groups")
    (artifacts / "negative-control.json").write_text(json.dumps(report, indent=2) + "\n")
    # Missing, malformed, and reordered observations are measurement errors,
    # even if every sample remaining in the file happens to match.
    content = result_path.read_text()
    row = next(line for line in content.splitlines() if line.startswith("REG ap="))
    for name, broken in (
        ("missing-row", content.replace(row + "\n", "", 1)),
        ("malformed-row", content.replace(row, "REG ap=not_hex", 1)),
        ("reordered-group", content.replace("BEGIN adc_c0_d0", "BEGIN adc_c1_d0", 1)),
    ):
        path = artifacts / f"{name}.result"
        path.write_text(broken)
        require(evaluate(path, 16, mutant)["status"] == "error", f"{name} was accepted")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/nes_alu")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    test_oracle()
    rom_path = HERE / "alu_exhaustive.nes"
    rom = build()
    require(rom_path.read_bytes() == rom, "repository ROM differs from its source build")
    test_negative_controls(args.emulator, args.artifacts)
    result_path = args.artifacts / "alu_exhaustive.result"
    run_rom(args.emulator, rom_path, result_path)
    report = evaluate(result_path, 256, rom)
    (args.artifacts / "alu_exhaustive.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"NES ADC/SBC: {report['status'].upper()}, {report['observed_cases']}/{report['expected_cases']} "
          f"register pairs, {report['mismatches']} mismatches; artifacts: {args.artifacts}")
    if report.get("detail"):
        print(report["detail"])
    for example in report["examples"]:
        print(json.dumps(example))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
        print(f"NES ADC/SBC harness error: {exc}", file=sys.stderr)
        sys.exit(1)
