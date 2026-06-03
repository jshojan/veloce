"""
Reference per-console runner built entirely on the testkit.

The four console agents have two options:
  (A) Replace their cores/<c>/tests/test_runner.py with a thin wrapper that calls
      run_console_main() below (recommended once their test_config.json is migrated
      to schema v2).
  (B) Keep their bespoke runner but import veloce_testkit.{detect,scoring} so the
      verdicts and scores stay consistent platform-wide.

This module deliberately lives in the SHARED testkit so all consoles share the
exact CLI, JSON shape, and scorecard. The per-core runner.py shim is owned by
each console agent; it should be ~10 lines:

    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tests"))
    from veloce_testkit.runner import run_console_main
    sys.exit(run_console_main("snes", Path(__file__).parent))

CLI (identical for every console):
    run_tests.sh                 # run all, human scorecard
    run_tests.sh cpu ppu         # subset by subsystem or suite id
    run_tests.sh --json          # emit scorecard JSON (consumed by tests/run_all.py)
    run_tests.sh --generate-refs # screenshot-crc: print measured hashes
    run_tests.sh -v              # per-test verdict lines
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Callable, Optional

from .schema import load_config
from .harness import Harness, RunSettings
from .detect import TestStatus
from .scoring import score_console, render_scorecard, scorecard_to_dict


# Console agents supply a callable that ensures ROMs are present and returns the
# directory the test 'file' paths resolve against. The testkit does not hardcode
# download logic because each console pulls from different upstreams.
RomProvider = Callable[[Path, bool, bool], Path]
#                       script_dir, keep, verbose -> roms_dir


def run_console_main(
    console: str,
    script_dir: Path,
    rom_provider: Optional[RomProvider] = None,
    argv: Optional[list[str]] = None,
) -> int:
    ap = argparse.ArgumentParser(description=f"Veloce {console.upper()} test suite")
    ap.add_argument("filters", nargs="*", help="subsystem keys or suite ids to run")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--keep", action="store_true", help="keep downloaded ROMs")
    ap.add_argument("--generate-refs", action="store_true")
    ap.add_argument("--config", default=str(script_dir / "test_config.json"))
    args = ap.parse_args(argv)

    project_root = script_dir.resolve().parents[2]  # cores/<c>/tests -> repo root
    cfg = load_config(args.config, console)

    if rom_provider is not None:
        roms_dir = rom_provider(script_dir, args.keep, args.verbose)
    else:
        # Default: assume ROMs already laid out under script_dir per repo layout.
        roms_dir = script_dir

    settings = RunSettings(
        project_root=project_root,
        roms_dir=roms_dir,
        screenshots_dir=script_dir / "screenshots",
        default_frames=cfg.frame_limit,
        default_timeout=cfg.timeout_seconds,
        generate_refs=args.generate_refs,
        console=console,
    )
    harness = Harness(cfg, settings)

    suite_filter = set(args.filters) if args.filters else None
    results = harness.run_all(suite_filter=suite_filter)

    points = [r.point for r in results if r.point is not None]
    card = score_console(console, points)

    if not args.json and args.verbose:
        for r in results:
            sym = {
                TestStatus.PASS: "PASS", TestStatus.FAIL: "FAIL",
                TestStatus.KNOWN_FAIL: "KNOWN", TestStatus.RUNS: "RUNS",
                TestStatus.SKIP: "SKIP", TestStatus.TIMEOUT: "TIMEOUT",
                TestStatus.ERROR: "ERROR",
            }.get(r.status, "?")
            print(f"  [{sym:7}] {r.test.id}  {r.detail}")

    if args.json:
        doc = scorecard_to_dict(card)
        doc["results"] = [
            {"id": r.test.id, "subsystem": r.test.subsystem,
             "status": r.status.value, "detail": r.detail,
             "actual_hash": r.actual_hash}
            for r in results
        ]
        print(json.dumps(doc, indent=2))
    else:
        print(render_scorecard(card))

    if args.generate_refs:
        refs = {r.test.file: r.actual_hash for r in results if r.actual_hash}
        if refs:
            print("\n# screenshot-crc reference_hash values (paste into test_config.json):")
            for f, h in sorted(refs.items()):
                print(f'  "{f}": "{h}"')

    # CI semantics: exit nonzero if any *scored* test failed (known_fail excluded).
    hard_fail = any(r.status in (TestStatus.FAIL, TestStatus.TIMEOUT, TestStatus.ERROR)
                    for r in results)
    return 1 if hard_fail else 0
