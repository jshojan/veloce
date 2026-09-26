"""
rom_manifest.json loader (SH-3b).

`tests/roms-src/build.py` writes one shared manifest at
`tests/roms-src/rom_manifest.json` recording, per built ROM, the sha256 it
produced, the upstream source it was built from, the patches applied (if
any) and the toolchain image's tool versions. The harness reads it here to
enforce the SH-3 rule from the plan (section 5): "the harness refuses to
score a `rom_variant: veloce` ROM whose hash is not in the manifest" — i.e. a
config can only claim `rom_variant: veloce` for a ROM whose reproducible
build is on record.

The manifest key is the ROM's path relative to the console's `roms_dir`
(the same string a `TestSpec.file` uses), so `Harness.run_test` can look a
ROM up by the path it already resolved.
"""

from __future__ import annotations

import functools
import hashlib
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


DEFAULT_MANIFEST_REL = Path("tests") / "roms-src" / "rom_manifest.json"


@dataclass
class RomEntry:
    sha256: str
    console: str = ""
    suite: str = ""
    rom_variant: str = "upstream"
    source_repo: str = ""
    source_sha: str = ""
    patches: list = field(default_factory=list)
    tool_versions: dict = field(default_factory=dict)
    built_at: str = ""
    raw: dict = field(default_factory=dict)


@dataclass
class RomManifest:
    path: Path
    exists: bool
    roms: dict[str, RomEntry] = field(default_factory=dict)
    tool_versions: dict = field(default_factory=dict)

    def lookup(self, rel_path: str) -> Optional[RomEntry]:
        # Keys are stored with forward slashes regardless of platform.
        return self.roms.get(str(rel_path).replace("\\", "/"))


def manifest_path(project_root: Path) -> Path:
    return Path(project_root) / DEFAULT_MANIFEST_REL


def load_manifest(project_root: Path) -> RomManifest:
    p = manifest_path(project_root)
    if not p.exists():
        return RomManifest(path=p, exists=False)
    try:
        raw = json.loads(p.read_text())
    except (OSError, json.JSONDecodeError):
        return RomManifest(path=p, exists=False)
    roms = {}
    for key, entry in (raw.get("roms") or {}).items():
        roms[key] = RomEntry(
            sha256=entry.get("sha256", ""),
            console=entry.get("console", ""),
            suite=entry.get("suite", ""),
            rom_variant=entry.get("rom_variant", "upstream"),
            source_repo=entry.get("source_repo", ""),
            source_sha=entry.get("source_sha", ""),
            patches=entry.get("patches", []) or [],
            tool_versions=entry.get("tool_versions", {}) or {},
            built_at=entry.get("built_at", ""),
            raw=entry,
        )
    return RomManifest(path=p, exists=True, roms=roms, tool_versions=raw.get("tool_versions", {}) or {})


@functools.lru_cache(maxsize=8)
def _cached_load(project_root_str: str, mtime: float) -> RomManifest:
    return load_manifest(Path(project_root_str))


def load_manifest_cached(project_root: Path) -> RomManifest:
    """Like load_manifest, but re-reads only when the file's mtime changes
    (the harness calls this once per test in a run)."""
    p = manifest_path(project_root)
    try:
        mtime = p.stat().st_mtime
    except OSError:
        mtime = -1.0
    return _cached_load(str(project_root), mtime)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def check_rom_variant(
    project_root: Path, rel_path: str, rom_path: Path, rom_variant: str
) -> Optional[str]:
    """Returns None when a `veloce` rom_variant ROM is manifest-clean, else a
    human-readable SKIP reason. Always None for `rom_variant: upstream` (the
    manifest is optional there; the golden gate already covers provenance)."""
    if rom_variant != "veloce":
        return None
    manifest = load_manifest_cached(project_root)
    if not manifest.exists:
        return f"rom_variant veloce but no {DEFAULT_MANIFEST_REL} (run tests/roms-src/build.py)"
    entry = manifest.lookup(rel_path)
    if entry is None:
        return f"rom_variant veloce but '{rel_path}' is not in rom_manifest.json"
    try:
        actual = sha256_file(rom_path)
    except OSError as e:
        return f"rom_variant veloce but ROM unreadable for hash check: {e}"
    if actual != entry.sha256:
        return (
            f"rom_variant veloce but sha256 mismatch for '{rel_path}' "
            f"(built {entry.sha256[:12]}…, on disk {actual[:12]}…; rebuild with build.py)"
        )
    return None
