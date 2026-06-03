"""
Result-detection conventions for every detection method.

All four detection protocols return a DetectionResult so the scoring layer is
agnostic to how a verdict was obtained.

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
from dataclasses import dataclass
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
class DetectionResult:
    status: TestStatus
    detail: str = ""            # human text (error code, divergent line, hash, ...)
    status_code: Optional[int] = None
    # progress in [0,1] for partial-credit aware methods (cpu-trace). For binary
    # methods this is 1.0 on pass and 0.0 on fail.
    progress: float = 0.0


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
