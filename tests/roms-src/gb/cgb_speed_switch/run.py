#!/usr/bin/env python3
"""Grade every KEY1 observation from the original CGB speed-switch ROM."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import zlib

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / "tests"))
from veloce_testkit.detect import TestStatus, detect_result_file, parse_result_file  # noqa: E402

spec = importlib.util.spec_from_file_location("cgb_speed_switch_build", HERE / "build.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(emulator, rom_path, result_path):
    env = os.environ.copy()
    for name in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(name, None)
    env.update(HEADLESS="1", FRAMES="120", VELOCE_TEST_OUT=str(result_path), VELOCE_TEST_EXIT="1")
    completed = subprocess.run([str(emulator), str(rom_path)], cwd=ROOT, env=env,
                               capture_output=True, text=True, timeout=30)
    require(completed.returncode == 0, completed.stdout + completed.stderr)
    parsed = parse_result_file(result_path)
    require(parsed.rom_crc32 == f"{zlib.crc32(rom_path.read_bytes()):08x}", "wrong ROM CRC32")
    verdict = detect_result_file(result_path, expected_checks=1, require_channel="serial")
    lines = result_path.read_text().splitlines()
    return parsed, verdict, lines


def rows(lines, name):
    prefix = f"REG {name}="
    values = [line[len(prefix):] for line in lines if line.startswith(prefix)]
    require(len(values) == 16, f"{name}: expected 16 observed rows, got {len(values)}")
    require(all(len(value) == 32 for value in values), f"{name}: truncated row")
    return [int(value[i:i + 2], 16) for value in values for i in range(0, 32, 2)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emulator", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path,
                        default=ROOT / "build/artifacts/owned/gb_cgb_speed_switch")
    args = parser.parse_args()
    emulator = args.emulator.resolve()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    original = builder.build()
    require((HERE / "cgb_speed_switch.gb").read_bytes() == original,
            "checked-in ROM differs from source build")
    good_rom = args.artifacts / "good.gb"
    good_rom.write_bytes(original)
    parsed, verdict, lines = run(emulator, good_rom, args.artifacts / "good.result")
    require(parsed.channels == ["serial", "mooneye", "a000", "hram", "blarggtext"],
            "wrong test channel")
    require(verdict.status == TestStatus.PASS, verdict)
    require([(check.id, check.name, check.passed) for check in verdict.checks] ==
            [("1", "key1_stop_roundtrip", True)], "wrong passing check")
    expected_registers = {"normal": "7E", "armed": "7F", "double": "FE",
                          "armed_double": "FF", "restored": "7E"}
    for name, expected in expected_registers.items():
        require(lines.count(f"REG {name}={expected}") == 1, f"wrong {name} observation")
    for name, high in (("normal_domain", 0), ("double_domain", 0x80)):
        actual = rows(lines, name)
        expected = [high | 0x7E | (value & 1) for value in range(256)]
        require(actual == expected, f"{name}: KEY1 readback differs from 256-byte oracle")

    # The executed bad cartridge disarms the first STOP. Its output must fail
    # from an observed KEY1 value, not merely from a host-side image diff.
    bad_rom = args.artifacts / "disarmed.gb"
    bad_rom.write_bytes(builder.build(arm_value=0))
    _, bad_verdict, bad_lines = run(emulator, bad_rom, args.artifacts / "disarmed.result")
    require(bad_verdict.status == TestStatus.FAIL, bad_verdict)
    require([(check.id, check.passed) for check in bad_verdict.checks] == [("1", False)],
            "wrong injected failure check")
    require("REG armed=7E" in bad_lines and "REG double=7E" in bad_lines,
            "mutant did not expose disarmed KEY1 and unchanged speed")

    report = {"console": "gbc", "suite": "cgb_speed_switch", "status": "pass",
              "register_readbacks": 517, "domain_values_per_speed": 256,
              "rom_sha256": hashlib.sha256(original).hexdigest(),
              "checks": [check.to_dict() for check in verdict.checks],
              "mutant_status": bad_verdict.status.value}
    (args.artifacts / "cgb_speed_switch.json").write_text(json.dumps(report, indent=2) + "\n")
    print("CGB KEY1: 517 observed readbacks and executed disarmed-STOP failure verified")


if __name__ == "__main__":
    main()
