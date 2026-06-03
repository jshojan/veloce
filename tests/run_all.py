#!/usr/bin/env python3
"""
Root orchestrator: run every console's test suite and aggregate a combined,
defensible accuracy scorecard for the whole Veloce platform.

It shells out to each cores/<c>/tests/run_tests.sh --json (the per-console
runners remain the source of truth for downloading ROMs and producing results),
parses the standardized JSON, and prints a cross-console scorecard. It can also
gate CI: --min-overall / --no-regressions.

Usage:
  python tests/run_all.py                 # run all cores, print scorecard
  python tests/run_all.py nes snes        # subset
  python tests/run_all.py --json          # machine-readable aggregate
  python tests/run_all.py --baseline scorecard.json --no-regressions
  python tests/run_all.py --min-overall 80
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORES = ["nes", "snes", "gb", "gba"]


def run_console(console: str) -> dict | None:
    runner = ROOT / "cores" / console / "tests" / "run_tests.sh"
    if not runner.exists():
        print(f"[warn] no runner for {console} ({runner})", file=sys.stderr)
        return None
    proc = subprocess.run(
        ["bash", str(runner), "--json"],
        capture_output=True, text=True, cwd=str(ROOT),
    )
    # Per-console runners emit the scorecard JSON as the last JSON object on stdout.
    out = proc.stdout.strip()
    if not out:
        print(f"[warn] {console} produced no JSON\n{proc.stderr[-2000:]}", file=sys.stderr)
        return None
    try:
        return _last_json_object(out)
    except Exception as e:  # noqa: BLE001
        print(f"[warn] {console} JSON parse failed: {e}", file=sys.stderr)
        return None


def _last_json_object(text: str) -> dict:
    # Tolerate human log lines before the JSON: find the last top-level {...}.
    depth = 0
    start = None
    last = None
    for i, ch in enumerate(text):
        if ch == "{":
            if depth == 0:
                start = i
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0 and start is not None:
                last = text[start:i + 1]
    if last is None:
        raise ValueError("no JSON object found")
    return json.loads(last)


def render(cards: dict[str, dict]) -> str:
    GREEN, YELLOW, RED, BOLD, BLUE, NC = (
        "\033[32m", "\033[33m", "\033[31m", "\033[1m", "\033[34m", "\033[0m")

    def band(p):
        return GREEN if p >= 85 else (YELLOW if p >= 60 else RED)

    lines = [f"{BLUE}{'=' * 60}{NC}",
             f"{BOLD}  VELOCE PLATFORM ACCURACY SCORECARD{NC}",
             f"{BLUE}{'=' * 60}{NC}",
             f"  {'Console':<10}{'Accuracy':>10}{'Pass':>7}{'Fail':>7}{'Known':>7}{'Unver':>7}"]
    weighted_sum = 0.0
    weight_total = 0.0
    for con in CORES:
        card = cards.get(con)
        if not card:
            lines.append(f"  {con:<10}{'  n/a':>10}")
            continue
        pct = card.get("overall_accuracy_pct", 0.0)
        t = card.get("totals", {})
        lines.append(
            f"  {con:<10}{band(pct)}{pct:>9.1f}%{NC}"
            f"{t.get('passed', 0):>7}{t.get('failed', 0):>7}"
            f"{t.get('known_fail', 0):>7}{t.get('unverified', 0):>7}")
        # Platform roll-up weights each console equally (they are separate products).
        weighted_sum += pct
        weight_total += 1
    lines.append("  " + "-" * 54)
    platform = (weighted_sum / weight_total) if weight_total else 0.0
    lines.append(f"  {BOLD}PLATFORM MEAN: {band(platform)}{platform:.1f}%{NC}")
    lines.append(f"{BLUE}{'=' * 60}{NC}")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description="Veloce cross-console test orchestrator")
    ap.add_argument("consoles", nargs="*", help="subset of: " + ", ".join(CORES))
    ap.add_argument("--json", action="store_true", help="emit aggregate JSON")
    ap.add_argument("--min-overall", type=float, default=None,
                    help="fail (exit 1) if any console's accuracy is below this %%")
    ap.add_argument("--baseline", type=str, default=None,
                    help="path to a prior aggregate JSON for regression gating")
    ap.add_argument("--no-regressions", action="store_true",
                    help="fail if any console drops >0.1%% vs --baseline")
    args = ap.parse_args()

    targets = args.consoles or CORES
    cards: dict[str, dict] = {}
    for con in targets:
        card = run_console(con)
        if card:
            cards[con] = card

    aggregate = {
        "consoles": cards,
        "platform_mean_pct": round(
            sum(c.get("overall_accuracy_pct", 0) for c in cards.values()) / len(cards), 1
        ) if cards else 0.0,
    }

    if args.json:
        print(json.dumps(aggregate, indent=2))
    else:
        print(render(cards))

    exit_code = 0
    if args.min_overall is not None:
        for con, card in cards.items():
            if card.get("overall_accuracy_pct", 0) < args.min_overall:
                print(f"[gate] {con} below --min-overall {args.min_overall}", file=sys.stderr)
                exit_code = 1

    if args.no_regressions and args.baseline:
        base = json.loads(Path(args.baseline).read_text()).get("consoles", {})
        for con, card in cards.items():
            prev = base.get(con, {}).get("overall_accuracy_pct")
            cur = card.get("overall_accuracy_pct", 0)
            if prev is not None and cur < prev - 0.1:
                print(f"[gate] regression in {con}: {prev}% -> {cur}%", file=sys.stderr)
                exit_code = 1
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
