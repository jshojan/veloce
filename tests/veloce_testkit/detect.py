"""
Result-detection conventions for every detection method.

All detection protocols return a DetectionResult so the scoring layer is
agnostic to how a verdict was obtained.

===========================================================================
0. VELOCE-RESULT/1 FILE  (all consoles; method = "file")
===========================================================================
The binary runs with VELOCE_TEST_OUT=<path>; the application writes the ROM's
result channel to that file (header "#VELOCE 1 core= rom_crc32= channels=",
ROM lines VELOCE/INFO/TEST/CHECK/REG/LOG/END, trailer "#VELOCE end reason=").
detect_result_file() turns it into a verdict with per-check detail. This is
the only method that scores in strict configs; the full spec and the verdict
rules are in docs/testing/VELOCE-RESULT.md.

"memory" and "serial" (sections 1-3) are DEPRECATED aliases of "file": the
harness reads the result file first and only falls back to the stdout parsers
below while a core has not yet adopted the channel (no verdict in the file).

===========================================================================
1. BLARGG MEMORY PROTOCOL  (NES / SNES; method = "memory")
===========================================================================
Blargg test ROMs write a result to RAM at $6000:
    $6000        status byte:
                   0x80  -> running (not finished)
                   0x81  -> needs reset button pressed
                   0x00  -> PASSED
                   0x01..0x7F -> FAILED with that error code
    $6001..$6003 magic signature 0xDE 0xB0 0x61 (valid only once written)
    $6004..      null-terminated ASCII result text
The headless `veloce` binary, when run with DEBUG=1, prints lines:
    "BLARGG_STATUS: 0x00"
    "BLARGG_RESULT: <text>"
and/or  "Status code: 0 (PASSED)" / "Status code: N (FAILED)".
detect_blargg_memory() parses those.

===========================================================================
2. GAME BOY SERIAL PROTOCOL  (GB; method = "serial")
===========================================================================
Blargg GB ROMs echo their result over the link-port serial register (SB/SC).
The harness/binary forwards serial bytes to stdout. Convention:
   * the literal substring "Passed" (often "Passed all tests") => PASS
   * the literal substring "Failed" => FAIL (followed by which sub-test)
Mooneye ROMs instead signal via register fingerprint and a software breakpoint
(LD B,B). On success registers are B=3 C=5 D=8 E=13 H=21 L=34 (Fibonacci);
the binary prints "MOONEYE: PASS"/"MOONEYE: FAIL" which we also match.

===========================================================================
3. GBA REGISTER PROTOCOL  (GBA; method = "serial", sub-variant)
===========================================================================
jsmolka/alyosha GBA ROMs spin in an infinite loop with R12 holding the failing
test number (0 == all passed). The binary prints:
   "[GBA] PASSED"  or  "[GBA] FAILED - Failed at test #N"
detect_gba_register() parses those. Treated as the serial family for config.

===========================================================================
4. SCREENSHOT CRC  (all; method = "screenshot-crc", accuracy_type "visual")
===========================================================================
The binary is run with SAVE_SCREENSHOT=<frame> (or =<path>), producing a PNG of
the framebuffer at that frame. We CRC32 the PNG bytes and compare to the test's
"reference_hash". Equal => PASS, differ => FAIL. With --generate-refs the
measured hash is emitted for the console agent to paste into test_config.json.
NOTE: CRC is exact-match; any 1-pixel diff fails. That is intentional for
pixel-accurate tests (dmg-acid2, mealybug). Reference hashes are tied to a fixed
output resolution and PNG encoder, so regenerate them if either changes.

===========================================================================
5. CPU GOLDEN TRACE  (NES nestest; method = "cpu-trace", "cycle-accurate")
===========================================================================
The binary, with TRACE=1, emits one line per executed instruction in the
canonical nestest.log format, e.g.:
   C000  4C F5 C5  JMP $C5F5   A:00 X:00 Y:00 P:24 SP:FD CYC:7
We compare line-by-line against the golden log (trace_log). The verdict is the
first divergent line (PASS only if all compared lines match up to trace_limit).
Partial credit (fraction of matching lines) is reported for scoring nuance.
"""

