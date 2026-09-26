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
                                                   # (reset to the pinned SHA, cleaned)
    tests/roms-src/build.py --check ...             # rebuild and verify every ROM's sha256
                                                   # against rom_manifest.json (no write)

The toolchain image defaults to $VELOCE_ROM_TOOLCHAIN_IMAGE, else
veloce/rom-toolchain:dev (what tools/rom-toolchain/build-image.sh tags by
default). Containers run as the invoking user so the clone never collects
root-owned build outputs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

ROMS_SRC = Path(__file__).resolve().parent          # tests/roms-src
REPO_ROOT = ROMS_SRC.parents[1]                      # repo root
DEFAULT_IMAGE = os.environ.get("VELOCE_ROM_TOOLCHAIN_IMAGE") or "veloce/rom-toolchain:dev"
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


def _rmtree(path: Path) -> None:
    try:
        shutil.rmtree(path)
    except PermissionError as e:
        raise BuildError(
            f"cannot remove {path}: {e}. Earlier builds ran the toolchain as root; "
            f"remove it once with: docker run --rm -v {path.parent.resolve()}:/p "
            f"alpine rm -rf /p/{path.name}") from e


def _restore_pristine(dest: Path, sha: str) -> bool:
    """Reset an existing clone to exactly the pinned commit with no build
    outputs or applied patches left over. False if it can't be reused."""
    if not (dest / ".git").is_dir():
        return False
    head = sh(["git", "rev-parse", "HEAD"], cwd=dest, check=False)
    if head.returncode != 0 or head.stdout.strip() != sha:
        return False
    sh(["git", "reset", "-q", "--hard", sha], cwd=dest)
    proc = sh(["git", "clean", "-qffdx"], cwd=dest, check=False)
    if proc.returncode != 0:
        raise BuildError(f"cannot clean {dest} (root-owned build outputs?):\n{proc.stderr}")
    return True


def clone_at_sha(url: str, sha: str, dest: Path, *, skip_if_exists: bool = False) -> None:
    """git-clone-at-SHA: works for any commit GitHub still has reachable, not
    just a ref it advertises (`git fetch <sha>` against a public GitHub repo
    is allowed even though the SHA isn't a branch/tag tip).

    With skip_if_exists an existing clone is reused only after it is reset to
    the pinned SHA and cleaned: a previous run leaves rebuilt (and possibly
    patched) artifacts in place, and hashing those as "upstream's shipped
    binary" would make the golden gate compare the rebuild with itself."""
    if dest.exists():
        if skip_if_exists and _restore_pristine(dest, sha):
            return
        _rmtree(dest)
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


def _user_args() -> list[str]:
    # Run as the invoking user: build outputs stay removable by build.py and
    # git clean. HOME points somewhere writable for tools that want one.
    if hasattr(os, "getuid"):
        return ["--user", f"{os.getuid()}:{os.getgid()}", "-e", "HOME=/tmp"]
    return []


def run_in_toolchain(image: str, clone_dir: Path, workdir: str, commands: list[str]) -> None:
    script = " && ".join(commands)
    container_workdir = f"/work/{workdir}" if workdir not in (".", "") else "/work"
    proc = subprocess.run(
        [
            "docker", "run", "--rm", *_user_args(),
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


def image_info(image: str) -> tuple[str, dict]:
    """(image id, {tool: version}) straight from the image that builds the
    ROMs: build-image.sh stores every tool's captured --version as a
    "tool.<name>.version" label. Falls back to versions.json (written by the
    same script) only when the labels are unreadable."""
    proc = subprocess.run(["docker", "image", "inspect", "--format",
                           "{{.Id}}\t{{json .Config.Labels}}", image],
                          capture_output=True, text=True)
    image_id, tools = "", {}
    if proc.returncode == 0 and "\t" in proc.stdout:
        image_id, labels_json = proc.stdout.strip().split("\t", 1)
        try:
            labels = json.loads(labels_json) or {}
        except json.JSONDecodeError:
            labels = {}
        for k, v in labels.items():
            if k.startswith("tool.") and k.endswith(".version"):
                tools[k[len("tool."):-len(".version")]] = v.strip()
    if not tools and VERSIONS_PATH.exists():
        try:
            tools = json.loads(VERSIONS_PATH.read_text()).get("tools", {})
        except (OSError, json.JSONDecodeError):
            tools = {}
    return image_id, tools


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
        # The build must really produce the artifact: if the shipped file
        # stayed in place, a recipe whose commands write somewhere else would
        # "rebuild" it byte-identically and pass the gate without building.
        shipped.unlink()

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
    image_id, tool_versions = image_info(image)

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
            # Content-addressed id of the image that built it (the tag is
            # mutable). No build timestamp: rebuilding an unchanged recipe
            # must leave the committed manifest byte-identical.
            "toolchain_image_id": image_id,
            "license": recipe.license,
        }
    return manifest_entries


def find_recipes(selector: Optional[str]) -> list[Path]:
    if selector:
        p = ROMS_SRC / selector / "recipe.json"
        if not p.exists():
            raise BuildError(f"no recipe at {p}")
        return [p]
    return sorted(ROMS_SRC.glob("*/*/recipe.json"))


def read_manifest() -> dict:
    if not MANIFEST_PATH.exists():
        return {"roms": {}}
    try:
        return json.loads(MANIFEST_PATH.read_text())
    except (OSError, json.JSONDecodeError) as e:
        # Never silently start a fresh ledger over a damaged one.
        raise BuildError(f"{MANIFEST_PATH} is unreadable ({e}); fix or restore it from git") from e


def write_manifest(new_entries: dict, image: str) -> None:
    manifest = read_manifest()
    manifest.setdefault("roms", {}).update(new_entries)
    manifest["tool_versions"] = image_info(image)[1]
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def check_manifest(new_entries: dict) -> list[str]:
    """--check: every rebuilt ROM must match the sha256 already on record."""
    roms = read_manifest().get("roms", {})
    problems = []
    for key, entry in sorted(new_entries.items()):
        rec = roms.get(key)
        if rec is None:
            problems.append(f"{key}: not in rom_manifest.json (run build.py without --check)")
        elif rec.get("sha256") != entry["sha256"]:
            problems.append(f"{key}: rebuilt {entry['sha256']} != recorded {rec.get('sha256')}")
    return problems


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("suite", nargs="?", help="console/suite, e.g. nes/cpu_dummy_reads (default: all)")
    ap.add_argument("--image", default=DEFAULT_IMAGE, help=f"toolchain image (default: {DEFAULT_IMAGE})")
    ap.add_argument("--skip-clone", action="store_true",
                    help="reuse an existing upstream/ checkout (reset to the pinned SHA and cleaned)")
    ap.add_argument("--check", action="store_true",
                    help="verify rebuilt sha256s against rom_manifest.json instead of writing it")
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

    if all_entries and args.check:
        try:
            problems = check_manifest(all_entries)
        except BuildError as e:
            problems = [str(e)]
        for pr in problems:
            print(f"MISMATCH {pr}", file=sys.stderr)
        if problems:
            return 1
        print(f"{len(all_entries)} ROM(s) match rom_manifest.json", file=sys.stderr)
    elif all_entries:
        try:
            write_manifest(all_entries, args.image)
        except BuildError as e:
            print(f"error: {e}", file=sys.stderr)
            return 1
        print(f"wrote {MANIFEST_PATH} ({len(all_entries)} ROM(s))", file=sys.stderr)

    if failures:
        print(f"{len(failures)} suite(s) failed: {', '.join(failures)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
