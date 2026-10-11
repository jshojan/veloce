#!/usr/bin/env python3
"""Grade original SNES per-phase byte observations with closed-form integer math."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import zlib
from build import MULTIPLICANDS, DIVIDENDS, GROUPS, SEED_QUOTIENT, build, group_name
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def registers(op, lhs, rhs, phase):
    if op == "mul":
        k = min(phase, 8)
        quotient = ((rhs * 256) + lhs) // (2 ** k)
        product = rhs * (lhs % (2 ** k))
        return quotient, product
    k = min(phase, 16)
    scale = 2 ** (16 - k)
    if rhs:
        prefix, remainder = divmod(lhs, rhs * scale)
    else:
        prefix, remainder = 2 ** k - 1, lhs
    quotient = ((SEED_QUOTIENT * (2 ** k)) % 65536) + prefix
    return quotient, remainder


def evaluate(path, rom):
    report = {"console": "snes", "suite": "math_timing", "status": "error",
              "expected_cases": 504832, "observed_cases": 0, "mismatches": 0,
              "groups": [], "examples": [], "result_file": str(path),
              "rom_sha256": hashlib.sha256(rom).hexdigest()}
    try:
        with path.open(encoding="ascii") as stream:
            require(stream.readline().rstrip("\n") ==
                    f"#VELOCE 1 core=SNES rom_crc32={zlib.crc32(rom):08x} channels=port,blargg6000,stp,sram,spcport", "wrong ROM/channel")
            require(stream.readline() == "VELOCE 1 snes math_timing\n", "missing ROM header")
            require(stream.readline() == "INFO phases=2..18 seed=00a5\n", "wrong phase domain")
            for op, register, phase in GROUPS:
                name = group_name(op, register, phase)
                require(stream.readline() == f"BEGIN {name}\n", "wrong or reordered group")
                operands = MULTIPLICANDS if op == "mul" else DIVIDENDS
                group = {"id": name, "expected_cases": len(operands) * 256, "observed_cases": 0, "mismatches": 0}
                report["groups"].append(group)
                for lhs in operands:
                    for start in range(0, 256, 32):
                        row = stream.readline()
                        require(re.fullmatch(r"REG bytes=[0-9a-f]{64}\n", row), "malformed/missing register row")
                        data = bytes.fromhex(row[10:-1])
                        for offset, got in enumerate(data):
                            rhs = start + offset
                            q, p = registers(op, lhs, rhs, phase)
                            value = q if register < 0x16 else p
                            expected = (value // (256 if register & 1 else 1)) % 256
                            if got != expected:
                                report["mismatches"] += 1
                                group["mismatches"] += 1
                                if len(report["examples"]) < 16:
                                    report["examples"].append({"group": name, "lhs": lhs, "rhs": rhs,
                                                               "expected": expected, "observed": got})
                            report["observed_cases"] += 1
                            group["observed_cases"] += 1
            require(stream.readline() == "LOG complete cases=504832\n", "missing completion")
            line = stream.readline()
            require(line.startswith("#VELOCE end ") and line.endswith("\n"), "missing/truncated trailer")
            trailer = dict(token.split("=", 1) for token in line.split()[2:])
            require(trailer.get("reason") == "terminator" and trailer.get("status") == "-1", "ROM declared a verdict")
            require(not stream.read(), "data after trailer")
            report["status"] = "fail" if report["mismatches"] else "pass"
            report["frames_used"] = int(trailer["frames"])
            report["cycles_used"] = int(trailer["cycles"])
    except (OSError, UnicodeError, ValueError, KeyError) as error:
        report["detail"] = str(error)
    return report


def run_rom(emulator, rom_path, path):
    env = os.environ.copy()
    for key in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(key, None)
    env.update(HEADLESS="1", FRAMES="12000", VELOCE_TEST_OUT=str(path), VELOCE_TEST_EXIT="1")
    proc = subprocess.run([str(emulator.resolve()), str(rom_path.resolve())], cwd=ROOT, env=env,
                          capture_output=True, text=True, timeout=180)
    require(proc.returncode == 0, proc.stdout + proc.stderr)


def negative_controls(emulator, artifacts, rom, content):
    # Change the first register sample from RDDIVL to RDMPYL. Metadata and
    # phase remain unchanged, so the independent oracle must reject it.
    mutant = bytearray(rom)
    marker = bytes((0x8D, 3, 0x42, 0xA5, 0x14, 0x8D, 6, 0))
    offset = mutant.index(marker) + 4
    mutant[offset] = 0x16
    path = artifacts / "negative.sfc"
    path.write_bytes(mutant)
    output = artifacts / "negative.result"
    run_rom(emulator, path, output)
    report = evaluate(output, mutant)
    require(report["status"] == "fail" and report["groups"][0]["mismatches"] and
            all(group["mismatches"] == 0 for group in report["groups"][1:]), "mutated sample not rejected/isolated")
    (artifacts / "negative.json").write_text(json.dumps(report, indent=2) + "\n")
    row = next(line for line in content.splitlines() if line.startswith("REG bytes="))
    for name, text in (("missing-row", content.replace(row + "\n", "", 1)),
                       ("bad-row", content.replace(row, "REG bytes=xyz", 1)),
                       ("reordered-group", content.replace("BEGIN mul_r14_p02", "BEGIN mul_r14_p03", 1)),
                       ("missing-trailer", content[:content.index("#VELOCE end ")]),
                       ("trailing-data", content + "REG bytes=00\n")):
        path = artifacts / f"{name}.result"
        path.write_text(text)
        result = evaluate(path, rom)
        require(result["status"] == "error", f"damaged file accepted: {name}")
        (artifacts / f"{name}.json").write_text(json.dumps(result, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/snes_math_timing")
    args = parser.parse_args()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    rom = build()
    require((HERE / "math_timing.sfc").read_bytes() == rom, "archived ROM differs from generator")
    # Reference points check both intermediate and settled outputs, including
    # old-quotient shifting and division by zero.
    require(registers("mul", 255, 255, 0) == (65535, 0), "bad multiply initial reference")
    require(registers("mul", 255, 255, 7) == (511, 32385), "bad multiply intermediate reference")
    require(registers("mul", 255, 255, 8) == (255, 65025), "bad multiply final reference")
    require(registers("div", 65535, 255, 16) == (257, 0), "bad division final reference")
    require(registers("div", 32768, 0, 16) == (65535, 32768), "bad zero divisor reference")
    output = args.artifacts / "math_timing.result"
    run_rom(args.emulator, HERE / "math_timing.sfc", output)
    report = evaluate(output, rom)
    (args.artifacts / "math_timing.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"SNES math timing: {report['status'].upper()} {report['observed_cases']}/{report['expected_cases']} bytes; "
          f"mismatches={report['mismatches']}")
    if report.get("detail"):
        print(report["detail"])
    for example in report["examples"]:
        print(json.dumps(example))
    if report["status"] != "pass":
        return 1
    negative_controls(args.emulator, args.artifacts, rom, output.read_text())
    return 0


if __name__ == "__main__":
    sys.exit(main())