from __future__ import annotations

import re
import zlib
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Optional


class TestStatus(str, Enum):
    PASS = "pass"
    FAIL = "fail"
    KNOWN_FAIL = "known_fail"   # failed, but expected to (documented hw quirk / WIP)
    RUNS = "runs"               # completed without crash, no pass/fail signal extracted
    TIMEOUT = "timeout"
    SKIP = "skip"               # ROM missing / detection prerequisite absent
    ERROR = "error"             # harness/launch error


@dataclass
class CheckResult:
    """One CHECK line of a VELOCE-RESULT/1 file."""
    id: str
    name: str
    passed: bool
    exp: str = ""
    got: str = ""
    mask: str = ""
    at: str = ""
    extra: dict = field(default_factory=dict)   # any other key=value tokens (code=, ...)

    def to_dict(self) -> dict:
        d = {"id": self.id, "name": self.name, "status": "pass" if self.passed else "fail"}
        for k in ("exp", "got", "mask", "at"):
            v = getattr(self, k)
            if v:
                d[k] = v
        d.update(self.extra)
        return d


@dataclass
class DetectionResult:
    status: TestStatus
    detail: str = ""            # human text (error code, divergent line, hash, ...)
    status_code: Optional[int] = None
    # progress in [0,1] for partial-credit aware methods (cpu-trace). For binary
    # methods this is 1.0 on pass and 0.0 on fail.
    progress: float = 0.0
    # --- VELOCE-RESULT/1 file detail (empty for the legacy stdout methods) ---
    checks: list[CheckResult] = field(default_factory=list)
    end_reason: str = ""        # terminator | frames | reset_limit | quit | missing
    frames_used: int = 0
    adapter: str = ""           # "" for a ROM-emitted stream, else blargg6000|mooneye|r12|...


# --------------------------------------------------------------------------
# 1. Blargg memory protocol
# --------------------------------------------------------------------------
def detect_blargg_memory(output: str, exit_code: int) -> DetectionResult:
    m = re.search(r"BLARGG_STATUS:\s*0x([0-9A-Fa-f]+)", output)
    if m:
        status = int(m.group(1), 16)
        if status == 0x00:
            return DetectionResult(TestStatus.PASS, "Test passed", 0, 1.0)
        if status in (0x80, 0x81):
            # still running / needs reset -> no verdict
            return DetectionResult(TestStatus.RUNS, "did not finish", status, 0.0)
        return DetectionResult(TestStatus.FAIL, f"failed code {status}", status, 0.0)

    txt = ""
    mt = re.search(r"BLARGG_RESULT:\s*(.+)", output)
    if mt:
        txt = mt.group(1).strip()

    if re.search(r"Status code:\s*0\s*\(PASSED\)", output, re.I):
        return DetectionResult(TestStatus.PASS, txt or "Passed", 0, 1.0)
    mf = re.search(r"Status code:\s*(\d+)\s*\(FAILED\)", output, re.I)
    if mf:
        return DetectionResult(TestStatus.FAIL, txt, int(mf.group(1)), 0.0)

    if re.search(r"\bpassed\b", output, re.I) and not re.search(r"\bfailed\b", output, re.I):
        return DetectionResult(TestStatus.PASS, txt or "Passed", 0, 1.0)
    if re.search(r"\bfailed\b", output, re.I):
        return DetectionResult(TestStatus.FAIL, txt or "Failed", 1, 0.0)

    if exit_code == 0:
        return DetectionResult(TestStatus.RUNS, "no result signal", None, 0.0)
    return DetectionResult(TestStatus.FAIL, "crashed / nonzero exit", None, 0.0)


