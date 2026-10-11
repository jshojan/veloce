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

import os
import subprocess
import sys
from pathlib import Path

# Make the shared testkit importable: cores/nes/tests -> repo root / tests
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tests"))

from veloce_testkit.runner import run_console_main  # noqa: E402

NES_TEST_ROMS_REPO = "https://github.com/christopherpow/nes-test-roms.git"

# ROMs rebuilt by tests/roms-src/build.py land in <repo>/build/roms/nes/<suite>/.
# Config entries with rom_variant "veloce" address them as
# "veloce/<suite>/<rom>"; the harness still refuses any such ROM whose sha256
# is not in tests/roms-src/rom_manifest.json. Without a build the link dangles
# and those tests SKIP ("ROM not found"). Same layout as cores/gb/tests and
# cores/gba/tests.
#
# The link lives in a merged view (_roms/: one symlink per top-level entry of
# the nes-test-roms checkout, plus veloce/) rather than inside the checkout
# itself: a git worktree commonly shares one nes-test-roms clone through a
# symlink, and a veloce/ link placed there would point every worktree at the
# same (other) build/ tree.
VELOCE_LINK = "veloce"
MERGED_DIR = "_roms"
REPO_ROOT = Path(__file__).resolve().parents[3]


def _ensure_link(link: Path, target: Path) -> None:
    try:
        if link.is_symlink():
            if Path(os.readlink(link)) == target:
                return
            link.unlink()
        elif link.exists():
            return  # a real file/directory someone put there; leave it alone
        link.symlink_to(target, target_is_directory=True)
    except OSError as e:
        print(f"  warning: cannot link {link} -> {target}: {e}", file=sys.stderr)


def _merged_view(script_dir: Path, upstream: Path) -> Path:
    merged = script_dir / MERGED_DIR
    merged.mkdir(exist_ok=True)
    for entry in sorted(upstream.iterdir()):
        if entry.name in (".git", VELOCE_LINK):
            continue
        _ensure_link(merged / entry.name, Path(os.path.relpath(upstream / entry.name, merged)))
    _ensure_link(merged / VELOCE_LINK,
                 Path(os.path.relpath(REPO_ROOT / "build" / "roms" / "nes", merged)))
    return merged


def nes_rom_provider(script_dir: Path, keep: bool, verbose: bool) -> Path:
    """Ensure nes-test-roms is present; return the dir test 'file' paths resolve against
    (the _roms/ merged view: nes-test-roms entries plus veloce/)."""
    roms_dir = script_dir / "nes-test-roms"
    if not roms_dir.exists():
        if verbose:
            print(f"Cloning {NES_TEST_ROMS_REPO} ...")
        subprocess.run(
            ["git", "clone", "--depth", "1", NES_TEST_ROMS_REPO, str(roms_dir)],
            check=True,
            capture_output=not verbose,
        )
    return _merged_view(script_dir, roms_dir)


if __name__ == "__main__":
    sys.exit(run_console_main("nes", Path(__file__).parent, nes_rom_provider))
