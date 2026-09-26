#!/usr/bin/env python3
"""
tests/roms-src/build.py — SH-3b

Rebuilds Veloce's test ROMs from source, one suite at a time, driven by a
`recipe.json` next to each suite (`tests/roms-src/<console>/<suite>/recipe.json`).
For every suite this:

  1. **clones the upstream repo at its pinned SHA** into
     `tests/roms-src/<console>/<suite>/upstream/` (never committed — see
     .gitignore). `git init` + `git remote add` + `git fetch --depth 1 origin
     <sha>` + `git checkout FETCH_HEAD` works against GitHub for an arbitrary
     reachable commit, not just a ref GitHub advertises.
  2. **runs the golden gate**: builds the *unmodified* upstream source (inside
     the pinned `tools/rom-toolchain` image) and `cmp`s the result — as a
     sha256 — against the sha256 the same file had right after checkout (i.e.
     upstream's own shipped binary, before we touch anything). A suite may
     only be patched once this passes; recipes may set `"golden_gate": false`
     for the documented exceptions (behaviourally-verified-only sources).
  3. **applies `patches/*.patch`** (git apply, from the clone root) — only
     suites that need real ROM changes carry any.
  4. **builds** (patched or not) into `build/roms/<console>/<suite>/...`.
  5. **writes `rom_manifest.json`**: sha256 per built ROM, the upstream
     repo+sha it came from, which patches were applied, and the toolchain
     image's recorded tool versions (`tools/rom-toolchain/versions.json`,
     written by `build-image.sh`). `veloce_testkit.rom_manifest` is what the
     harness reads back to refuse an unreproducible `rom_variant: veloce` ROM.

Usage:
    tests/roms-src/build.py                        # every recipe.json found
    tests/roms-src/build.py nes/cpu_dummy_reads     # one suite
    tests/roms-src/build.py --image IMG ...         # override the toolchain image
    tests/roms-src/build.py --skip-clone ...        # reuse an existing upstream/ checkout
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

ROMS_SRC = Path(__file__).resolve().parent          # tests/roms-src
REPO_ROOT = ROMS_SRC.parents[1]                      # repo root
DEFAULT_IMAGE = "veloce/rom-toolchain:dev"
MANIFEST_PATH = ROMS_SRC / "rom_manifest.json"
VERSIONS_PATH = REPO_ROOT / "tools" / "rom-toolchain" / "versions.json"


class BuildError(RuntimeError):
    pass


@dataclass
class Recipe:
    console: str
    suite: str
    dir: Path
    repo_url: str
    repo_sha: str
    toolchain_stage: str            # informational; the image already has every tool on PATH
    workdir: str                    # cwd (relative to the clone root) the build commands run in
    commands: list[str]
    artifacts: dict[str, str]       # {built path (rel. to clone root): output name under build/roms}
    golden_gate: bool
    rom_variant: str
    license: str
    raw: dict = field(default_factory=dict)

    @property
    def clone_dir(self) -> Path:
        return self.dir / "upstream"

    @staticmethod
    def load(path: Path) -> "Recipe":
        raw = json.loads(path.read_text())
        repo = raw["repo"]
        build = raw.get("golden_gate_build") or raw.get("build") or {}
        return Recipe(
            console=raw["console"],
            suite=raw["suite"],
            dir=path.parent,
            repo_url=repo["url"],
            repo_sha=repo["sha"],
            toolchain_stage=raw.get("toolchain_stage", ""),
            workdir=build.get("workdir", "."),
            commands=build.get("commands", []),
            artifacts=build.get("artifacts", {}),
            golden_gate=raw.get("golden_gate", True),
            rom_variant=raw.get("rom_variant", "upstream"),
            license=raw.get("license", ""),
            raw=raw,
        )


def sh(cmd: list[str], cwd: Optional[Path] = None, check: bool = True) -> subprocess.CompletedProcess:
    proc = subprocess.run(cmd, cwd=str(cwd) if cwd else None, capture_output=True, text=True)
    if check and proc.returncode != 0:
        raise BuildError(
            f"command failed ({proc.returncode}): {' '.join(cmd)}\n"
            f"--- stdout ---\n{proc.stdout}\n--- stderr ---\n{proc.stderr}"
        )
    return proc


def clone_at_sha(url: str, sha: str, dest: Path, *, skip_if_exists: bool = False) -> None:
    """git-clone-at-SHA: works for any commit GitHub still has reachable, not
    just a ref it advertises (`git fetch <sha>` against a public GitHub repo
    is allowed even though the SHA isn't a branch/tag tip)."""
    if dest.exists():
        if skip_if_exists:
            return
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    sh(["git", "init", "-q"], cwd=dest)
    sh(["git", "remote", "add", "origin", url], cwd=dest)
    sh(["git", "fetch", "-q", "--depth", "1", "origin", sha], cwd=dest)
    sh(["git", "checkout", "-q", "FETCH_HEAD"], cwd=dest)
    got = sh(["git", "rev-parse", "HEAD"], cwd=dest).stdout.strip()
    if got != sha:
        raise BuildError(f"clone landed on {got}, expected pinned {sha}")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run_in_toolchain(image: str, clone_dir: Path, workdir: str, commands: list[str]) -> None:
    script = " && ".join(commands)
    container_workdir = f"/work/{workdir}" if workdir not in (".", "") else "/work"
    proc = subprocess.run(
        [
            "docker", "run", "--rm",
            "-v", f"{clone_dir.resolve()}:/work",
            "-w", container_workdir,
            image,
            "sh", "-c", script,
        ],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        raise BuildError(
            f"toolchain build failed in {image} ({workdir}):\n"
            f"$ {script}\n--- stdout ---\n{proc.stdout}\n--- stderr ---\n{proc.stderr}"
        )


def apply_patches(recipe: Recipe) -> list[str]:
    patches_dir = recipe.dir / "patches"
    applied = []
    if not patches_dir.is_dir():
        return applied
    for patch in sorted(patches_dir.glob("*.patch")):
        sh(["git", "apply", "--whitespace=nowarn", str(patch.resolve())], cwd=recipe.clone_dir)
        applied.append(patch.name)
    return applied


def load_tool_versions() -> dict:
    if VERSIONS_PATH.exists():
        try:
            return json.loads(VERSIONS_PATH.read_text()).get("tools", {})
        except (OSError, json.JSONDecodeError):
            return {}
    return {}


def build_one(recipe: Recipe, *, image: str, skip_clone: bool) -> dict:
    print(f"== {recipe.console}/{recipe.suite} ==", file=sys.stderr)
    clone_at_sha(recipe.repo_url, recipe.repo_sha, recipe.clone_dir, skip_if_exists=skip_clone)

    # Golden-gate baseline: hash every artifact as shipped by upstream, BEFORE
    # we run any build command that will overwrite it in place.
    golden_hashes: dict[str, str] = {}
    for built_rel in recipe.artifacts:
        shipped = recipe.clone_dir / built_rel
        if not shipped.exists():
            if recipe.golden_gate:
                raise BuildError(f"golden gate: upstream doesn't ship {built_rel} at pinned SHA")
            continue
        golden_hashes[built_rel] = sha256_file(shipped)

    if not recipe.commands:
        raise BuildError(f"{recipe.suite}: recipe has no build commands")
    run_in_toolchain(image, recipe.clone_dir, recipe.workdir, recipe.commands)

    gate_results = {}
    for built_rel, golden_sha in golden_hashes.items():
        rebuilt = recipe.clone_dir / built_rel
        if not rebuilt.exists():
            raise BuildError(f"golden gate: build didn't produce {built_rel}")
        rebuilt_sha = sha256_file(rebuilt)
        gate_results[built_rel] = {"golden_sha256": golden_sha, "rebuilt_sha256": rebuilt_sha,
                                    "match": rebuilt_sha == golden_sha}
        if recipe.golden_gate and rebuilt_sha != golden_sha:
            raise BuildError(
                f"golden gate FAILED for {built_rel}: rebuilt {rebuilt_sha} != "
                f"shipped {golden_sha} (suite may not be patched until this passes)"
            )
        print(f"   golden gate {built_rel}: {'PASS' if gate_results[built_rel]['match'] else 'behavioural-only'}",
              file=sys.stderr)

    patches_applied = apply_patches(recipe)
    if patches_applied:
        # A patched suite must be rebuilt post-patch; the golden-gate pass
        # above already proved the *unmodified* source reproduces byte-for-byte.
        run_in_toolchain(image, recipe.clone_dir, recipe.workdir, recipe.commands)

    out_dir = REPO_ROOT / "build" / "roms" / recipe.console / recipe.suite
    out_dir.mkdir(parents=True, exist_ok=True)
    tool_versions = load_tool_versions()

    manifest_entries = {}
    for built_rel, out_name in recipe.artifacts.items():
        built = recipe.clone_dir / built_rel
        if not built.exists():
            raise BuildError(f"{recipe.suite}: expected build output missing: {built_rel}")
        dest = out_dir / out_name
        shutil.copy2(built, dest)
        rel_key = f"{recipe.console}/{recipe.suite}/{out_name}"
        manifest_entries[rel_key] = {
            "sha256": sha256_file(dest),
            "console": recipe.console,
            "suite": recipe.suite,
            "rom_variant": "veloce" if patches_applied else recipe.rom_variant,
            "source_repo": recipe.repo_url,
            "source_sha": recipe.repo_sha,
            "patches": patches_applied,
            "golden_gate": gate_results.get(built_rel),
            "tool_versions": tool_versions,
            "license": recipe.license,
            "built_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }
    return manifest_entries


def find_recipes(selector: Optional[str]) -> list[Path]:
    if selector:
        p = ROMS_SRC / selector / "recipe.json"
        if not p.exists():
            raise BuildError(f"no recipe at {p}")
        return [p]
    return sorted(ROMS_SRC.glob("*/*/recipe.json"))


def write_manifest(new_entries: dict) -> None:
    manifest = {"roms": {}, "tool_versions": load_tool_versions()}
    if MANIFEST_PATH.exists():
        try:
            manifest = json.loads(MANIFEST_PATH.read_text())
        except (OSError, json.JSONDecodeError):
            pass
    manifest.setdefault("roms", {}).update(new_entries)
    manifest["tool_versions"] = load_tool_versions()
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("suite", nargs="?", help="console/suite, e.g. nes/cpu_dummy_reads (default: all)")
    ap.add_argument("--image", default=DEFAULT_IMAGE, help=f"toolchain image (default: {DEFAULT_IMAGE})")
    ap.add_argument("--skip-clone", action="store_true", help="reuse an existing upstream/ checkout")
    args = ap.parse_args(argv)

    try:
        recipe_paths = find_recipes(args.suite)
    except BuildError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    if not recipe_paths:
        print("no recipe.json found under tests/roms-src/*/*/", file=sys.stderr)
        return 2

    all_entries = {}
    failures = []
    for rp in recipe_paths:
        recipe = Recipe.load(rp)
        try:
            all_entries.update(build_one(recipe, image=args.image, skip_clone=args.skip_clone))
        except BuildError as e:
            print(f"FAILED {recipe.console}/{recipe.suite}: {e}", file=sys.stderr)
            failures.append(recipe.suite)

    if all_entries:
        write_manifest(all_entries)
        print(f"wrote {MANIFEST_PATH} ({len(all_entries)} ROM(s))", file=sys.stderr)

    if failures:
        print(f"{len(failures)} suite(s) failed: {', '.join(failures)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