# --------------------------------------------------------------------------
# 2. Game Boy serial protocol (Blargg serial + Mooneye fingerprint)
# --------------------------------------------------------------------------
def detect_serial_output(output: str, exit_code: int) -> DetectionResult:
    if "MOONEYE: PASS" in output:
        return DetectionResult(TestStatus.PASS, "mooneye pass", 0, 1.0)
    if "MOONEYE: FAIL" in output:
        return DetectionResult(TestStatus.FAIL, "mooneye fail", 1, 0.0)
    # Blargg serial text
    if "Passed" in output:
        return DetectionResult(TestStatus.PASS, "serial: Passed", 0, 1.0)
    if "Failed" in output:
        return DetectionResult(TestStatus.FAIL, "serial: Failed", 1, 0.0)
    if re.search(r"Status code:\s*0\s*\(PASSED\)", output, re.I):
        return DetectionResult(TestStatus.PASS, "Passed", 0, 1.0)
    if exit_code == 0:
        return DetectionResult(TestStatus.RUNS, "no serial verdict", None, 0.0)
    return DetectionResult(TestStatus.FAIL, "crashed / nonzero exit", None, 0.0)


# --------------------------------------------------------------------------
# 3. GBA register protocol
# --------------------------------------------------------------------------
def detect_gba_register(output: str, exit_code: int) -> DetectionResult:
    if "[GBA] PASSED" in output:
        return DetectionResult(TestStatus.PASS, "GBA passed", 0, 1.0)
    mf = re.search(r"\[GBA\]\s*FAILED.*?test\s*#?(\d+)", output)
    if mf:
        n = int(mf.group(1))
        return DetectionResult(TestStatus.FAIL, f"failed at test #{n}", n, 0.0)
    if "[GBA] FAILED" in output:
        return DetectionResult(TestStatus.FAIL, "GBA failed", 1, 0.0)
    if exit_code == 0:
        return DetectionResult(TestStatus.RUNS, "no GBA verdict", None, 0.0)
    return DetectionResult(TestStatus.FAIL, "crashed / nonzero exit", None, 0.0)


# --------------------------------------------------------------------------
# 4. Screenshot CRC
# --------------------------------------------------------------------------
def crc32_file(path: Path) -> str:
    if not Path(path).exists():
        return ""
    with open(path, "rb") as f:
        return format(zlib.crc32(f.read()) & 0xFFFFFFFF, "08x")


def detect_screenshot_crc(
    screenshot_path: Path,
    reference_hash: str,
    *,
    generate_refs: bool = False,
) -> DetectionResult:
    actual = crc32_file(screenshot_path)
    if not actual:
        return DetectionResult(TestStatus.SKIP, "no screenshot captured", None, 0.0)
    if generate_refs:
        # caller records actual into config; report as a non-scoring RUNS
        return DetectionResult(TestStatus.RUNS, f"hash={actual}", None, 0.0)
    if not reference_hash:
        return DetectionResult(
            TestStatus.SKIP, f"no reference_hash (measured {actual})", None, 0.0
        )
    if actual == reference_hash:
        return DetectionResult(TestStatus.PASS, f"hash={actual}", 0, 1.0)
    return DetectionResult(
        TestStatus.FAIL, f"hash mismatch exp {reference_hash} got {actual}", 1, 0.0
    )


# --------------------------------------------------------------------------
# 5. CPU golden trace
# --------------------------------------------------------------------------
def detect_cpu_trace(
    emitted_trace: str,
    golden_path: Path,
    *,
    limit: int = 0,
) -> DetectionResult:
    golden_path = Path(golden_path)
    if not golden_path.exists():
        return DetectionResult(TestStatus.SKIP, f"no golden log {golden_path}", None, 0.0)
    golden = golden_path.read_text(errors="replace").splitlines()
    emitted = emitted_trace.splitlines()
    n = min(len(golden), len(emitted))
    if limit:
        n = min(n, limit)
    if n == 0:
        return DetectionResult(TestStatus.ERROR, "empty trace", None, 0.0)
    for i in range(n):
        if _norm_trace_line(emitted[i]) != _norm_trace_line(golden[i]):
            return DetectionResult(
                TestStatus.FAIL,
                f"diverged at line {i + 1}: got '{emitted[i].strip()}' "
                f"want '{golden[i].strip()}'",
                i + 1,
                i / n,
            )
    return DetectionResult(TestStatus.PASS, f"{n} trace lines matched", 0, 1.0)


