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
from build import MASK, OPS, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
TOTAL = sum(16 * (256 if kind == "shift" else 16) for _, _, _, kind in OPS)


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sx(value, bits):
    value %= 1 << bits
    return (value - (1 << bits) if value >= 1 << (bits - 1) else value) & MASK


def signed(value, bits):
    value %= 1 << bits
    return value - (1 << bits) if value >= 1 << (bits - 1) else value


def expected(name, lhs, rhs):
    if name == "addu": return (sx(lhs + rhs, 32),)
    if name == "subu": return (sx(lhs - rhs, 32),)
    if name == "daddu": return ((lhs + rhs) & MASK,)
    if name == "dsubu": return ((lhs - rhs) & MASK,)
    if name == "and": return (lhs & rhs,)
    if name == "or": return (lhs | rhs,)
    if name == "xor": return (lhs ^ rhs,)
    if name == "nor": return ((lhs | rhs) ^ MASK,)
    if name == "slt": return (int(signed(lhs, 64) < signed(rhs, 64)),)
    if name == "sltu": return (int(lhs < rhs),)
    if name in ("sllv", "srlv", "srav", "dsllv", "dsrlv", "dsrav"):
        bits = 64 if name.startswith("d") else 32
        amount = rhs % bits
        value = signed(lhs, bits) if "ra" in name else lhs % (1 << bits)
        result = value * (2 ** amount) if "ll" in name else value // (2 ** amount)
        return (sx(result, bits),)
    if name in ("mult", "multu"):
        x, y = (signed(lhs, 32), signed(rhs, 32)) if name == "mult" else (lhs % (1 << 32), rhs % (1 << 32))
        product = (x * y) % (1 << 64)
        return sx(product, 32), sx(product // (1 << 32), 32)
    if name in ("div", "divu"):
        x, y = (signed(lhs, 32), signed(rhs, 32)) if name == "div" else (lhs % (1 << 32), rhs % (1 << 32))
        # Architecturally undefined cases are observed but never scored as accuracy.
        if y == 0 or (name == "div" and x == -(1 << 31) and y == -1):
            return None
        quotient = abs(x) // abs(y) * (-1 if (x < 0) != (y < 0) else 1)
        return sx(quotient, 32), sx(x - quotient * y, 32)
    raise ValueError("unknown instruction")


def references():
    for name, lhs, rhs, want in (
        ("addu", 0x7fffffff, 1, (0xffffffff80000000,)),
        ("subu", 0, 1, (MASK,)), ("daddu", MASK, 1, (0,)),
        ("dsllv", 1, 63, (1 << 63,)), ("dsllv", 1, 64, (1,)),
        ("srlv", 0xffffffff80000000, 0, (0xffffffff80000000,)),
        ("srav", 0xffffffff80000000, 31, (MASK,)),
        ("dsrav", 1 << 63, 63, (MASK,)), ("slt", MASK, 0, (1,)),
        ("mult", MASK, 2, (MASK - 1, MASK)),
        ("multu", MASK, MASK, (1, MASK - 1)),
        ("div", MASK - 6, 3, (MASK - 1, MASK)),
        ("divu", 0xffffffff, 2, (0x7fffffff, 1)),
    ):
        require(expected(name, lhs, rhs) == want, f"oracle reference failed: {name}")
    require(expected("div", 0x80000000, MASK) is None and expected("divu", 1, 0) is None,
            "undefined divide must not contribute to score")


def evaluate(path, rom):
    report = {"console": "n64", "suite": "integer_registers", "status": "error",
              "expected_cases": TOTAL, "observed_cases": 0, "graded_cases": 0,
              "undefined_cases": 0, "mismatches": 0, "groups": [], "examples": [],
              "result_file": str(path), "rom_sha256": hashlib.sha256(rom).hexdigest()}
    try:
        with path.open(encoding="ascii") as stream:
            require(stream.readline() == f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n", "wrong ROM/core/channel")
            require(stream.readline() == "VELOCE 1 n64 integer_registers\n", "wrong ROM header")
            require(stream.readline() == f"INFO patterns=16 shift_amounts=256 groups={len(OPS)}\n", "wrong domain")
            for name, _, values, kind in OPS:
                size = 256 if kind == "shift" else 16
                words = 2 if kind == "hilo" else 1
                require(stream.readline() == f"BEGIN {name} cases={16 * size} words={words}\n", "missing/reordered group")
                group = {"id": name, "observed_cases": 0, "graded_cases": 0, "undefined_cases": 0, "mismatches": 0}
                report["groups"].append(group)
                for lhs in values:
                    for index in range(size):
                        line = stream.readline()
                        require(re.fullmatch(r"REG r=[0-9a-f]{" + str(words * 16) + r"}\n", line), "missing/malformed register observation")
                        got = tuple(int(line[6 + i * 16:22 + i * 16], 16) for i in range(words))
                        rhs = index if kind == "shift" else values[index]
                        want = expected(name, lhs, rhs)
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
            require(stream.readline() == f"LOG complete cases={TOTAL}\n", "missing completion")
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
    env.update(HEADLESS="1", FRAMES="200", VELOCE_TEST_OUT=str(path.resolve()), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], cwd=ROOT,
                          env=env, capture_output=True, text=True, timeout=60)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def controls(emulator, artifacts):
    rom = bytearray(build())
    stimulus = ((18 << 21) | (22 << 16) | (21 << 11) | 0x21).to_bytes(4, "big")
    position = rom.index(stimulus, 0x1000)
    rom[position + 3] = 0x23  # First ADDU executes SUBU.
    image = artifacts / "negative.z64"
    image.write_bytes(rom)
    path = artifacts / "negative.result"
    run(emulator, image, path)
    report = evaluate(path, rom)
    require(report["status"] == "fail" and report["groups"][0]["mismatches"] > 0 and
            all(g["mismatches"] == 0 for g in report["groups"][1:]), "executed mutation not isolated/rejected")
    (artifacts / "negative.json").write_text(json.dumps(report, indent=2) + "\n")
    content = path.read_text()
    row = next(line for line in content.splitlines() if line.startswith("REG r="))
    for name, broken in (
        ("missing-row", content.replace(row + "\n", "", 1)),
        ("malformed-row", content.replace(row, "REG r=invalid", 1)),
        ("reordered-group", content.replace("BEGIN addu", "BEGIN subu", 1)),
        ("false-pass", content.replace("END observations_complete", "END 0/0")),
        ("missing-trailer", content[:content.index("#VELOCE end ")]),
        ("unterminated-trailer", content.rstrip("\n")),
        ("trailing-data", content + "REG r=0000000000000000\n"),
    ):
        damaged = artifacts / f"{name}.result"
        damaged.write_text(broken)
        rejected = evaluate(damaged, rom)
        require(rejected["status"] == "error", f"damaged file accepted: {name}")
        (artifacts / f"{name}.json").write_text(json.dumps(rejected, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/n64_integer")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    references()
    rom = build()
    require((HERE / "integer_registers.z64").read_bytes() == rom, "archive differs from generator")
    controls(args.emulator, args.artifacts)
    path = args.artifacts / "integer_registers.result"
    run(args.emulator, HERE / "integer_registers.z64", path)
    report = evaluate(path, rom)
    (args.artifacts / "integer_registers.json").write_text(json.dumps(report, indent=2) + "\n")
    if report["status"] == "pass":
        # Execute both common byte-swapped archives through frontend discovery,
        # including their distinct raw-file CRC identities.
        for extension, width in (("v64", 2), ("n64", 4)):
            swapped = b"".join(rom[i:i + width][::-1] for i in range(0, len(rom), width))
            image = args.artifacts / f"integer_registers.{extension}"
            image.write_bytes(swapped)
            result = args.artifacts / f"{extension}.result"
            run(args.emulator, image, result)
            alternate = evaluate(result, swapped)
            (args.artifacts / f"{extension}.json").write_text(json.dumps(alternate, indent=2) + "\n")
            require(alternate["status"] == "pass" and alternate["graded_cases"] == report["graded_cases"],
                    f"{extension} execution differs")
    print(f"N64 integer: {report['status'].upper()} {report['graded_cases']} graded; "
          f"{report['observed_cases']}/{TOTAL} observations; {report['undefined_cases']} undefined; mismatches={report['mismatches']}")
    if report.get("detail"): print(report["detail"])
    for example in report["examples"]: print(json.dumps(example))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
