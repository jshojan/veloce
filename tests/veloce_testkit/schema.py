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
  "result_policy": "legacy",               # legacy | strict (see VALIDATION below)
  "rom_build": {"dockerfile": "tools/rom-toolchain/Dockerfile",
                "recipes": "tests/roms-src/<console>"},   # recipe dir for rom_variant "veloce"
  "repositories": { <id>: {url,dir,type,license,...} },
  "test_suites": { <suite_id>: SuiteSpec },
  "visual_test_suites": { <suite_id>: SuiteSpec },   # non-scoring pixel tests
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
  "result_detection": "file",              # file | cpu-trace | screenshot-crc
                                           #   (memory | serial: DEPRECATED aliases of file)
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
  "trace_limit": 8991,                     # # of trace lines to compare
  # VELOCE-RESULT/1 file extras (docs/testing/VELOCE-RESULT.md):
  "channel": "port",                       # what the ROM reports through: auto | one of
                                           #   CONSOLE_CHANNELS[console]
  "require_channel": "port",               # verdict must come from this channel, else SKIP
  "rom_variant": "veloce",                 # upstream | veloce (built from patched source)
  "expected_checks": 12,                   # fewer CHECK lines than this -> FAIL (died early)
  "allow_empty": false,                    # END 0/0 is a PASS (smoke ROMs)
  "resets": 3,                             # max ROM-requested resets (VELOCE_TEST_RESETS)
  "input": "200:40,201:0"                  # deterministic INPUT= schedule (frame:hexmask)
}

