#!/usr/bin/env python3
"""Grade every observed SM83 A/F pair using an independent integer oracle."""
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


def require(ok, message):
    if not ok:
        raise ValueError(message)


def expected_registers(op, lhs, rhs, flags):
    carry = bool(flags & 0x10) if op in ("adc", "sbc") else 0
    if op in ("add", "adc"):
        total = lhs + rhs + carry
        result = total % 256
        half = (lhs % 16) + (rhs % 16) + carry >= 16
        out = (0x20 if half else 0) | (0x10 if total >= 256 else 0)
    elif op in ("sub", "sbc", "cp"):
        total = lhs - rhs - carry
        result = total % 256
        half = lhs % 16 < (rhs % 16) + carry
        out = 0x40 | (0x20 if half else 0) | (0x10 if total < 0 else 0)
    elif op == "and":
        result, out = lhs & rhs, 0x20
    elif op == "or":
        result, out = lhs | rhs, 0
    elif op == "xor":
        result, out = lhs ^ rhs, 0
    else:
        raise ValueError(f"unknown ALU operation: {op}")
    out |= 0x80 if result == 0 else 0
    return lhs if op == "cp" else result, out


def test_oracle():
    examples = (
        ("add", 0x0F, 1, 0, 0x10, 0x20),
        ("add", 0xFF, 1, 0xF0, 0, 0xB0),
        ("adc", 0xFF, 0, 0x10, 0, 0xB0),
        ("adc", 0x7F, 0, 0, 0x7F, 0),
        ("sub", 0, 1, 0, 0xFF, 0x70),
        ("sbc", 0x10, 0x0F, 0x10, 0, 0xE0),
        ("sbc", 0, 0, 0x10, 0xFF, 0x70),
        ("cp", 0x31, 0x31, 0xF0, 0x31, 0xC0),
        ("cp", 0, 1, 0, 0, 0x70),
        ("and", 0x55, 0xAA, 0xF0, 0, 0xA0),
        ("xor", 0xAA, 0xAA, 0xF0, 0, 0x80),
        ("or", 0, 0x80, 0xF0, 0x80, 0),
    )
    for op, lhs, rhs, flags, a, f in examples:
        require(expected_registers(op, lhs, rhs, flags) == (a, f), f"oracle reference failed: {op}")


def evaluate(path, operands, rom):
    report = {"console": "gb", "suite": "alu_exhaustive", "status": "error",
              "operands_per_input": operands, "incoming_flag_states": 16,
              "expected_cases": len(GROUPS) * operands * operands, "observed_cases": 0,
              "mismatches": 0, "groups": [], "examples": [], "result_file": str(path),
              "rom_sha256": hashlib.sha256(rom).hexdigest()}
    try:
        with path.open(encoding="ascii") as stream:
            header = stream.readline().rstrip("\n")
            require(header == f"#VELOCE 1 core=GB rom_crc32={zlib.crc32(rom):08x} channels=serial,mooneye,a000,hram,blarggtext",
                    "wrong core, ROM identity, or channel")
            require(stream.readline().rstrip("\n") == "VELOCE 1 gb alu_exhaustive", "missing ROM header")
            require(stream.readline().rstrip("\n") == f"INFO operands={operands} groups={len(GROUPS)}", "wrong domain")
            index, count, complete, ended, trailer = -1, 0, False, False, None
            row_cases = min(32, operands)
            for raw in stream:
                require(raw.endswith("\n"), "unterminated result record")
                require(trailer is None, "data after trailer")
                line = raw.rstrip("\n")
                if line.startswith("BEGIN "):
                    require(not complete and (index < 0 or count == operands * operands), "incomplete previous group")
                    index += 1
                    require(index < len(GROUPS), "extra group")
                    op, flags = GROUPS[index]
                    require(line == "BEGIN " + group_name(op, flags), "reordered or incorrect group")
                    count = 0
                    report["groups"].append({"id": group_name(op, flags), "observed_cases": 0, "mismatches": 0})
                elif line.startswith("REG af="):
                    require(index >= 0 and not complete, "observations outside group")
                    data = line[7:]
                    require(re.fullmatch(r"[0-9a-f]+", data) and len(data) == row_cases * 4,
                            "malformed or truncated register row")
                    require(count + row_cases <= operands * operands, "extra observations")
                    values = bytes.fromhex(data)
                    group = report["groups"][-1]
                    for offset in range(0, len(values), 2):
                        lhs, rhs = divmod(count, operands)
                        got = values[offset], values[offset + 1]
                        expected = expected_registers(op, lhs, rhs, flags)
                        if got != expected:
                            report["mismatches"] += 1
                            group["mismatches"] += 1
                            if len(report["examples"]) < 16:
                                report["examples"].append({"group": group["id"], "lhs": lhs, "rhs": rhs,
                                                           "expected_af": expected, "observed_af": got})
                        count += 1
                        group["observed_cases"] += 1
                        report["observed_cases"] += 1
                elif line.startswith("LOG complete "):
                    require(not complete and index == len(GROUPS) - 1 and count == operands * operands,
                            "premature or duplicate completion")
                    require(line == f"LOG complete cases={report['expected_cases']}", "wrong completion count")
                    complete = True
                elif line == "END observations_complete":
                    require(complete and not ended, "premature or duplicate END")
                    ended = True
                elif line.startswith("#VELOCE end "):
                    require(ended, "trailer before END")
                    trailer = dict(token.split("=", 1) for token in line.split()[2:])
                    require(trailer.get("reason") == "terminator" and trailer.get("status") == "-1",
                            "missing ungraded completion hint")
                else:
                    raise ValueError(f"unexpected line: {line[:80]}")
            require(trailer is not None and report["observed_cases"] == report["expected_cases"], "incomplete corpus")
            report["status"] = "fail" if report["mismatches"] else "pass"
            report["frames_used"] = int(trailer["frames"])
            report["cycles_used"] = int(trailer["cycles"])
    except (OSError, UnicodeError, ValueError, KeyError) as error:
        report["detail"] = str(error)
    return report


