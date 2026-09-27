#!/usr/bin/env python3
"""
Self-test for build.py's pure Recipe/local_source logic (NES-2). No docker,
no network, no ROMs required; safe to run in CI as a fast sanity gate.

  python tests/roms-src/selftest.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from build import Recipe, BuildError, build_one  # noqa: E402

failures = 0


def check(name: str, cond: bool):
    global failures
    if not cond:
        failures += 1
        print(f"  FAIL: {name}")
    else:
        print(f"  ok:   {name}")


def write_recipe(tmp: Path, **fields) -> Path:
    p = tmp / "recipe.json"
    p.write_text(json.dumps(fields))
    return p


def expect_build_error(fn, needle: str) -> bool:
    try:
        fn()
    except BuildError as e:
        return needle in str(e)
    return False


import tempfile

with tempfile.TemporaryDirectory() as td:
    tmp = Path(td)

    # A normal (upstream) recipe still requires 'repo'.
    p = write_recipe(tmp, console="nes", suite="s", golden_gate_build={"commands": ["true"], "artifacts": {}})
    check("upstream recipe without 'repo' is rejected",
          expect_build_error(lambda: Recipe.load(p), "needs 'repo'"))

    # local_source needs no 'repo'.
    p = write_recipe(tmp, console="nes", suite="s", local_source=True,
                      golden_gate_build={"commands": ["true"], "artifacts": {}})
    r = Recipe.load(p)
    check("local_source recipe loads without 'repo'", r.local_source is True)
    check("local_source defaults golden_gate to False", r.golden_gate is False)
    check("local_source clone_dir is the recipe's own dir (no upstream/)", r.clone_dir == r.dir)

    # local_source explicitly setting golden_gate: true is rejected at build time
    # (there is no upstream binary to diff against), before anything touches docker.
    p = write_recipe(tmp, console="nes", suite="s", local_source=True, golden_gate=True,
                      golden_gate_build={"commands": ["true"], "artifacts": {}})
    r = Recipe.load(p)
    check("local_source + golden_gate:true raises before any docker call",
          expect_build_error(lambda: build_one(r, image="unused:image", skip_clone=True),
                              "local_source recipes can't set golden_gate: true"))

    # A normal upstream recipe still defaults golden_gate to True.
    p = write_recipe(tmp, console="nes", suite="s", repo={"url": "https://example/x.git", "sha": "a" * 40},
                      golden_gate_build={"commands": ["true"], "artifacts": {}})
    r = Recipe.load(p)
    check("upstream recipe defaults golden_gate to True", r.golden_gate is True)
    check("upstream recipe clone_dir is dir/upstream", r.clone_dir == r.dir / "upstream")

print()
if failures:
    print(f"{failures} FAILURE(S)")
    sys.exit(1)
print("all ok")
