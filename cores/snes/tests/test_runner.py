#!/usr/bin/env python3
"""
Veloce SNES test runner (schema-v2 shim).

This is a thin wrapper around the shared ``veloce_testkit`` package so the SNES
core uses the exact same detection, scoring, CLI, and JSON scorecard as every
other console. All test definitions live in ``test_config.json`` (schema v2).

What this file owns (per the foundation conventions, console agents keep only a
~10-line shim plus a ROM provider):

  * SNES-specific ROM provisioning: clone the standard public SNES test-ROM
    repositories declared in test_config.json["repositories"] and stage them so
    that each test's config ``file`` path resolves. Because the upstream repo
    layouts do not always match our tidy config paths, the provider also builds
    a basename index and symlinks any ROM whose basename matches a config path
    into the expected location (best-effort path healing). Unresolved ROMs are
    left missing -> the harness reports SKIP (excluded from the score) rather
    than a false FAIL.

CLI (identical for every console, provided by veloce_testkit.runner):
    ./run_tests.sh                 # run all, human scorecard
    ./run_tests.sh cpu ppu apu     # subset by subsystem key or suite id
    ./run_tests.sh --json          # scorecard JSON (consumed by tests/run_all.py)
    ./run_tests.sh -v              # per-test verdict lines
    ./run_tests.sh --generate-refs # screenshot-crc: print measured CRC32 hashes
    ./run_tests.sh --keep          # keep cloned ROM repos
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

# Make the shared testkit importable: cores/snes/tests -> repo_root/tests.
_REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(_REPO_ROOT / "tests"))

from veloce_testkit.runner import run_console_main  # noqa: E402

CONSOLE = "snes"
ROM_EXTS = (".sfc", ".smc", ".bin")

# ROMs rebuilt by tests/roms-src/build.py land in <repo>/build/roms/snes/<suite>/.
# Config entries with rom_variant "veloce" address them as
# "veloce/<suite>/<rom>" through this symlink; the harness still refuses any
# such ROM whose sha256 is not in tests/roms-src/rom_manifest.json. Without a
# build the link dangles and those tests SKIP ("ROM not found").
VELOCE_LINK = "veloce"


def _load_doc(script_dir: Path) -> dict:
    try:
        with open(script_dir / "test_config.json") as f:
            return json.load(f)
    except Exception:
        return {}


def _config_file_paths(doc: dict) -> list[str]:
    """Every test 'file'/'path' string declared in the config."""
    paths: list[str] = []
    suites = {**doc.get("test_suites", {}), **doc.get("visual_test_suites", {})}
    for suite in suites.values():
        for t in suite.get("tests", []):
            p = t.get("file") or t.get("path")
            if p:
                paths.append(p)
    return paths


def _clone(url: str, dest: Path, verbose: bool) -> bool:
    if dest.exists() and any(dest.iterdir()):
        return True
    dest.parent.mkdir(parents=True, exist_ok=True)
    if verbose:
        print(f"  cloning {url} -> {dest}")
    try:
        subprocess.run(
            ["git", "clone", "--depth", "1", url, str(dest)],
            check=True,
            stdout=(None if verbose else subprocess.DEVNULL),
            stderr=(None if verbose else subprocess.DEVNULL),
        )
        return True
    except Exception as e:  # noqa: BLE001
        print(f"  WARNING: clone failed for {url}: {e}", file=sys.stderr)
        return False


def _link_veloce_builds(script_dir: Path, roms_dir: Path) -> None:
    link = roms_dir / VELOCE_LINK
    target = Path(os.path.relpath(script_dir.parents[2] / "build" / "roms" / "snes", roms_dir))
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
    """Clone the configured repos under tests/roms/ and heal config paths.

    Returns the directory the harness resolves test 'file' paths against.
    """
    roms_dir = script_dir / "roms"
    roms_dir.mkdir(parents=True, exist_ok=True)
    _link_veloce_builds(script_dir, roms_dir)

    doc = _load_doc(script_dir)
    repos = doc.get("repositories", {})
    allow_net = os.environ.get("SNES_TEST_OFFLINE") != "1"
    for rid, spec in repos.items():
        url = spec.get("url")
        sub = spec.get("dir", rid)
        if not url:
            continue
        if allow_net:
            _clone(url, roms_dir / sub, verbose)
        elif verbose:
            print(f"  (offline) skipping clone of {url}")

    # Build a basename index across everything we cloned. Use os.walk with
    # followlinks=True so a pre-existing clone symlinked under roms/ is indexed
    # too (Path.rglob does not descend into symlinked directories).
    index: dict[str, Path] = {}
    for dirpath, dirs, files in os.walk(roms_dir, followlinks=True):
        if Path(dirpath) == roms_dir and VELOCE_LINK in dirs:
            # Never heal an upstream path with a patched Veloce build.
            dirs.remove(VELOCE_LINK)
        for fn in files:
            ext = os.path.splitext(fn)[1].lower()
            if ext in ROM_EXTS:
                index.setdefault(fn.lower(), Path(dirpath) / fn)

    # Heal: for every config path, if the exact path is missing but a ROM with
    # the same basename exists, symlink it into place so the harness finds it.
    healed = 0
    missing = []
    for rel in _config_file_paths(doc):
        target = roms_dir / rel
        if target.exists():
            continue
        if Path(rel).parts[:1] == (VELOCE_LINK,):
            missing.append(rel)  # build.py output only; never healed from upstream
            continue
        src = index.get(Path(rel).name.lower())
        if src is not None:
            target.parent.mkdir(parents=True, exist_ok=True)
            try:
                target.symlink_to(src.resolve())
                healed += 1
            except FileExistsError:
                pass
            except OSError:
                import shutil

                shutil.copy2(src, target)
                healed += 1
        else:
            missing.append(rel)

    if verbose:
        print(f"  ROM index: {len(index)} ROMs; healed {healed} config paths; "
              f"{len(missing)} unresolved (will SKIP)")
        for m in missing:
            print(f"    unresolved: {m}")

    return roms_dir


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    return run_console_main(CONSOLE, script_dir, rom_provider)


if __name__ == "__main__":
    sys.exit(main())