def _norm_trace_line(line: str) -> str:
    # Collapse whitespace so spacing differences between emitters do not
    # cause spurious divergence; comparison stays on tokens (PC, opcode, regs).
    return " ".join(line.split())


# --------------------------------------------------------------------------
# 0. VELOCE-RESULT/1 file
# --------------------------------------------------------------------------
_HEADER_RE = re.compile(r"^#VELOCE 1(?: |$)")
# Exactly "END <digits>/<digits>" followed by whitespace or end of line; the
# application's sink (src/core/test_file_sink.cpp) uses the same rule to decide
# that the run is over, so the two must agree.
_END_RE = re.compile(r"^END[ \t]+(\d+)/(\d+)(?:[ \t]|$)")
_CODE_RE = re.compile(r"^(?:0[xX]([0-9a-fA-F]+)|(\d+))$")


def _parse_code(value: str) -> Optional[int]:
    """code=<n>: decimal, or hex with an explicit 0x prefix (never octal),
    matching the sink's parser."""
    m = _CODE_RE.match(value)
    if not m:
        return None
    return int(m.group(1), 16) if m.group(1) is not None else int(m.group(2))


def _kv_tokens(tokens: list[str]) -> dict[str, str]:
    out: dict[str, str] = {}
    for t in tokens:
        if "=" in t:
            k, v = t.split("=", 1)
            out[k] = v
    return out


@dataclass
class ResultFile:
    """Parsed VELOCE-RESULT/1 file (see docs/testing/VELOCE-RESULT.md)."""
    path: Path
    exists: bool = False
    header_ok: bool = False
    core: str = ""
    rom_crc32: str = ""
    channels: list[str] = field(default_factory=list)
    adapter: str = ""
    notes: dict[str, str] = field(default_factory=dict)
    blobs: list[dict] = field(default_factory=list)
    resets: int = 0
    suite: str = ""
    console: str = ""
    info: dict[str, str] = field(default_factory=dict)
    regs: dict[str, str] = field(default_factory=dict)
    log: list[str] = field(default_factory=list)
    checks: list[CheckResult] = field(default_factory=list)
    end: Optional[tuple[int, int]] = None      # (pass, total) of the first END line
    end_code: Optional[int] = None
    end_malformed: bool = False
    malformed_checks: list[str] = field(default_factory=list)   # CHECK lines without id/PASS|FAIL
    trailer: dict[str, str] = field(default_factory=dict)   # "#VELOCE end ..." tokens

    @property
    def has_trailer(self) -> bool:
        return bool(self.trailer)

    @property
    def end_reason(self) -> str:
        return self.trailer.get("reason", "missing") if self.trailer else "missing"

    @property
    def frames_used(self) -> int:
        try:
            return int(self.trailer.get("frames", "0"))
        except ValueError:
            return 0

    @property
    def trailer_status(self) -> Optional[int]:
        v = self.trailer.get("status")
        try:
            return int(v) if v is not None else None
        except ValueError:
            return None

    @property
    def has_verdict(self) -> bool:
        """True once the core put a verdict on the channel (END, CHECK, or an
        adapter terminator). False for a core without a channel."""
        return (self.end is not None or self.end_malformed or bool(self.checks)
                or bool(self.malformed_checks)
                or self.trailer_status is not None
                or self.end_reason == "reset_limit")


