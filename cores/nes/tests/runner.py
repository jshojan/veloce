#!/usr/bin/env python3
"""
NES test runner shim.

Thin wrapper over the shared veloce_testkit so detection + scoring are identical
across all four consoles. The NES-specific part is only the ROM provider, which
ensures the christopherpow/nes-test-roms repo is present (and, if available,
optional pinobatch/holy-mapperel mapper ROMs) before the harness runs.

CLI (identical for every console, see veloce_testkit.runner):
    runner.py                 # run all, human scorecard
    runner.py cpu ppu         # subset by subsystem key or suite id
    runner.py --json          # scorecard JSON (consumed by tests/run_all.py)
    runner.py --generate-refs # screenshot-crc: print measured reference hashes
    runner.py -v              # per-test verdict lines
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# Make the shared testkit importable: cores/nes/tests -> repo root / tests
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tests"))

from veloce_testkit.runner import run_console_main  # noqa: E402

NES_TEST_ROMS_REPO = "https://github.com/christopherpow/nes-test-roms.git"


def nes_rom_provider(script_dir: Path, keep: bool, verbose: bool) -> Path:
    """Ensure nes-test-roms is present; return the dir test 'file' paths resolve against."""
    roms_dir = script_dir / "nes-test-roms"
    if not roms_dir.exists():
        if verbose:
            print(f"Cloning {NES_TEST_ROMS_REPO} ...")
        subprocess.run(
            ["git", "clone", "--depth", "1", NES_TEST_ROMS_REPO, str(roms_dir)],
            check=True,
            capture_output=not verbose,
        )
    return roms_dir


if __name__ == "__main__":
    sys.exit(run_console_main("nes", Path(__file__).parent, nes_rom_provider))
