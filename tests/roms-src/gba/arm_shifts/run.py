#!/usr/bin/env python3
"""Grade ARM register shifts from raw mGBA-channel result/CPSR observations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib

from build import GROUPS, build, build_channel_probe, group_name

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
MASK = (1 << 32) - 1


def require(condition, message):
    if not condition:
        raise ValueError(message)


def expected_registers(op, value, amount, carry, overflow):
    """Mathematical shifts/rotations; does not import emulator implementation."""
    amount %= 256
    if not amount:
        result, carry_out = value, carry
    elif op == "lsl":
        result = (value * (2 ** amount)) & MASK
        carry_out = ((value * (2 ** (amount - 1))) // (2 ** 31)) & 1 if amount <= 32 else 0
    elif op == "lsr":
        result = value // (2 ** amount)
        carry_out = (value // (2 ** (amount - 1))) & 1
    elif op == "asr":
        signed = value if value < 2 ** 31 else value - 2 ** 32
        result = (signed // (2 ** amount)) & MASK
        carry_out = (signed // (2 ** (amount - 1))) & 1
    elif op == "ror":
        rotation = amount % 32
        result = value if not rotation else ((value // (2 ** rotation)) |
                 ((value % (2 ** rotation)) * (2 ** (32 - rotation))))
        carry_out = result // (2 ** 31)
    else:
        raise ValueError(f"unknown shift: {op}")
    # System mode, IRQ/FIQ disabled, ARM state; MOVS preserves V.
    flags = 0xDF | (int(result >= 2 ** 31) << 31) | (int(result == 0) << 30)
    flags |= carry_out << 29 | overflow << 28
    return result, flags


def test_oracle():
    examples = (("lsl", 0x80000001, 0, 1, 0x80000001, 1),
                ("lsl", 0x80000001, 32, 0, 0, 1),
                ("lsl", 0x80000001, 33, 1, 0, 0),
                ("lsr", 0x80000001, 32, 0, 0, 1),
                ("lsr", 0x80000001, 255, 1, 0, 0),
                ("asr", 0x80000001, 1, 0, 0xC0000000, 1),
                ("asr", 0x80000001, 255, 0, MASK, 1),
                ("ror", 0x80000001, 1, 0, 0xC0000000, 1),
                ("ror", 0x80000001, 32, 0, 0x80000001, 1),
                ("ror", 1, 255, 0, 2, 0))
    for op, value, amount, carry, result, out_carry in examples:
        for overflow in (0, 1):
            got = expected_registers(op, value, amount, carry, overflow)
            require(got[0] == result and (got[1] >> 29) & 1 == out_carry and
                    (got[1] >> 28) & 1 == overflow, f"oracle reference failed: {op}/{amount}")


def evaluate(path, amounts, rom):
    report = {"console": "gba", "suite": "arm_shifts", "status": "error",
              "expected_cases": len(GROUPS) * amounts, "observed_cases": 0,
              "mismatches": 0, "groups": [], "examples": [],
              "rom_sha256": hashlib.sha256(rom).hexdigest(), "result_file": str(path),
              "coverage": {"instructions": "ARM MOVS register-specified LSL/LSR/ASR/ROR",
                           "amounts_per_group": amounts, "operand_patterns": 16,
                           "carry_inputs": [0, 1], "overflow_inputs": [0, 1],
                           "timing": "unverified"}}
    try:
        lines = path.read_text(encoding="ascii").splitlines()
        require(lines[:4] == [f"#VELOCE 1 core=GBA rom_crc32={zlib.crc32(rom):08x} channels=mgba",
                "VELOCE 1 gba arm_shifts", f"INFO amounts={amounts} groups={len(GROUPS)}",
                "REG probe=00001dea"], "wrong core, ROM identity, channel, header or handshake")
        cursor = 4
        for op, value, carry, overflow in GROUPS:
            name = group_name(op, value, carry, overflow)
            require(lines[cursor] == f"BEGIN {name}", "missing or reordered group")
            cursor += 1
            group = {"id": name, "observed_cases": 0, "mismatches": 0}
            report["groups"].append(group)
            for amount in range(amounts):
                row = lines[cursor]
                cursor += 1
                require(re.fullmatch(r"REG rp=[0-9a-f]{16}", row), "malformed result/CPSR row")
                got = (int(row[7:15], 16), int(row[15:], 16))
                expected = expected_registers(op, value, amount, carry, overflow)
                if got != expected:
                    report["mismatches"] += 1
                    group["mismatches"] += 1
                    if len(report["examples"]) < 12:
                        report["examples"].append({"group": name, "amount": amount,
                                                  "expected": expected, "observed": got})
                report["observed_cases"] += 1
                group["observed_cases"] += 1
        require(lines[cursor] == f"LOG complete cases={report['expected_cases']}",
                "missing or incorrect completion count")
        require(len(lines) == cursor + 2 and lines[-1].startswith("#VELOCE end "),
                "missing trailer or extra observations")
        trailer = dict(token.split("=", 1) for token in lines[-1].split()[2:])
        require(trailer.get("reason") == "terminator" and trailer.get("status") == "-1",
                "completion was not an unknown-status hint")
        report["frames_used"] = int(trailer["frames"])
        report["cycles_used"] = int(trailer["cycles"])
        report["status"] = "fail" if report["mismatches"] else "pass"
    except (OSError, UnicodeError, ValueError, KeyError, IndexError) as exc:
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
    mutant = bytearray(build(16))
    # First operand with a nonzero result: LSL 1, C=V=0 becomes LSR.
    # Zero-operand groups stay unchanged; the oracle still expects LSL.
    instruction = (0xE1B00211).to_bytes(4, "little")
    offsets = [i for i in range(0xC0, len(mutant), 4) if mutant[i:i + 4] == instruction]
    require(len(offsets) == 64, "unexpected number of LSL instruction sites")
    mutant[offsets[4]:offsets[4] + 4] = (0xE1B00231).to_bytes(4, "little")
    mutant = bytes(mutant)
    rom_path = artifacts / "negative-control.gba"
    result_path = artifacts / "negative-control.result"
    rom_path.write_bytes(mutant)
    run_rom(emulator, rom_path, result_path)
    report = evaluate(result_path, 16, mutant)
    require(report["status"] == "fail" and report["groups"][4]["mismatches"] > 0,
            f"shift mutation was not rejected: {report}")
    require(all(g["mismatches"] == 0 for i, g in enumerate(report["groups"]) if i != 4),
            "mutation affected unexpected groups")
    (artifacts / "negative-control.json").write_text(json.dumps(report, indent=2) + "\n")
    content = result_path.read_text()
    row = next(line for line in content.splitlines() if line.startswith("REG rp="))
    first = group_name(*GROUPS[0])
    for name, broken in (
        ("missing-row", content.replace(row + "\n", "", 1)),
        ("malformed-row", content.replace(row, "REG rp=not_hex", 1)),
        ("reordered-group", content.replace(f"BEGIN {first}", f"BEGIN {group_name(*GROUPS[1])}", 1)),
        ("truncated-file", content[:content.index("LOG complete")]),
    ):
        path = artifacts / f"{name}.result"
        path.write_text(broken)
        require(evaluate(path, 16, mutant)["status"] == "error", f"{name} accepted")


def test_channel(emulator, artifacts):
    rom = build_channel_probe()
    rom_path = artifacts / "channel-probe.gba"
    result_path = artifacts / "channel-probe.result"
    rom_path.write_bytes(rom)
    run_rom(emulator, rom_path, result_path)
    lines = result_path.read_text(encoding="ascii").splitlines()
    require(lines[:-1] == [f"#VELOCE 1 core=GBA rom_crc32={zlib.crc32(rom):08x} channels=mgba",
                          "A" * 255, "INFO enabled"],
            "disabled output leaked, maximum-length message damaged, or extra newline")
    require(lines[-1].startswith("#VELOCE end reason=terminator ") and lines[-1].endswith(" status=-1"),
            "channel probe did not complete")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/gba_shifts")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    test_oracle()
    rom = build()
    require(len(rom) == 0x40000, "ROM is not 256 KiB")
    require((sum(rom[0xA0:0xBE]) + 0x19) & 255 == 0, "bad header checksum")
    rom_path = HERE / "arm_shifts.gba"
    require(rom_path.read_bytes() == rom, "archived image differs from generator")
    test_channel(args.emulator, args.artifacts)
    negative_controls(args.emulator, args.artifacts)
    result_path = args.artifacts / "arm_shifts.result"
    run_rom(args.emulator, rom_path, result_path)
    report = evaluate(result_path, 256, rom)
    (args.artifacts / "arm_shifts.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"GBA ARM shifts: {report['status'].upper()}, {report['observed_cases']}/{report['expected_cases']} "
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
        print(f"GBA shift harness error: {exc}", file=sys.stderr)
        sys.exit(1)
