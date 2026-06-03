"""
Veloce standard test_config.json schema (v2).

Every console's cores/<c>/tests/test_config.json MUST conform to this schema.
It is backward-friendly: the existing per-test fields ("path", "expected",
"notes", "screenshot_frame", "reference_hash", "test_type") are still accepted,
and missing new fields are filled with documented defaults so the legacy runners
keep working while the four console agents migrate.

===========================================================================
TOP-LEVEL DOCUMENT
===========================================================================
{
  "schema_version": 2,
  "console": "snes",                      # nes | snes | gb | gba
  "description": "...",
  "timeout_seconds": 60,                   # default per-test wall-clock timeout
  "frame_limit": 1800,                     # default FRAMES= if a test omits it
  "repositories": { <id>: {url,dir,type,license,...} },
  "test_suites": { <suite_id>: SuiteSpec },
  "known_issues": { ... },                 # free-form, human notes
  "references": { ... }                    # free-form citation links
}

===========================================================================
SuiteSpec
===========================================================================
{
  "name": "PPU VBlank/NMI",
  "description": "...",
  "subsystem": "ppu",                      # canonical subsystem key (see scoring.SUBSYSTEM_WEIGHTS)
  "priority": "critical",                  # critical | high | medium | low (suite default)
  "repo": "blargg",                        # repository id the ROMs live under
  "tests": [ TestSpec, ... ]
}

===========================================================================
TestSpec  (one ROM)
===========================================================================
{
  "id": "ppu_vbl_nmi.01_vbl_basics",       # stable unique id (defaults to slug of path)
  "file": "ppu_vbl_nmi/rom_singles/01-vbl_basics.nes",   # alias: "path"
  "subsystem": "ppu",                      # overrides suite subsystem if present
  "accuracy_type": "timing",               # functional | timing | cycle-accurate | visual
  "result_detection": "memory",            # memory | serial | screenshot-crc | cpu-trace
  "expected": "pass",                      # pass | known_fail | <int status> | <crc hex>
  "priority": "critical",                  # overrides suite priority if present
  "source_url": "https://github.com/christopherpow/nes-test-roms",
  "license": "see upstream (Blargg, public domain test code)",
  "notes": "...",
  # detection-specific extras:
  "frames": 1800,                          # FRAMES= override
  "screenshot_frame": 60,                  # for screenshot-crc / visual
  "reference_hash": "a1b2c3d4",            # CRC32 hex for screenshot-crc
  "trace_log": "nestest/nestest.log",      # golden log for cpu-trace
  "trace_limit": 8991                      # # of trace lines to compare
}
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Optional

SCHEMA_VERSION = 2

VALID_CONSOLES = ("nes", "snes", "gb", "gba")


class AccuracyType(str, Enum):
    """How rigorous the test is. Drives the rigor weight in scoring."""
    FUNCTIONAL = "functional"        # does the feature work at all (smoke / instr correctness)
    TIMING = "timing"                # sub-instruction / event timing
    CYCLE_ACCURATE = "cycle-accurate"  # exact per-cycle behavior (hardest)
    VISUAL = "visual"                # pixel-accurate rendering (acid2, mealybug)


class DetectionMethod(str, Enum):
    MEMORY = "memory"                # Blargg $6000 status protocol
    SERIAL = "serial"               # Game Boy serial / textual PASS-FAIL stream
    SCREENSHOT_CRC = "screenshot-crc"  # CRC32 of captured frame vs reference
    CPU_TRACE = "cpu-trace"          # golden trace log compare (e.g. nestest.log)


class Priority(str, Enum):
    CRITICAL = "critical"
    HIGH = "high"
    MEDIUM = "medium"
    LOW = "low"


# Maps a console to its default detection method when a test omits one.
DEFAULT_DETECTION = {
    "nes": DetectionMethod.MEMORY,
    "snes": DetectionMethod.MEMORY,
    "gb": DetectionMethod.SERIAL,
    "gba": DetectionMethod.SERIAL,
}


@dataclass
class TestSpec:
    id: str
    file: str
    subsystem: str
    accuracy_type: AccuracyType
    result_detection: DetectionMethod
    expected: str = "pass"
    priority: Priority = Priority.MEDIUM
    source_url: str = ""
    license: str = ""
    notes: str = ""
    repo: str = ""
    # detection extras
    frames: Optional[int] = None
    screenshot_frame: int = 300
    reference_hash: str = ""
    trace_log: str = ""
    trace_limit: int = 0
    raw: dict = field(default_factory=dict)


@dataclass
class SuiteSpec:
    id: str
    name: str
    description: str
    subsystem: str
    priority: Priority
    repo: str
    tests: list[TestSpec] = field(default_factory=list)


@dataclass
class ConsoleConfig:
    console: str
    description: str
    schema_version: int
    timeout_seconds: int
    frame_limit: int
    repositories: dict[str, dict]
    suites: list[SuiteSpec]
    raw: dict = field(default_factory=dict)


def _coerce_enum(enum_cls, value, default):
    if value is None:
        return default
    try:
        return enum_cls(value)
    except ValueError:
        return default


def _slug(s: str) -> str:
    out = []
    for ch in s:
        out.append(ch if (ch.isalnum()) else "_")
    return "".join(out).strip("_").lower()


def _parse_test(
    raw: dict,
    *,
    console: str,
    suite_id: str,
    suite_subsystem: str,
    suite_priority: Priority,
    suite_repo: str,
) -> TestSpec:
    # Accept both new "file" and legacy "path".
    file = raw.get("file") or raw.get("path")
    if not file:
        raise ValueError(f"test in suite '{suite_id}' has no 'file'/'path'")

    subsystem = raw.get("subsystem", suite_subsystem)
    priority = _coerce_enum(Priority, raw.get("priority"), suite_priority)

    # accuracy_type: explicit, else inferred from legacy test_type, else functional
    legacy_type = raw.get("test_type")
    if raw.get("accuracy_type"):
        acc = _coerce_enum(AccuracyType, raw["accuracy_type"], AccuracyType.FUNCTIONAL)
    elif legacy_type == "visual":
        acc = AccuracyType.VISUAL
    else:
        acc = AccuracyType.FUNCTIONAL

    # detection: explicit, else inferred (visual->screenshot, trace_log->cpu-trace,
    # else console default).
    if raw.get("result_detection"):
        det = _coerce_enum(DetectionMethod, raw["result_detection"], DEFAULT_DETECTION[console])
    elif legacy_type == "visual" or raw.get("reference_hash"):
        det = DetectionMethod.SCREENSHOT_CRC
    elif raw.get("trace_log"):
        det = DetectionMethod.CPU_TRACE
    else:
        det = DEFAULT_DETECTION[console]

    tid = raw.get("id") or f"{suite_id}.{_slug(Path(file).stem)}"

    return TestSpec(
        id=tid,
        file=file,
        subsystem=subsystem,
        accuracy_type=acc,
        result_detection=det,
        expected=str(raw.get("expected", "pass")),
        priority=priority,
        source_url=raw.get("source_url", ""),
        license=raw.get("license", ""),
        notes=raw.get("notes", ""),
        repo=raw.get("repo", suite_repo),
        frames=raw.get("frames"),
        screenshot_frame=raw.get("screenshot_frame", 300),
        reference_hash=raw.get("reference_hash", ""),
        trace_log=raw.get("trace_log", ""),
        trace_limit=raw.get("trace_limit", 0),
        raw=raw,
    )


def load_config(path: str | Path, console: Optional[str] = None) -> ConsoleConfig:
    """Parse a console test_config.json into a normalized ConsoleConfig.

    `console` is inferred from the document's "console" field if not supplied.
    Both top-level "test_suites" and "visual_test_suites" are merged.
    """
    path = Path(path)
    with open(path) as f:
        raw = json.load(f)

    console = console or raw.get("console")
    if console not in VALID_CONSOLES:
        # Fall back to inferring from the path: cores/<console>/tests/...
        for part in path.parts:
            if part in VALID_CONSOLES:
                console = part
                break
    if console not in VALID_CONSOLES:
        raise ValueError(
            f"Cannot determine console for {path}; add a top-level \"console\" key."
        )

    all_suites = {**raw.get("test_suites", {}), **raw.get("visual_test_suites", {})}

    suites: list[SuiteSpec] = []
    for suite_id, sc in all_suites.items():
        if suite_id.startswith("_"):
            continue
        suite_priority = _coerce_enum(Priority, sc.get("priority"), Priority.MEDIUM)
        suite_subsystem = sc.get("subsystem", _infer_subsystem(suite_id, sc.get("name", "")))
        suite_repo = sc.get("repo") or sc.get("repository", "")
        tests = [
            _parse_test(
                t,
                console=console,
                suite_id=suite_id,
                suite_subsystem=suite_subsystem,
                suite_priority=suite_priority,
                suite_repo=suite_repo,
            )
            for t in sc.get("tests", [])
        ]
        suites.append(
            SuiteSpec(
                id=suite_id,
                name=sc.get("name", suite_id),
                description=sc.get("description", ""),
                subsystem=suite_subsystem,
                priority=suite_priority,
                repo=suite_repo,
                tests=tests,
            )
        )

    return ConsoleConfig(
        console=console,
        description=raw.get("description", ""),
        schema_version=raw.get("schema_version", 1),
        timeout_seconds=raw.get("timeout_seconds", 60),
        frame_limit=raw.get("frame_limit", 1800),
        repositories=raw.get("repositories", {}),
        suites=suites,
        raw=raw,
    )


# Best-effort subsystem inference for legacy suites that have no "subsystem" key.
# Console agents should add explicit "subsystem" keys; this is only a fallback.
_SUBSYSTEM_HINTS = {
    "cpu": "cpu", "instr": "cpu", "arm": "cpu", "thumb": "cpu", "65816": "cpu",
    "spc": "apu", "dsp": "apu", "apu": "apu", "sound": "apu", "dmc": "apu",
    "ppu": "ppu", "sprite": "ppu", "vbl": "ppu", "nmi": "ppu", "acid": "ppu",
    "mealybug": "ppu", "render": "ppu",
    "timer": "timing", "timing": "timing", "dma": "timing", "hdma": "timing",
    "irq": "timing", "interrupt": "timing",
    "mmc": "mapper", "mapper": "mapper",
    "mem": "memory", "memory": "memory",
}


def _infer_subsystem(suite_id: str, name: str) -> str:
    hay = (suite_id + " " + name).lower()
    for hint, sub in _SUBSYSTEM_HINTS.items():
        if hint in hay:
            return sub
    return "misc"


def validate_config(path: str | Path, console: Optional[str] = None) -> list[str]:
    """Return a list of human-readable validation errors (empty == valid).

    Console agents should run this in CI before committing a test_config.json.
    """
    errors: list[str] = []
    try:
        cfg = load_config(path, console)
    except Exception as e:  # noqa: BLE001
        return [f"failed to parse: {e}"]

    if cfg.console not in VALID_CONSOLES:
        errors.append(f"invalid console '{cfg.console}'")

    seen_ids: set[str] = set()
    for suite in cfg.suites:
        if not suite.tests:
            errors.append(f"suite '{suite.id}' has no tests")
        for t in suite.tests:
            if t.id in seen_ids:
                errors.append(f"duplicate test id '{t.id}'")
            seen_ids.add(t.id)
            if t.result_detection == DetectionMethod.SCREENSHOT_CRC and not t.reference_hash \
                    and t.expected not in ("known_fail",):
                errors.append(
                    f"test '{t.id}' is screenshot-crc but has no reference_hash "
                    "(run with --generate-refs, or mark expected=known_fail)"
                )
            if t.result_detection == DetectionMethod.CPU_TRACE and not t.trace_log:
                errors.append(f"test '{t.id}' is cpu-trace but has no trace_log")
    return errors
