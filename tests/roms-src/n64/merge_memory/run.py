#!/usr/bin/env python3
"""Independently grade every original N64 guest register observation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zlib
from build import MASK, GUARD, groups, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def expected(width, store, left, lane, memory, register):
    # Construct architectural byte lanes directly; do not reproduce core masks.
    limit = (1 << (8 * width)) - 1
    mem = bytearray((memory & limit).to_bytes(width, "big"))
    reg = bytearray((register & limit).to_bytes(width, "big"))
    if store:
        if left: mem[lane:] = reg[:width - lane]
        else: mem[:lane + 1] = reg[width - lane - 1:]
        if width == 4:
            mem.extend((GUARD & 0xffffffff).to_bytes(4, "big"))
        result = int.from_bytes(mem, "big")
    else:
        if left: reg[:width - lane] = mem[lane:]
        else: reg[width - lane - 1:] = mem[:lane + 1]
        result = int.from_bytes(reg, "big")
        if width == 4 and result >= 1 << 31: result |= 0xffffffff00000000
    return (result,)


def references():
    for width, store, left, lane, mem, reg, want in (
        (4, False, True, 1, 0x01234567, 0x89abcdef, 0x234567ef),
        (4, False, False, 1, 0x01234567, 0x89abcdef, 0xffffffff89ab0123),
        (4, True, True, 1, 0x01234567, 0x89abcdef, 0x0189abcd2468ace0),
        (4, True, False, 1, 0x01234567, 0x89abcdef, 0xcdef45672468ace0),
        (8, False, True, 1, 0x0123456789abcdef, 0xfedcba9876543210, 0x23456789abcdef10),
        (8, False, False, 1, 0x0123456789abcdef, 0xfedcba9876543210, 0xfedcba9876540123),
        (8, True, True, 1, 0x0123456789abcdef, 0xfedcba9876543210, 0x01fedcba98765432),
        (8, True, False, 1, 0x0123456789abcdef, 0xfedcba9876543210, 0x3210456789abcdef),
        (4, False, True, 0, 0x80000000, 0, 0xffffffff80000000),
        (8, False, False, 7, MASK, 0, MASK),
    ):
        require(expected(width, store, left, lane, mem, reg) == (want,), "oracle reference failed")


def evaluate(path, rom, byte_values=256):
    domain = groups(byte_values)
    total = sum(len(values) ** 2 for *_, values in domain)
    report = {"console": "n64", "suite": "merge_memory", "status": "error",
              "expected_cases": total, "observed_cases": 0, "graded_cases": 0,
              "undefined_cases": 0, "mismatches": 0, "groups": [], "examples": [],
              "result_file": str(path), "rom_sha256": hashlib.sha256(rom).hexdigest()}
    try:
        with path.open(encoding="ascii") as stream:
            require(stream.readline() == f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n", "wrong ROM/core/channel")
            require(stream.readline() == "VELOCE 1 n64 merge_memory\n", "wrong ROM header")
            require(stream.readline() == f"INFO boundary_patterns=16 byte_values={byte_values} groups={len(domain)}\n", "wrong domain")
            for name, _, _, width, store, left, lane, values in domain:
                size = len(values)
                words = 1
                require(stream.readline() == f"BEGIN {name} cases={size * size} words={words}\n", "missing/reordered group")
                group = {"id": name, "observed_cases": 0, "graded_cases": 0, "undefined_cases": 0, "mismatches": 0}
                report["groups"].append(group)
                for lhs in values:
                    for index in range(size):
                        line = stream.readline()
                        require(re.fullmatch(r"REG r=[0-9a-f]{" + str(words * 16) + r"}\n", line), "missing/malformed register observation")
                        got = tuple(int(line[6 + i * 16:22 + i * 16], 16) for i in range(words))
                        rhs = values[index]
                        want = expected(width, store, left, lane, lhs, rhs)
                        report["observed_cases"] += 1
                        group["observed_cases"] += 1
                        if want is None:
                            report["undefined_cases"] += 1; group["undefined_cases"] += 1
                        else:
                            report["graded_cases"] += 1; group["graded_cases"] += 1
                            if got != want:
                                report["mismatches"] += 1; group["mismatches"] += 1
                                if len(report["examples"]) < 16:
                                    report["examples"].append({"operation": name, "lhs": f"{lhs:016x}", "rhs": f"{rhs:016x}",
                                                               "expected": [f"{v:016x}" for v in want], "observed": [f"{v:016x}" for v in got]})
            require(stream.readline() == f"LOG complete cases={total}\n", "missing completion")
            require(stream.readline() == "END observations_complete\n", "wrong END")
            trailer = stream.readline()
            require(re.fullmatch(r"#VELOCE end reason=terminator frames=\d+ cycles=\d+ status=-1\n", trailer), "missing/ungraded trailer")
            require(stream.read() == "", "data after trailer")
            metadata = dict(token.split("=", 1) for token in trailer.split()[2:])
            report["frames_used"] = int(metadata["frames"])
            report["instruction_steps"] = int(metadata["cycles"])
            report["status"] = "fail" if report["mismatches"] else "pass"
    except (ValueError, OSError, UnicodeError) as error:
        report["detail"] = str(error)
    return report


def run(emulator, rom_path, path):
    env = os.environ.copy()
    for key in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(key, None)
    env.update(HEADLESS="1", FRAMES="30000", VELOCE_TEST_OUT=str(path.resolve()), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], cwd=ROOT,
                          env=env, capture_output=True, text=True, timeout=360)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def controls(emulator, artifacts):
    rom = bytearray(build(16))
    for left_opcode, right_opcode in ((0x22, 0x26), (0x2a, 0x2e)):
        stimulus = ((left_opcode << 26) | (24 << 21) | (22 << 16) | 8).to_bytes(4, "big")
        position = rom.index(stimulus, 0x1000)
        replacement = ((right_opcode << 26) | (24 << 21) | (22 << 16) | 8).to_bytes(4, "big")
        rom[position:position + 4] = replacement
    image = artifacts / "negative.z64"
    image.write_bytes(rom)
    path = artifacts / "negative.result"
    run(emulator, image, path)
    report = evaluate(path, rom, 16)
    require(report["status"] == "fail" and all(g["mismatches"] > 0 if i in (0, 24) else g["mismatches"] == 0
                for i, g in enumerate(report["groups"])), "executed mutation not isolated/rejected")
    (artifacts / "negative.json").write_text(json.dumps(report, indent=2) + "\n")
    content = path.read_text()
    row = next(line for line in content.splitlines() if line.startswith("REG r="))
    for name, broken in (
        ("missing-row", content.replace(row + "\n", "", 1)),
        ("malformed-row", content.replace(row, "REG r=invalid", 1)),
        ("reordered-group", content.replace("BEGIN lwl_boundary_lane0", "BEGIN lwr_boundary_lane0", 1)),
        ("false-pass", content.replace("END observations_complete", "END 0/0")),
        ("missing-trailer", content[:content.index("#VELOCE end ")]),
        ("unterminated-trailer", content.rstrip("\n")),
        ("trailing-data", content + "REG r=0000000000000000\n"),
    ):
        damaged = artifacts / f"{name}.result"
        damaged.write_text(broken)
        rejected = evaluate(damaged, rom, 16)
        require(rejected["status"] == "error", f"damaged file accepted: {name}")
        (artifacts / f"{name}.json").write_text(json.dumps(rejected, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/n64_merge_memory")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    references()
    rom = build()
    require((HERE / "merge_memory.z64").read_bytes() == rom, "archive differs from generator")
    controls(args.emulator, args.artifacts)
    path = args.artifacts / "merge_memory.result"
    run(args.emulator, HERE / "merge_memory.z64", path)
    report = evaluate(path, rom)
    (args.artifacts / "merge_memory.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"N64 merge memory: {report['status'].upper()} {report['graded_cases']} graded; "
          f"{report['observed_cases']}/{report['expected_cases']} observations; {report['undefined_cases']} undefined; mismatches={report['mismatches']}")
    if report.get("detail"): print(report["detail"])
    for example in report["examples"]: print(json.dumps(example))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
