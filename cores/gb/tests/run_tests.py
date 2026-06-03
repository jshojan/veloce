#!/usr/bin/env python3
"""
Game Boy / Game Boy Color test runner (thin shim over veloce_testkit).

Delegates ALL detection, scoring, and CLI handling to the shared
`tests/veloce_testkit` package so the GB scorecard matches the other consoles
exactly. The only GB-specific logic is the `rom_provider`.

ROM source: the c-sp `gameboy-test-roms` v7.0 release bundle. This is the
authoritative *prebuilt* source -- the upstream Mooneye / Mealybug / SameSuite /
acid2 repos ship only RGBDS assembly source, not runnable `.gb` ROMs. The bundle
contains blargg/, mooneye-test-suite/, mooneye-test-suite-wilbertpol/,
mealybug-tearoom-tests/, same-suite/, dmg-acid2/, cgb-acid2/, and the per-test
`file` paths in test_config.json are relative to the unpacked bundle root.

CLI (identical for every console -- see veloce_testkit.runner):
    run_tests.sh                 # run all, human scorecard
    run_tests.sh cpu ppu apu     # subset by subsystem key or suite id
    run_tests.sh --json          # scorecard JSON (consumed by tests/run_all.py)
    run_tests.sh --generate-refs # screenshot-crc: print measured hashes
    run_tests.sh -v              # per-test verdict lines

Detection (declared per test in test_config.json; see its known_issues):
  serial          Blargg ROMs echo ASCII "Passed"/"Failed" over the link port.
  screenshot-crc  Mealybug / acid2 / Blargg-sound / on-screen Blargg results.
  NOTE: Mooneye/SameSuite/Wilbertpol need a "MOONEYE: PASS/FAIL" line from the
        binary (Fibonacci reg fingerprint + LD B,B); until src emits it they
        resolve to RUNS (no credit, excluded from the denominator).
"""

from __future__ import annotations

import io
import sys
import urllib.request
import zipfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
# cores/gb/tests -> repo root (parents[2]); shared testkit lives in <root>/tests
sys.path.insert(0, str(SCRIPT_DIR.parents[2] / "tests"))

from veloce_testkit.runner import run_console_main  # noqa: E402

BUNDLE_URL = (
    "https://github.com/c-sp/gameboy-test-roms/releases/download/"
    "v7.0/game-boy-test-roms-v7.0.zip"
)
# A directory that always exists in the bundle -- used as the "already unpacked"
# sentinel so we do not re-download on every run.
SENTINEL = "blargg"


def rom_provider(script_dir: Path, keep: bool, verbose: bool) -> Path:
    """Ensure the c-sp gameboy-test-roms bundle is unpacked; return its root.

    The bundle is unpacked into <script_dir>/roms/. Per-test `file` paths are
    relative to that root (e.g. blargg/cpu_instrs/individual/01-special.gb).
    """
    roms_dir = script_dir / "roms"
    roms_dir.mkdir(exist_ok=True)

    if (roms_dir / SENTINEL).is_dir():
        return roms_dir

    if verbose:
        print(f"  downloading {BUNDLE_URL}")
    try:
        with urllib.request.urlopen(BUNDLE_URL, timeout=300) as resp:
            data = resp.read()
        with zipfile.ZipFile(io.BytesIO(data)) as zf:
            zf.extractall(roms_dir)
    except Exception as e:  # noqa: BLE001
        print(
            f"  ERROR: could not fetch/unpack the test-rom bundle: {e}\n"
            f"  Manually download {BUNDLE_URL} and unzip into {roms_dir}",
            file=sys.stderr,
        )
    return roms_dir


if __name__ == "__main__":
    sys.exit(run_console_main("gb", SCRIPT_DIR, rom_provider))
