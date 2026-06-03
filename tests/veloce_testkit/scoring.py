"""
Veloce completeness / accuracy scoring methodology.

GOAL
----
Produce a single, *defensible* "% accurate" number per subsystem and per console
that the project can stand behind publicly ("the SNES core is 87% accurate") and
point a skeptic at this file to justify.

DESIGN PRINCIPLES
-----------------
1. Every claim is reproducible from data: each test contributes a (weight, credit)
   pair; the score is a weighted average. No hand-waving.
2. Rigor matters. A cycle-accurate timing test that passes is worth more evidence
   of accuracy than a functional smoke test, so it carries more weight. This means
   a core cannot inflate its score by piling on easy functional ROMs.
3. Importance matters. A broken CPU is worse than a broken obscure mapper, so
   subsystems are weighted by how central they are to running real software.
4. Honesty about the unknown. A test that merely "RUNS" (no pass/fail signal) is
   NOT counted as a pass. It contributes ZERO credit but ALSO does not count
   against the score in the headline number; it is surfaced separately as
   "unverified" so coverage is transparent. "Not implemented / crash" is a hard 0.
5. Known failures (documented hardware quirks the core deliberately doesn't chase,
   or accepted WIP) are excluded from the *headline accuracy* denominator but are
   reported as a separate "known-issue" count so the number is not gamed by
   relabelling real failures as "known".

==========================================================================
THE FORMULA
==========================================================================
For each test t with status s:

  rigor_weight(t)   = ACCURACY_WEIGHTS[t.accuracy_type] * PRIORITY_WEIGHTS[t.priority]
  credit(t)         = CREDIT[s]          # 1.0 pass, partial for trace, 0 otherwise

A test is "scored" (enters the denominator) unless its status is one of the
EXCLUDED states (KNOWN_FAIL, SKIP, RUNS). Those are reported separately.

  subsystem_score   = sum(rigor_weight * credit  over scored tests in subsystem)
                      / sum(rigor_weight          over scored tests in subsystem)

  console_score     = sum( SUBSYSTEM_WEIGHTS[sub] * subsystem_score[sub]
                           for subsystems that have >=1 scored test )
                      / sum( SUBSYSTEM_WEIGHTS[sub] for those subsystems )

Subsystems with no scored tests are dropped from the console roll-up AND flagged
as "uncovered" so a high score on a thin suite is visibly caveated.

==========================================================================
WEIGHTS (tunable, but changing them is a deliberate, reviewed act)
==========================================================================
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable

from .schema import AccuracyType, Priority
from .detect import TestStatus


# Subsystem importance: how central to running/cycle-accurately-running software.
# Same key space across consoles; a console simply won't have some of these.
SUBSYSTEM_WEIGHTS: dict[str, float] = {
    "cpu": 1.00,        # nothing runs without the CPU
    "ppu": 0.90,        # graphics; near-universal need, sprite0/timing drives games
    "timing": 0.85,     # interrupts/DMA/HDMA/timers - drives raster effects & TAS
    "apu": 0.55,        # audio; important but rarely blocks a game from running
    "memory": 0.70,     # mirroring / open-bus / access timing
    "mapper": 0.65,     # cart hardware (MMC3 etc) - critical for the games using it
    "misc": 0.30,       # uncategorized / controller / smoke
}

# Rigor: how strong a signal a passing test of this type is.
ACCURACY_WEIGHTS: dict[AccuracyType, float] = {
    AccuracyType.FUNCTIONAL: 1.0,
    AccuracyType.TIMING: 2.0,
    AccuracyType.VISUAL: 2.5,         # pixel-perfect rendering is demanding evidence
    AccuracyType.CYCLE_ACCURATE: 3.0,  # strongest evidence of accuracy
}

# Priority multiplies rigor: a critical CPU instr test outweighs a low-pri quirk.
PRIORITY_WEIGHTS: dict[Priority, float] = {
    Priority.CRITICAL: 2.0,
    Priority.HIGH: 1.5,
    Priority.MEDIUM: 1.0,
    Priority.LOW: 0.5,
}

# Credit awarded per status. cpu-trace passes through its own partial fraction
# via score_test(); these are the discrete defaults.
CREDIT: dict[TestStatus, float] = {
    TestStatus.PASS: 1.0,
    TestStatus.FAIL: 0.0,
    TestStatus.TIMEOUT: 0.0,
    TestStatus.ERROR: 0.0,
}

# Statuses excluded from the headline denominator (reported separately).
EXCLUDED_FROM_SCORE = {TestStatus.KNOWN_FAIL, TestStatus.SKIP, TestStatus.RUNS}


@dataclass
class TestPoint:
    """A scored datapoint: the inputs to and outputs of score_test()."""
    id: str
    subsystem: str
    status: TestStatus
    weight: float
    credit: float
    scored: bool          # whether it enters the denominator
    detail: str = ""


@dataclass
class SubsystemScore:
    subsystem: str
    importance: float
    score: float           # 0..1 weighted accuracy
    scored_count: int
    passed: int
    failed: int
    known_fail: int
    unverified: int        # RUNS
    skipped: int
    weight_total: float


@dataclass
class Scorecard:
    console: str
    overall: float                       # 0..1
    subsystems: list[SubsystemScore]
    total_tests: int
    total_passed: int
    total_failed: int
    total_known_fail: int
    total_unverified: int
    total_skipped: int
    uncovered_subsystems: list[str] = field(default_factory=list)

    @property
    def overall_pct(self) -> float:
        return round(self.overall * 100, 1)


def rigor_weight(accuracy_type: AccuracyType, priority: Priority) -> float:
    return ACCURACY_WEIGHTS[accuracy_type] * PRIORITY_WEIGHTS[priority]


def score_test(
    *,
    test_id: str,
    subsystem: str,
    accuracy_type: AccuracyType,
    priority: Priority,
    status: TestStatus,
    progress: float = 0.0,
    detail: str = "",
) -> TestPoint:
    """Turn one detection verdict into a scored TestPoint.

    `progress` (0..1) supplies partial credit for cpu-trace results: a trace that
    matches 90% of golden lines before diverging earns 0.9 * rigor weight, which
    rewards "almost cycle-accurate" cores without claiming a pass.
    """
    w = rigor_weight(accuracy_type, priority)
    scored = status not in EXCLUDED_FROM_SCORE

    if status == TestStatus.PASS:
        credit = 1.0
    elif status == TestStatus.FAIL and progress > 0.0:
        credit = max(0.0, min(progress, 0.999))  # partial credit, never a full pass
    else:
        credit = CREDIT.get(status, 0.0)

    return TestPoint(
        id=test_id,
        subsystem=subsystem,
        status=status,
        weight=w,
        credit=credit,
        scored=scored,
        detail=detail,
    )


def score_suite(points: Iterable[TestPoint]) -> dict[str, SubsystemScore]:
    """Aggregate TestPoints into per-subsystem SubsystemScores."""
    by_sub: dict[str, list[TestPoint]] = {}
    for p in points:
        by_sub.setdefault(p.subsystem, []).append(p)

    out: dict[str, SubsystemScore] = {}
    for sub, pts in by_sub.items():
        scored = [p for p in pts if p.scored]
        wtot = sum(p.weight for p in scored)
        earned = sum(p.weight * p.credit for p in scored)
        score = (earned / wtot) if wtot > 0 else 0.0
        out[sub] = SubsystemScore(
            subsystem=sub,
            importance=SUBSYSTEM_WEIGHTS.get(sub, SUBSYSTEM_WEIGHTS["misc"]),
            score=score,
            scored_count=len(scored),
            passed=sum(1 for p in pts if p.status == TestStatus.PASS),
            failed=sum(1 for p in pts if p.status in (TestStatus.FAIL, TestStatus.TIMEOUT, TestStatus.ERROR)),
            known_fail=sum(1 for p in pts if p.status == TestStatus.KNOWN_FAIL),
            unverified=sum(1 for p in pts if p.status == TestStatus.RUNS),
            skipped=sum(1 for p in pts if p.status == TestStatus.SKIP),
            weight_total=wtot,
        )
    return out


def score_console(console: str, points: Iterable[TestPoint]) -> Scorecard:
    points = list(points)
    subs = score_suite(points)

    # Console roll-up over subsystems that actually have scored tests.
    covered = {s: v for s, v in subs.items() if v.scored_count > 0}
    imp_total = sum(v.importance for v in covered.values())
    overall = (
        sum(v.importance * v.score for v in covered.values()) / imp_total
        if imp_total > 0 else 0.0
    )

    uncovered = sorted(
        s for s in SUBSYSTEM_WEIGHTS
        if s != "misc" and s not in covered
    )

    return Scorecard(
        console=console,
        overall=overall,
        subsystems=sorted(subs.values(), key=lambda x: -x.importance),
        total_tests=len(points),
        total_passed=sum(1 for p in points if p.status == TestStatus.PASS),
        total_failed=sum(1 for p in points if p.status in (TestStatus.FAIL, TestStatus.TIMEOUT, TestStatus.ERROR)),
        total_known_fail=sum(1 for p in points if p.status == TestStatus.KNOWN_FAIL),
        total_unverified=sum(1 for p in points if p.status == TestStatus.RUNS),
        total_skipped=sum(1 for p in points if p.status == TestStatus.SKIP),
        uncovered_subsystems=uncovered,
    )


# --------------------------------------------------------------------------
# Presentation
# --------------------------------------------------------------------------
def render_scorecard(card: Scorecard, *, color: bool = True) -> str:
    """Human-readable scorecard table."""
    def c(code: str, s: str) -> str:
        return f"{code}{s}\033[0m" if color else s

    GREEN, YELLOW, RED, BOLD, BLUE = "\033[32m", "\033[33m", "\033[31m", "\033[1m", "\033[34m"

    def band(pct: float) -> str:
        return GREEN if pct >= 85 else (YELLOW if pct >= 60 else RED)

    lines = []
    lines.append(c(BLUE, "=" * 72))
    lines.append(c(BOLD, f"  VELOCE ACCURACY SCORECARD - {card.console.upper()}"))
    lines.append(c(BLUE, "=" * 72))
    lines.append(
        f"  {'Subsystem':<12}{'Importance':>11}{'Score':>9}"
        f"{'Pass':>6}{'Fail':>6}{'Known':>7}{'Unver':>7}"
    )
    lines.append("  " + "-" * 68)
    for s in card.subsystems:
        pct = s.score * 100
        lines.append(
            f"  {s.subsystem:<12}{s.importance:>11.2f}"
            f"{c(band(pct), f'{pct:>7.1f}%')}"
            f"{s.passed:>6}{s.failed:>6}{s.known_fail:>7}{s.unverified:>7}"
        )
    lines.append("  " + "-" * 68)
    opct = card.overall_pct
    lines.append("  " + c(BOLD, f"OVERALL ACCURACY: ") + c(band(opct), c(BOLD, f"{opct:.1f}%")))
    lines.append(
        f"  tests={card.total_tests}  passed={card.total_passed}  "
        f"failed={card.total_failed}  known={card.total_known_fail}  "
        f"unverified={card.total_unverified}  skipped={card.total_skipped}"
    )
    if card.uncovered_subsystems:
        lines.append(c(YELLOW, f"  uncovered subsystems (no tests): "
                              f"{', '.join(card.uncovered_subsystems)}"))
    lines.append(c(BLUE, "=" * 72))
    return "\n".join(lines)


def scorecard_to_dict(card: Scorecard) -> dict:
    return {
        "console": card.console,
        "overall_accuracy_pct": card.overall_pct,
        "totals": {
            "tests": card.total_tests,
            "passed": card.total_passed,
            "failed": card.total_failed,
            "known_fail": card.total_known_fail,
            "unverified": card.total_unverified,
            "skipped": card.total_skipped,
        },
        "uncovered_subsystems": card.uncovered_subsystems,
        "subsystems": [
            {
                "subsystem": s.subsystem,
                "importance": s.importance,
                "score_pct": round(s.score * 100, 1),
                "scored": s.scored_count,
                "passed": s.passed,
                "failed": s.failed,
                "known_fail": s.known_fail,
                "unverified": s.unverified,
                "skipped": s.skipped,
            }
            for s in card.subsystems
        ],
    }
