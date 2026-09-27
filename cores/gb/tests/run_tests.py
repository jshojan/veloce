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
  serial          alias of "file": the verdict comes from the VELOCE-RESULT/1
                  file the core writes (Blargg serial text / $A000 block,
                  Mooneye LD B,B fingerprint; cores/gb/src/test_channel.hpp).
  file            the same, with the adapter named in `channel` (a000 / hram /
                  mooneye); rom_variant veloce ROMs live under roms/veloce/.
  screenshot-crc  Mealybug / acid2 (visual_test_suites, never scored).
"""

from __future__ import annotations

import io
import os
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


# ROMs rebuilt by tests/roms-src/build.py land in <repo>/build/roms/gb/<suite>/.
# Config entries with rom_variant "veloce" address them as
# "veloce/<suite>/<rom>", through this symlink; the harness still refuses any
# such ROM whose sha256 is not in tests/roms-src/rom_manifest.json. Without a
# build the link dangles and those tests SKIP ("ROM not found").
VELOCE_LINK = "veloce"


def _link_veloce_builds(roms_dir: Path) -> None:
    link = roms_dir / VELOCE_LINK
    target = Path(os.path.relpath(SCRIPT_DIR.parents[2] / "build" / "roms" / "gb", roms_dir))
    try:
        if link.is_symlink():
            if Path(os.readlink(link)) == target:
                return
            link.unlink()
        elif link.exists():
            return  # a real directory someone put there; leave it alone
        link.symlink_to(target, target_is_directory=True)
    except OSError as e:
        print(f"  warning: cannot link {link} -> {target}: {e}", file=sys.stderr)


def rom_provider(script_dir: Path, keep: bool, verbose: bool) -> Path:
    """Ensure the c-sp gameboy-test-roms bundle is unpacked; return its root.

    The bundle is unpacked into <script_dir>/roms/. Per-test `file` paths are
    relative to that root (e.g. blargg/cpu_instrs/individual/01-special.gb).
    """
    roms_dir = script_dir / "roms"
    roms_dir.mkdir(exist_ok=True)
    _link_veloce_builds(roms_dir)

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
