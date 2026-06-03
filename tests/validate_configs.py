#!/usr/bin/env python3
"""
Validate every cores/<c>/tests/test_config.json against the v2 schema.

Run in CI before tests so a malformed config fails fast with a clear message.
Console agents should run:  python tests/validate_configs.py snes
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from veloce_testkit.schema import validate_config  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
CONSOLES = ["nes", "snes", "gb", "gba"]


def main() -> int:
    targets = sys.argv[1:] or CONSOLES
    rc = 0
    for con in targets:
        cfg = ROOT / "cores" / con / "tests" / "test_config.json"
        if not cfg.exists():
            print(f"[skip] {con}: no test_config.json")
            continue
        errors = validate_config(cfg, con)
        if errors:
            rc = 1
            print(f"[FAIL] {con}: {len(errors)} error(s)")
            for e in errors:
                print(f"    - {e}")
        else:
            print(f"[ok]   {con}: valid")
    return rc


if __name__ == "__main__":
    sys.exit(main())