def run_rom(emulator, rom_path, result_path, frames=120000):
    env = os.environ.copy()
    for key in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(key, None)
    env.update(HEADLESS="1", FRAMES=str(frames), VELOCE_TEST_OUT=str(result_path), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], cwd=ROOT, env=env,
                          capture_output=True, text=True, timeout=900)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def negative_controls(emulator, artifacts):
    mutant = bytearray(build(16))
    stimulus = bytes((0x42, 0x0E, 0, 0xC5, 0xF1, 0x83, 0xF5, 0xC1))
    offset = mutant.index(stimulus) + 5
    mutant[offset] = 0x93  # One ADD group executes SUB; expected domain unchanged.
    rom_path, result_path = artifacts / "negative.gb", artifacts / "negative.result"
    rom_path.write_bytes(mutant)
    run_rom(emulator, rom_path, result_path, frames=600)
    report = evaluate(result_path, 16, mutant)
    require(report["status"] == "fail" and report["groups"][0]["mismatches"] > 0 and
            all(group["mismatches"] == 0 for group in report["groups"][1:]),
            f"executed opcode mutation not isolated/rejected: {report}")
    (artifacts / "negative.json").write_text(json.dumps(report, indent=2) + "\n")
    content = result_path.read_text()
    row = next(line for line in content.splitlines() if line.startswith("REG af="))
    for name, text in (("missing-row", content.replace(row + "\n", "", 1)),
                       ("malformed-row", content.replace(row, "REG af=xyz", 1)),
                       ("reordered-group", content.replace("BEGIN add_f00", "BEGIN add_f10", 1)),
                       ("false-pass", content.replace("END observations_complete", "END 0/0", 1)),
                       ("missing-trailer", content[:content.index("#VELOCE end ")]),
                       ("unterminated-trailer", content.rstrip("\n")),
                       ("data-after-trailer", content + "REG af=0000\n")):
        path = artifacts / f"{name}.result"
        path.write_text(text)
        broken = evaluate(path, 16, mutant)
        require(broken["status"] == "error", f"invalid measurements accepted: {name}")
        (artifacts / f"{name}.json").write_text(json.dumps(broken, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/gb_alu")
    parser.add_argument("--smoke", action="store_true", help="16 operand values; full run is the default")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    test_oracle()
    negative_controls(args.emulator, args.artifacts)
    operands = 16 if args.smoke else 256
    rom = build(operands)
    if not args.smoke:
        require((HERE / "alu_exhaustive.gb").read_bytes() == rom, "archived ROM differs from generator")
    rom_path = args.artifacts / "alu_exhaustive.gb"
    rom_path.write_bytes(rom)
    path = args.artifacts / "alu_exhaustive.result"
    run_rom(args.emulator, rom_path, path)
    report = evaluate(path, operands, rom)
    (args.artifacts / "alu_exhaustive.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"GB ALU: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']} "
          f"A/F pairs; mismatches={report['mismatches']}")
    if report.get("detail"):
        print(report["detail"])
    for example in report["examples"]:
        print(json.dumps(example))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
