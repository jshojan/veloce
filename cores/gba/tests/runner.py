#!/usr/bin/env python3
"""
Veloce GBA test runner shim built on the shared `veloce_testkit`.

This is the schema-v2 / scorecard entrypoint. It reuses the platform-wide
detection (`detect.detect_gba_register` -> '[GBA] PASSED/FAILED') and the
weighted completeness scoring, so the GBA accuracy number is computed the
exact same way as every other console.

The legacy `test_runner.py` remains for the older flat-summary output and is
still wired into `run_tests.sh`; both read the same `test_config.json`.

Usage:
    python3 runner.py                 # run all, render scorecard
    python3 runner.py cpu timing      # filter by subsystem or suite id
    python3 runner.py --json          # scorecard JSON for tests/run_all.py
    python3 runner.py --generate-refs # print measured screenshot CRCs
    python3 runner.py -v              # per-test verdict lines
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# Make the shared testkit importable: cores/gba/tests -> repo root / tests
_REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(_REPO_ROOT / "tests"))

from veloce_testkit.runner import run_console_main  # noqa: E402
from veloce_testkit.schema import load_config        # noqa: E402

# NOTE ON REPO COLLISIONS: a handful of cross-check suites (cpu_alyosha_core,
# memory_alyosha, bios_alyosha, ppu_timing's hello/shades/stripes) share a
# repo-relative path with their jsmolka counterpart (e.g. arm/arm.gba). The
# merged view below lets the canonical repo (jsmolka) win that path, so those
# cross-check entries resolve to the canonical ROM. They test the same
# instruction classes, so the verdict is equivalent; the authoritative
# per-repo resolution lives in the legacy `test_runner.py`. Use that runner if
# you need each repo's exact ROM scored independently.
#
# Repositories cloned per repo id; the test 'file' paths are repo-relative, so
# we materialize each repo into its own dir and symlink/copy into a single
# roms_dir tree keyed by the repo's on-disk layout. The simplest faithful layout
# is to clone each repo and resolve test files against the repo dir; the testkit
# resolves every 'file' against a single roms_dir, so we lay the repos out side
# by side and prefix each suite's files. To keep the testkit contract (one
# roms_dir), we clone repos directly under script_dir using their configured
# 'dir' and rely on the fact that suite file paths are already repo-relative and
# unique enough; where two repos share a relative path (jsmolka vs alyosha
# arm/arm.gba) the per-suite repo dir disambiguates via a merged view built here.


def _ensure_repos(script_dir: Path, keep: bool, verbose: bool) -> Path:
    cfg = load_config(script_dir / "test_config.json", "gba")
    merged = script_dir / "_roms"
    merged.mkdir(exist_ok=True)

    for repo_id, repo in cfg.repositories.items():
        url = repo.get("url")
        rdir = script_dir / repo.get("dir", repo_id)
        if not rdir.exists() and url:
            if verbose:
                print(f"cloning {repo_id}: {url}")
            try:
                subprocess.run(
                    ["git", "clone", "--depth", "1", url, str(rdir)],
                    check=True, capture_output=not verbose,
                )
            except subprocess.CalledProcessError as e:
                print(f"  warn: failed to clone {repo_id}: {e}", file=sys.stderr)

    # Build a per-repo merged view: a test's 'file' is resolved against the
    # repo dir of its suite. The testkit resolves against a single roms_dir, so
    # we materialize a tree where each suite's files are reachable. We symlink
    # each repo dir's contents under the merged root, with the alyosha tree
    # taking a 'alyosha-gba-tests'-free overlay only where jsmolka lacks a file.
    # Practically, every suite in the config sets its own repo, and the file
    # paths are unique per repo, so we copy-link the union with jsmolka first
    # (canonical), then fill gaps from the other repos.
    order = ["jsmolka", "alyosha", "nba-hw-test", "fuzzarm", "armwrestler"]
    for repo_id in order + [r for r in cfg.repositories if r not in order]:
        rdir = script_dir / cfg.repositories.get(repo_id, {}).get("dir", repo_id)
        if not rdir.exists():
            continue
        for src in rdir.rglob("*.gba"):
            rel = src.relative_to(rdir)
            dst = merged / rel
            if dst.exists():
                continue
            dst.parent.mkdir(parents=True, exist_ok=True)
            try:
                dst.symlink_to(src.resolve())
            except OSError:
                import shutil
                shutil.copy2(src, dst)
    return merged


if __name__ == "__main__":
    sys.exit(run_console_main("gba", Path(__file__).parent, rom_provider=_ensure_repos))
