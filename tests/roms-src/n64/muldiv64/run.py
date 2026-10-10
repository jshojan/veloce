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
from build import MASK, OPS, TOTAL, build

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


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
    op = name.split("_")[0]
    if op in ("dmult", "dmultu"):
        x, y = (signed(lhs, 64), signed(rhs, 64)) if op == "dmult" else (lhs, rhs)
        product = (x * y) % (1 << 128)
        return product % (1 << 64), product // (1 << 64)
    if op in ("ddiv", "ddivu"):
        x, y = (signed(lhs, 64), signed(rhs, 64)) if op == "ddiv" else (lhs, rhs)
        if y == 0 or (op == "ddiv" and x == -(1 << 63) and y == -1):
            return None
        # Quotient truncates toward zero; remainder has dividend's sign.
        quotient = abs(x) // abs(y) * (-1 if (x < 0) != (y < 0) else 1)
        return quotient & MASK, (x - quotient * y) & MASK
    raise ValueError("unknown multiply/divide instruction")


def references():
    for op, lhs, rhs, want in (
        ("dmultu", MASK, MASK, (1, MASK - 1)),
        ("dmult", MASK, MASK, (1, 0)),
        ("dmult", 1 << 63, MASK, (1 << 63, 0)),
        ("dmult", 1 << 63, 2, (0, MASK)),
        ("dmultu", 1 << 32, 1 << 32, (0, 1)),
        ("dmultu", MASK, 2, (MASK - 1, 1)),
        ("dmult", 0x7fffffffffffffff, 2, (MASK - 1, 0)),
        ("ddiv", 1 << 63, 2, (0xc000000000000000, 0)),
        ("ddiv", 1 << 63, 3, (0xd555555555555556, MASK - 1)),
        ("ddiv", MASK - 6, 3, (MASK - 1, MASK)),
        ("ddiv", 7, MASK - 2, (MASK - 1, 1)),
        ("ddivu", MASK, 2, (0x7fffffffffffffff, 1)),
    ):
        require(expected(op, lhs, rhs) == want, f"oracle reference failed: {op}")
    require(expected("ddiv", 1 << 63, MASK) is None and expected("ddivu", 1, 0) is None,
            "undefined division must be unscored")


def evaluate(path, rom):
    report = {"console": "n64", "suite": "muldiv64", "status": "error",
              "expected_cases": TOTAL, "observed_cases": 0, "graded_cases": 0,
              "undefined_cases": 0, "mismatches": 0, "groups": [], "examples": [],
              "result_file": str(path), "rom_sha256": hashlib.sha256(rom).hexdigest()}
    try:
        with path.open(encoding="ascii") as stream:
            require(stream.readline() == f"#VELOCE 1 core=N64 rom_crc32={zlib.crc32(rom):08x} channels=v64debug\n", "wrong ROM/core/channel")
            require(stream.readline() == "VELOCE 1 n64 muldiv64\n", "wrong ROM header")
            require(stream.readline() == f"INFO boundary_patterns=16 byte_operands=256 groups={len(OPS)}\n", "wrong domain")
            for name, _, values in OPS:
                size = len(values)
                words = 2
                require(stream.readline() == f"BEGIN {name} cases={size * size} words={words}\n", "missing/reordered group")
                group = {"id": name, "observed_cases": 0, "graded_cases": 0, "undefined_cases": 0, "mismatches": 0}
                report["groups"].append(group)
                for lhs in values:
                    for index in range(size):
                        line = stream.readline()
                        require(re.fullmatch(r"REG r=[0-9a-f]{" + str(words * 16) + r"}\n", line), "missing/malformed register observation")
                        got = tuple(int(line[6 + i * 16:22 + i * 16], 16) for i in range(words))
                        rhs = values[index]
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
    env.update(HEADLESS="1", FRAMES="6000", VELOCE_TEST_OUT=str(path.resolve()), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], cwd=ROOT,
                          env=env, capture_output=True, text=True, timeout=180)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def controls(emulator, artifacts):
    rom = bytearray(build())
    for signed_fn, unsigned_fn in ((0x1c, 0x1d), (0x1e, 0x1f)):
        stimulus = ((18 << 21) | (22 << 16) | signed_fn).to_bytes(4, "big")
        position = rom.index(stimulus, 0x1000)
        rom[position + 3] = unsigned_fn  # Boundary signed op executes unsigned.
    image = artifacts / "negative.z64"
    image.write_bytes(rom)
    path = artifacts / "negative.result"
    run(emulator, image, path)
    report = evaluate(path, rom)
    require(report["status"] == "fail" and all(g["mismatches"] > 0 if i in (0, 2) else g["mismatches"] == 0
                for i, g in enumerate(report["groups"])), "executed mutation not isolated/rejected")
    (artifacts / "negative.json").write_text(json.dumps(report, indent=2) + "\n")
    content = path.read_text()
    row = next(line for line in content.splitlines() if line.startswith("REG r="))
    for name, broken in (
        ("missing-row", content.replace(row + "\n", "", 1)),
        ("malformed-row", content.replace(row, "REG r=invalid", 1)),
        ("reordered-group", content.replace("BEGIN dmult_boundary", "BEGIN dmultu_boundary", 1)),
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
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/n64_muldiv64")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    references()
    rom = build()
    require((HERE / "muldiv64.z64").read_bytes() == rom, "archive differs from generator")
    controls(args.emulator, args.artifacts)
    path = args.artifacts / "muldiv64.result"
    run(args.emulator, HERE / "muldiv64.z64", path)
    report = evaluate(path, rom)
    (args.artifacts / "muldiv64.json").write_text(json.dumps(report, indent=2) + "\n")
    if report["status"] == "pass":
        # Execute both common byte-swapped archives through frontend discovery,
        # including their distinct raw-file CRC identities.
        for extension, width in (("v64", 2), ("n64", 4)):
            swapped = b"".join(rom[i:i + width][::-1] for i in range(0, len(rom), width))
            image = args.artifacts / f"muldiv64.{extension}"
            image.write_bytes(swapped)
            result = args.artifacts / f"{extension}.result"
            run(args.emulator, image, result)
            alternate = evaluate(result, swapped)
            (args.artifacts / f"{extension}.json").write_text(json.dumps(alternate, indent=2) + "\n")
            require(alternate["status"] == "pass" and alternate["graded_cases"] == report["graded_cases"],
                    f"{extension} execution differs")
    print(f"N64 muldiv64: {report['status'].upper()} {report['graded_cases']} graded; "
          f"{report['observed_cases']}/{TOTAL} observations; {report['undefined_cases']} undefined; mismatches={report['mismatches']}")
    if report.get("detail"): print(report["detail"])
    for example in report["examples"]: print(json.dumps(example))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