def parse_result_file(path: str | Path) -> ResultFile:
    path = Path(path)
    rf = ResultFile(path=path)
    if not path.exists():
        return rf
    rf.exists = True
    lines = path.read_text(encoding="ascii", errors="replace").splitlines()
    if not lines or not _HEADER_RE.match(lines[0]):
        return rf
    rf.header_ok = True
    head = _kv_tokens(lines[0].split()[2:])
    rf.core = head.get("core", "")
    rf.rom_crc32 = head.get("rom_crc32", "")
    rf.channels = [c for c in head.get("channels", "").split(",") if c]

    for line in lines[1:]:
        if line.startswith("#VELOCE"):
            rest = line[len("#VELOCE"):].strip()
            if not rest:
                continue
            first = rest.split(None, 1)[0]
            if "=" in first:                       # single note: key=value-to-EOL
                key, value = rest.split("=", 1)
                rf.notes[key] = value
                if key == "adapter":
                    rf.adapter = value.strip()
                continue
            toks = rest.split()
            kv = _kv_tokens(toks[1:])
            if first == "end":
                rf.trailer = kv
            elif first == "blob":
                rf.blobs.append(kv)
            elif first == "reset":
                try:
                    rf.resets = max(rf.resets, int(kv.get("n", "0")))
                except ValueError:
                    pass
            continue

        toks = line.split()
        if not toks:
            continue
        kw = toks[0]
        if rf.end is not None or rf.end_malformed:
            # Only the first END counts; anything after it is informational.
            if kw == "LOG":
                rf.log.append(line[4:])
            continue
        if kw == "VELOCE" and len(toks) >= 4:
            rf.console, rf.suite = toks[2], toks[3]
        elif kw == "INFO":
            rf.info.update(_kv_tokens(toks[1:]))
        elif kw == "REG":
            rf.regs.update(_kv_tokens(toks[1:]))
        elif kw == "LOG":
            rf.log.append(line[4:])
        elif kw == "CHECK" and len(toks) >= 3 and toks[2] in ("PASS", "FAIL"):
            name_parts: list[str] = []
            rest_toks = toks[3:]
            i = 0
            while i < len(rest_toks) and "=" not in rest_toks[i]:
                name_parts.append(rest_toks[i])
                i += 1
            kv = _kv_tokens(rest_toks[i:])
            rf.checks.append(CheckResult(
                id=toks[1], name=" ".join(name_parts), passed=toks[2] == "PASS",
                exp=kv.pop("exp", ""), got=kv.pop("got", ""),
                mask=kv.pop("mask", ""), at=kv.pop("at", ""), extra=kv,
            ))
        elif kw == "CHECK":
            # A broken emitter must not be able to drop a failing check from
            # the tally ("CHECK 3 fail x" + "END 2/2" would otherwise PASS).
            rf.malformed_checks.append(line)
        elif kw == "END":
            m = _END_RE.match(line)
            if m:
                rf.end = (int(m.group(1)), int(m.group(2)))
                code = _kv_tokens(toks[2:]).get("code")
                if code is not None:
                    rf.end_code = _parse_code(code)
            else:
                rf.end_malformed = True
    return rf


def _fail_summary(checks: list[CheckResult], limit: int = 3) -> str:
    parts = []
    for c in [c for c in checks if not c.passed][:limit]:
        s = f"{c.id} {c.name}".strip()
        if c.exp or c.got:
            s += f" exp={c.exp} got={c.got}"
        if c.mask:
            s += f" mask={c.mask}"
        parts.append(s)
    return "; ".join(parts)


