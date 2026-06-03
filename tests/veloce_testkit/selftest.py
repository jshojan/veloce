#!/usr/bin/env python3
"""
Self-test for the testkit's pure logic (detection parsing + scoring math).
No emulator or ROMs required; safe to run in CI as a fast sanity gate.

  python tests/veloce_testkit/selftest.py
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tests"))

from veloce_testkit.detect import (  # noqa: E402
    detect_blargg_memory, detect_serial_output, detect_gba_register,
    detect_cpu_trace, TestStatus,
)
from veloce_testkit.schema import AccuracyType, Priority  # noqa: E402
from veloce_testkit.scoring import score_test, score_console  # noqa: E402

failures = 0


def check(name: str, cond: bool):
    global failures
    if not cond:
        failures += 1
        print(f"  FAIL: {name}")
    else:
        print(f"  ok:   {name}")


# --- detection ---
check("blargg pass", detect_blargg_memory("BLARGG_STATUS: 0x00", 0).status == TestStatus.PASS)
check("blargg fail", detect_blargg_memory("BLARGG_STATUS: 0x03", 0).status == TestStatus.FAIL)
check("blargg running->runs", detect_blargg_memory("BLARGG_STATUS: 0x80", 0).status == TestStatus.RUNS)
check("serial pass", detect_serial_output("Passed all tests", 0).status == TestStatus.PASS)
check("serial fail", detect_serial_output("Failed #4", 0).status == TestStatus.FAIL)
check("gba pass", detect_gba_register("[GBA] PASSED", 0).status == TestStatus.PASS)
check("gba fail#", detect_gba_register("[GBA] FAILED - Failed at test #7", 0).status_code == 7)


# --- cpu-trace partial credit ---
import tempfile, os  # noqa: E402
golden = "C000  4C F5 C5  JMP $C5F5\nC5F5  A2 00  LDX #$00\nC5F7  86 00  STX $00\nFFFF END"
emitted = "C000  4C F5 C5  JMP $C5F5\nC5F5  A2 00  LDX #$00\nDEAD bad line\nFFFF END"
with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as f:
    f.write(golden)
    gpath = f.name
tr = detect_cpu_trace(emitted, Path(gpath))
os.unlink(gpath)
check("trace diverges line 3", tr.status == TestStatus.FAIL and tr.status_code == 3)
check("trace partial progress ~0.5", 0.4 < tr.progress < 0.6)


# --- scoring: rigor weighting ---
# A passing cycle-accurate critical test must outweigh a passing functional low test.
cyc = score_test(test_id="a", subsystem="cpu", accuracy_type=AccuracyType.CYCLE_ACCURATE,
                 priority=Priority.CRITICAL, status=TestStatus.PASS)
fun = score_test(test_id="b", subsystem="cpu", accuracy_type=AccuracyType.FUNCTIONAL,
                 priority=Priority.LOW, status=TestStatus.PASS)
check("cycle-acc weight > functional weight", cyc.weight > fun.weight * 5)

# Known fail excluded from denominator: all-known-fail subsystem -> 0 scored.
kf = score_test(test_id="c", subsystem="apu", accuracy_type=AccuracyType.TIMING,
                priority=Priority.HIGH, status=TestStatus.KNOWN_FAIL)
check("known_fail not scored", kf.scored is False)

# RUNS contributes nothing and is not scored.
runs = score_test(test_id="d", subsystem="ppu", accuracy_type=AccuracyType.VISUAL,
                  priority=Priority.HIGH, status=TestStatus.RUNS)
check("runs not scored", runs.scored is False)

# Console roll-up: one perfect cpu test + one failed apu test.
fail_apu = score_test(test_id="e", subsystem="apu", accuracy_type=AccuracyType.FUNCTIONAL,
                      priority=Priority.MEDIUM, status=TestStatus.FAIL)
card = score_console("nes", [cyc, fail_apu])
# cpu subsystem 100%, apu 0%; importance cpu 1.0 apu 0.55 -> 1.0/(1.55) ~ 0.645
check("console rollup weights importance", 0.60 < card.overall < 0.69)
check("uncovered subsystems flagged", "ppu" in card.uncovered_subsystems)

print(f"\n{'ALL PASS' if failures == 0 else str(failures) + ' FAILURES'}")
sys.exit(1 if failures else 0)