===========================================================================
VALIDATION (validate_config)
===========================================================================
Always: channel / require_channel must be one the console supports;
channel only on file-family tests; rom_variant "veloce" needs rom_build.recipes
to exist; expected_checks / resets are non-negative ints; input is well formed.
result_policy "strict" (set by the config flip, SH-7) additionally rejects:
bare screenshot-crc tests in test_suites (move them to visual_test_suites),
Tier C channels (mooneye, r12), and the deprecated memory/serial aliases.
Under "legacy" those are reported as warnings.
"""

from __future__ import annotations

import json
import re
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
    FILE = "file"                    # VELOCE-RESULT/1 file written via VELOCE_TEST_OUT
    MEMORY = "memory"                # DEPRECATED alias of FILE (blargg $6000 stdout fallback)
    SERIAL = "serial"                # DEPRECATED alias of FILE (serial / GBA stdout fallback)
    SCREENSHOT_CRC = "screenshot-crc"  # CRC32 of captured frame vs reference (non-scoring in strict)
    CPU_TRACE = "cpu-trace"          # golden trace log compare (e.g. nestest.log)


# Methods whose verdict comes from the VELOCE-RESULT/1 file. MEMORY/SERIAL keep
# a stdout fallback until every core emits to the sink (removed in SH-8).
FILE_FAMILY = (DetectionMethod.FILE, DetectionMethod.MEMORY, DetectionMethod.SERIAL)
LEGACY_ALIASES = (DetectionMethod.MEMORY, DetectionMethod.SERIAL)

# Channel drivers per console (header "channels=", "#VELOCE adapter=", config
# "channel"/"require_channel"). Tier A = ROM-emitted stream from patched source;
# Tier C = CPU-register heuristics, bootstrap only.
CONSOLE_CHANNELS = {
    "nes": ("port", "blargg6000"),
    "snes": ("port", "blargg6000", "stp", "sram", "spcport"),
    "gb": ("serial", "mooneye", "a000", "hram"),
    "gba": ("mgba", "r12"),
}
TIER_A_CHANNELS = ("port", "serial", "mgba")
TIER_C_CHANNELS = ("mooneye", "r12")
ROM_VARIANTS = ("upstream", "veloce")
RESULT_POLICIES = ("legacy", "strict")


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
    # VELOCE-RESULT/1 file extras
    channel: str = "auto"
    require_channel: str = ""
    rom_variant: str = "upstream"
    expected_checks: int = 0
    allow_empty: bool = False
    resets: int = 3
    input: str = ""
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
    visual: bool = False             # declared under visual_test_suites (non-scoring)


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
    rom_build: dict = field(default_factory=dict)
    result_policy: str = "legacy"


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
        channel=raw.get("channel") or "auto",
        require_channel=raw.get("require_channel", "") or "",
        rom_variant=raw.get("rom_variant") or "upstream",
        expected_checks=_as_int(raw.get("expected_checks"), 0),
        allow_empty=bool(raw.get("allow_empty", False)),
        resets=_as_int(raw.get("resets"), 3),
        # Non-string values are kept out of the INPUT= env var (the validator
        # reports them); a list here would otherwise crash the harness.
        input=raw["input"] if isinstance(raw.get("input"), str) else "",
        raw=raw,
    )


def _as_int(value, default: int) -> int:
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


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

    visual_ids = set(raw.get("visual_test_suites", {}))
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
                visual=suite_id in visual_ids,
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
        rom_build=raw.get("rom_build", {}) or {},
        result_policy=raw.get("result_policy", "legacy") or "legacy",
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


_INPUT_RE = re.compile(r"^\d+:[0-9A-Fa-f]+(,\d+:[0-9A-Fa-f]+)*$")


def _repo_root_for(config_path: Path) -> Path:
    # cores/<console>/tests/test_config.json -> repo root
    p = config_path.resolve()
    for parent in p.parents:
        if (parent / "cores").is_dir() and (parent / "tests").is_dir():
            return parent
    return p.parent


def validate_config(
    path: str | Path,
    console: Optional[str] = None,
    warnings: Optional[list[str]] = None,
) -> list[str]:
    """Return a list of human-readable validation errors (empty == valid).

    Non-fatal findings (rules that only become errors under
    result_policy "strict") are appended to `warnings` when a list is given.
    Console agents should run this in CI before committing a test_config.json.
    """
    errors: list[str] = []
    warns: list[str] = warnings if warnings is not None else []
    try:
        cfg = load_config(path, console)
    except Exception as e:  # noqa: BLE001
        return [f"failed to parse: {e}"]

    if cfg.console not in VALID_CONSOLES:
        errors.append(f"invalid console '{cfg.console}'")
    if cfg.result_policy not in RESULT_POLICIES:
        errors.append(f"result_policy must be one of {RESULT_POLICIES}, got '{cfg.result_policy}'")
    strict = cfg.result_policy == "strict"
    console_channels = CONSOLE_CHANNELS.get(cfg.console, ())

    recipes = (cfg.rom_build or {}).get("recipes", "")
    recipes_ok = bool(recipes) and (_repo_root_for(Path(path)) / recipes).exists()

    bare_visual: list[str] = []
    tier_c: list[str] = []
    aliases = 0

    seen_ids: set[str] = set()
    for suite in cfg.suites:
        if not suite.tests:
            errors.append(f"suite '{suite.id}' has no tests")
        for t in suite.tests:
            if t.id in seen_ids:
                errors.append(f"duplicate test id '{t.id}'")
            seen_ids.add(t.id)
            raw_det = t.raw.get("result_detection")
            if raw_det and raw_det not in [m.value for m in DetectionMethod]:
                errors.append(f"test '{t.id}' has unknown result_detection '{raw_det}'")
            if t.result_detection == DetectionMethod.SCREENSHOT_CRC and not t.reference_hash \
                    and t.expected not in ("known_fail",):
                errors.append(
                    f"test '{t.id}' is screenshot-crc but has no reference_hash "
                    "(run with --generate-refs, or mark expected=known_fail)"
                )
            if t.result_detection == DetectionMethod.CPU_TRACE and not t.trace_log:
                errors.append(f"test '{t.id}' is cpu-trace but has no trace_log")

            # --- VELOCE-RESULT/1 fields ---
            file_family = t.result_detection in FILE_FAMILY
            for key in ("channel", "require_channel"):
                val = getattr(t, key)
                if val in ("", "auto"):
                    continue
                if val not in console_channels:
                    errors.append(
                        f"test '{t.id}' {key} '{val}' is not a {cfg.console} channel "
                        f"(one of: {', '.join(console_channels)})")
                elif not file_family:
                    errors.append(f"test '{t.id}' sets {key} but is {t.result_detection.value}")
            if t.rom_variant not in ROM_VARIANTS:
                errors.append(f"test '{t.id}' rom_variant must be one of {ROM_VARIANTS}")
            elif t.rom_variant == "veloce" and not recipes_ok:
                errors.append(
                    f"test '{t.id}' is rom_variant veloce but rom_build.recipes "
                    f"{'is missing' if not recipes else repr(recipes) + ' does not exist'}")
            for key in ("expected_checks", "resets"):
                if key in t.raw and (not isinstance(t.raw[key], int) or isinstance(t.raw[key], bool)
                                     or t.raw[key] < 0):
                    errors.append(f"test '{t.id}' {key} must be a non-negative integer")
            if "allow_empty" in t.raw and not isinstance(t.raw["allow_empty"], bool):
                errors.append(f"test '{t.id}' allow_empty must be true/false")
            if "input" in t.raw and t.raw["input"] not in ("", None) \
                    and not isinstance(t.raw["input"], str):
                errors.append(f"test '{t.id}' input must be a string 'frame:hexmask,...'")
            if t.input and not _INPUT_RE.match(t.input):
                errors.append(f"test '{t.id}' input must look like 'frame:hexmask,...'")

            # --- rules that are errors only after the config flip ---
            if t.result_detection == DetectionMethod.SCREENSHOT_CRC and not suite.visual:
                bare_visual.append(t.id)
            if t.channel in TIER_C_CHANNELS or t.require_channel in TIER_C_CHANNELS:
                tier_c.append(t.id)
            if t.result_detection in LEGACY_ALIASES:
                aliases += 1

    if strict:
        for tid in bare_visual:
            errors.append(f"test '{tid}' is screenshot-crc in test_suites "
                          "(move it to visual_test_suites; screenshots never score)")
        for tid in tier_c:
            errors.append(f"test '{tid}' uses a Tier C channel (mooneye/r12); "
                          "flip it to a ROM-emitted channel")
        if aliases:
            errors.append(f"{aliases} test(s) use deprecated result_detection memory/serial; use file")
    else:
        if bare_visual:
            warns.append(f"{len(bare_visual)} screenshot-crc test(s) in test_suites "
                         "(error under result_policy strict; move to visual_test_suites)")
        if tier_c:
            warns.append(f"{len(tier_c)} test(s) use Tier C channels (error under result_policy strict)")
        if aliases:
            warns.append(f"{aliases} test(s) use deprecated memory/serial (alias of file)")
    return errors