def detect_result_file(
    path: str | Path,
    *,
    expected_checks: int = 0,
    require_channel: str = "",
    allow_empty: bool = False,
    timed_out: bool = False,
    exit_code: Optional[int] = None,
    parsed: Optional[ResultFile] = None,
) -> DetectionResult:
    """Verdict from a VELOCE-RESULT/1 file. Rules: docs/testing/VELOCE-RESULT.md s.3."""
    rf = parsed if parsed is not None else parse_result_file(path)

    def mk(status: TestStatus, detail: str, code: Optional[int] = None,
           progress: float = 0.0) -> DetectionResult:
        return DetectionResult(
            status, detail, code, progress,
            checks=list(rf.checks), end_reason=rf.end_reason if rf.header_ok else "",
            frames_used=rf.frames_used, adapter=rf.adapter,
        )

    # 1. file / header
    if not rf.exists:
        return mk(TestStatus.TIMEOUT if timed_out else TestStatus.ERROR,
                  f"no result file {rf.path.name}")
    if not rf.header_ok:
        return mk(TestStatus.TIMEOUT if timed_out else TestStatus.ERROR,
                  "missing '#VELOCE 1' header")

    # 2. channel requirement (Tier A demands a modified ROM, not an adapter)
    if require_channel:
        if rf.adapter and rf.adapter != require_channel:
            return mk(TestStatus.SKIP,
                      f"needs modified ROM (channel {require_channel}, got adapter {rf.adapter})")
        if require_channel not in rf.channels:
            return mk(TestStatus.SKIP, f"core has no '{require_channel}' channel")

    passed = sum(1 for c in rf.checks if c.passed)
    total = len(rf.checks)
    frames = f" after {rf.frames_used} frames" if rf.has_trailer else ""

    # 3. END present
    if rf.malformed_checks:
        return mk(TestStatus.ERROR,
                  f"malformed CHECK line: {rf.malformed_checks[0][:80]!r} "
                  f"({len(rf.malformed_checks)} total)")
    if rf.end_malformed:
        return mk(TestStatus.ERROR, "malformed END line")
    if rf.end is not None:
        p, t = rf.end
        if total and (p, t) != (passed, total):
            return mk(TestStatus.ERROR,
                      f"tally mismatch: END {p}/{t} but CHECK lines {passed}/{total}")
        if t == 0:
            if allow_empty:
                return mk(TestStatus.PASS, "END 0/0 (allow_empty)", 0, 1.0)
            return mk(TestStatus.ERROR, "END 0/0 with no checks (set allow_empty for smoke ROMs)")
        if expected_checks and t < expected_checks:
            return mk(TestStatus.FAIL,
                      f"only {t} of {expected_checks} expected checks ran ({p} passed)",
                      rf.end_code if rf.end_code is not None else expected_checks - p,
                      p / expected_checks)
        if p == t:
            return mk(TestStatus.PASS, f"{p}/{t} checks passed", 0, 1.0)
        code = rf.end_code if rf.end_code is not None else t - p
        detail = f"{p}/{t} checks passed"
        summary = _fail_summary(rf.checks)
        if summary:
            detail += f": {summary}"
        elif rf.end_code is not None:
            detail = f"failed code {rf.end_code}"
        return mk(TestStatus.FAIL, detail, code, p / t)

    # 4. no END
    progress = passed / total if total else 0.0
    reason = rf.end_reason
    if reason == "reset_limit":
        return mk(TestStatus.FAIL, f"reset limit reached ({rf.resets} resets)", None, progress)
    ts = rf.trailer_status
    if reason == "terminator" and ts is not None and ts >= 0:
        if ts == 0 and not any(not c.passed for c in rf.checks):
            return mk(TestStatus.PASS, "terminator status 0", 0, 1.0)
        return mk(TestStatus.FAIL, f"terminator status {ts}", ts, progress)
    if not timed_out and not rf.has_trailer and exit_code not in (None, 0):
        return mk(TestStatus.ERROR,
                  f"no END; process exited with code {exit_code} before the trailer "
                  f"(crash?); {passed}/{total} checks passed", None, progress)
    if timed_out or not rf.has_trailer:
        return mk(TestStatus.TIMEOUT,
                  f"no END; {passed}/{total} checks passed before the run was killed",
                  None, progress)
    if total and passed < total:
        return mk(TestStatus.FAIL,
                  f"no END{frames}; {passed}/{total} checks passed: {_fail_summary(rf.checks)}",
                  total - passed, progress)
    if total:
        return mk(TestStatus.RUNS, f"no END{frames}; {passed}/{total} checks passed",
                  None, progress)
    if not rf.channels:
        return mk(TestStatus.RUNS, f"core has no result channel (no verdict{frames})")
    return mk(TestStatus.RUNS, f"no result on channel{frames}")
