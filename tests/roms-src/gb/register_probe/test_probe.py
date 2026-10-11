#!/usr/bin/env python3
"""End-to-end register-output test, including a deliberately failing ROM."""
import importlib.util
import argparse
import hashlib
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

spec = importlib.util.spec_from_file_location("register_probe_build", HERE / "build.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def run(rom_path: Path, result_path: Path, emulator: Path):
    env = os.environ.copy()
    for name in ("DEBUG", "TRACE", "TRACE_FILE", "INPUT", "SAVE_SCREENSHOT", "VELOCE_TEST_RESETS"):
        env.pop(name, None)
    env.update(HEADLESS="1", FRAMES="120", VELOCE_TEST_OUT=str(result_path), VELOCE_TEST_EXIT="1")
    completed = subprocess.run([str(emulator), str(rom_path)], cwd=ROOT, env=env,
                               capture_output=True, text=True, timeout=30)
    if completed.returncode:
        raise AssertionError(completed.stdout + completed.stderr)
    parsed = parse_result_file(result_path)
    require(parsed.rom_crc32 == f"{zlib.crc32(rom_path.read_bytes()):08x}", "wrong standard ROM CRC32")
    verdict = detect_result_file(result_path, expected_checks=2, require_channel="serial")
    return parsed, verdict, result_path.read_text()


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("emulator", nargs="?", type=Path, default=ROOT / "build/bin/veloce")
    parser.add_argument("--artifacts", type=Path, default=ROOT / "build/artifacts/owned/gb_register_probe")
    args = parser.parse_args()
    emulator = args.emulator.resolve()
    source_rom = HERE / "register_probe.gb"
    original = module.build()
    require(source_rom.read_bytes() == original, "checked-in ROM differs from source build")
    args.artifacts.mkdir(parents=True, exist_ok=True)
    good_rom = args.artifacts / "good.gb"
    good_rom.write_bytes(original)
    parsed, verdict, content = run(good_rom, args.artifacts / "good.result", emulator)
    require(verdict.status == TestStatus.PASS, verdict)
    require(parsed.channels == ["serial", "mooneye", "a000", "hram", "blarggtext"], parsed.channels)
    require("REG wram=5A\n" in content and "REG alu=10\n" in content, "missing observed registers")
    require([(c.id, c.name, c.passed) for c in verdict.checks] == [
        ("1", "wram_roundtrip", True), ("2", "add_result", True)], "wrong passing check set")
    report = {"console": "gb", "suite": "register_probe", "status": "pass",
              "checks": [c.to_dict() for c in verdict.checks], "registers": parsed.regs,
              "rom_sha256": hashlib.sha256(original).hexdigest()}
    (args.artifacts / "register_probe.json").write_text(json.dumps(report, indent=2) + "\n")

    # Corrupt only the ROM's WRAM stimulus. The expected $5A stays in the
    # ROM. This proves the file verdict follows the CPU's observed value.
    broken = bytearray(original)
    offset = broken.index(bytes((0x3E, 0x5A, 0xEA, 0x00, 0xC0))) + 1
    broken[offset] = 0x59
    bad_rom = args.artifacts / "bad.gb"
    bad_rom.write_bytes(broken)
    _, verdict, content = run(bad_rom, args.artifacts / "bad.result", emulator)
    require(verdict.status == TestStatus.FAIL, verdict)
    require("REG wram=59\n" in content, "missing injected WRAM observation")
    require([(c.id, c.passed) for c in verdict.checks] == [("1", False), ("2", True)],
            "wrong injected failure check set")
    print(f"GB register probe: build, PASS, and injected FAIL verified; artifacts: {args.artifacts}")


if __name__ == "__main__":
    main()
