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


def expected_registers(op, value, flags):
    carry = int(bool(flags & 0x10))
    if op == "inc":
        result = (value + 1) % 256
        out = (flags & 0x10) | (0x20 if value % 16 == 15 else 0)
    elif op == "dec":
        result = (value - 1) % 256
        out = (flags & 0x10) | 0x40 | (0x20 if value % 16 == 0 else 0)
    elif op == "daa":
        # Determine both decimal corrections from the ORIGINAL input, unlike
        # the core's sequential adjustment of a widened accumulator.
        subtract = bool(flags & 0x40)
        high = carry or (not subtract and value > 0x99)
        low = bool(flags & 0x20) or (not subtract and value % 16 > 9)
        correction = 96 * int(high) + 6 * int(low)
        result = (value + (-correction if subtract else correction)) % 256
        out = (flags & 0x40) | (0x10 if high else 0)
    elif op == "cpl":
        return 255 - value, (flags & 0x90) | 0x60
    elif op == "scf":
        return value, (flags & 0x80) | 0x10
    elif op == "ccf":
        return value, (flags & 0x80) | (0 if carry else 0x10)
    elif op in ("rlca", "rlc", "rla", "rl", "sla"):
        inserted = value // 128 if op in ("rlca", "rlc") else carry if op in ("rla", "rl") else 0
        result = (2 * value + inserted) % 256
        out = 0x10 if value >= 128 else 0
    elif op in ("rrca", "rrc", "rra", "rr", "sra", "srl"):
        inserted = value % 2 if op in ("rrca", "rrc") else carry if op in ("rra", "rr") else value // 128 if op == "sra" else 0
        result = value // 2 + 128 * inserted
        out = 0x10 if value % 2 else 0
    elif op == "swap":
        result, out = 16 * (value % 16) + value // 16, 0
    else:
        raise ValueError(f"unknown unary operation: {op}")
    if result == 0 and op not in ("rlca", "rrca", "rla", "rra"):
        out |= 0x80
    return result, out


def test_oracle():
    examples = (
        ("inc", 0xFF, 0xF0, 0, 0xB0), ("dec", 0, 0, 0xFF, 0x60),
        ("daa", 0x9A, 0, 0, 0x90), ("daa", 0x94, 0x20, 0x9A, 0),
        ("daa", 0, 0x70, 0x9A, 0x50), ("daa", 0x60, 0x50, 0, 0xD0),
        ("cpl", 0xAA, 0x90, 0x55, 0xF0), ("scf", 0, 0xF0, 0, 0x90),
        ("ccf", 0, 0xF0, 0, 0x80), ("rlca", 0, 0xF0, 0, 0),
        ("rlc", 0, 0xF0, 0, 0x80), ("rl", 0x80, 0x10, 1, 0x10),
        ("rr", 1, 0x10, 0x80, 0x10), ("sra", 0x81, 0, 0xC0, 0x10),
        ("sla", 0x80, 0xF0, 0, 0x90), ("srl", 1, 0xF0, 0, 0x90),
        ("swap", 0xF0, 0xF0, 0x0F, 0),
    )
    for op, value, flags, a, f in examples:
        require(expected_registers(op, value, flags) == (a, f), f"oracle reference failed: {op}")


def evaluate(path, operands, rom):
    report = {"console": "gb", "suite": "unary_exhaustive", "status": "error",
              "operands_per_input": operands, "incoming_flag_states": 16,
              "expected_cases": len(GROUPS) * operands, "observed_cases": 0,
              "mismatches": 0, "groups": [], "examples": [], "result_file": str(path),
              "rom_sha256": hashlib.sha256(rom).hexdigest()}
    try:
        with path.open(encoding="ascii") as stream:
            header = stream.readline().rstrip("\n")
            require(header == f"#VELOCE 1 core=GB rom_crc32={zlib.crc32(rom):08x} channels=serial",
                    "wrong core, ROM identity, or channel")
            require(stream.readline().rstrip("\n") == "VELOCE 1 gb unary_exhaustive", "missing ROM header")
            require(stream.readline().rstrip("\n") == f"INFO operands={operands} groups={len(GROUPS)}", "wrong domain")
            index, count, complete, ended, trailer = -1, 0, False, False, None
            row_cases = min(32, operands)
            for raw in stream:
                require(raw.endswith("\n"), "unterminated result record")
                require(trailer is None, "data after trailer")
                line = raw.rstrip("\n")
                if line.startswith("BEGIN "):
                    require(not complete and (index < 0 or count == operands), "incomplete previous group")
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
                    require(count + row_cases <= operands, "extra observations")
                    values = bytes.fromhex(data)
                    group = report["groups"][-1]
                    for offset in range(0, len(values), 2):
                        value = count
                        got = values[offset], values[offset + 1]
                        expected = expected_registers(op, value, flags)
                        if got != expected:
                            report["mismatches"] += 1
                            group["mismatches"] += 1
                            if len(report["examples"]) < 16:
                                report["examples"].append({"group": group["id"], "input_a": value, "input_f": flags,
                                                           "expected_af": expected, "observed_af": got})
                        count += 1
                        group["observed_cases"] += 1
                        report["observed_cases"] += 1
                elif line.startswith("LOG complete "):
                    require(not complete and index == len(GROUPS) - 1 and count == operands,
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


def run_rom(emulator, rom_path, result_path, frames=2000):
    env = os.environ.copy()
    for key in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(key, None)
    env.update(HEADLESS="1", FRAMES=str(frames), VELOCE_TEST_OUT=str(result_path), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], cwd=ROOT, env=env,
                          capture_output=True, text=True, timeout=60)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def negative_controls(emulator, artifacts):
    mutant = bytearray(build(16))
    stimulus = bytes((0x42, 0x0E, 0, 0xC5, 0xF1, 0x3C, 0xF5, 0xC1))
    offset = mutant.index(stimulus) + 5
    mutant[offset] = 0x3D  # One INC group executes DEC; domain unchanged.
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
                       ("reordered-group", content.replace("BEGIN inc_f00", "BEGIN inc_f10", 1)),
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
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/gb_unary")
    parser.add_argument("--smoke", action="store_true", help="16 accumulator values; full run is the default")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    test_oracle()
    negative_controls(args.emulator, args.artifacts)
    operands = 16 if args.smoke else 256
    rom = build(operands)
    if not args.smoke:
        require((HERE / "unary_exhaustive.gb").read_bytes() == rom, "archived ROM differs from generator")
    rom_path = args.artifacts / "unary_exhaustive.gb"
    rom_path.write_bytes(rom)
    path = args.artifacts / "unary_exhaustive.result"
    run_rom(args.emulator, rom_path, path)
    report = evaluate(path, operands, rom)
    (args.artifacts / "unary_exhaustive.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"GB unary: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']} "
          f"A/F pairs; mismatches={report['mismatches']}")
    if report.get("detail"):
        print(report["detail"])
    for example in report["examples"]:
        print(json.dumps(example))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
