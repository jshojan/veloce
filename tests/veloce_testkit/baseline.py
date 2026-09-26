"""
Per-test / per-check comparison of two runner --json documents.

Used by tests/run_all.py --baseline and runnable on its own:

    python -m veloce_testkit.baseline base.json current.json [--fail-on-regression]

Both inputs may be a single-console scorecard (runner.py --json) or the
aggregate {"consoles": {...}} document from run_all.py --json.

Reported:
  * status changes per test (pass -> fail is a regression),
  * per-check status / exp / got changes (from VELOCE-RESULT/1 CHECK lines),
  * frames_used drift on tests that pass in both runs: the verdict is the same
    but the ROM needed a different number of frames to reach END, a cheap early
    signal of a CPU/PPU cycle-timing change.
"""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path

PASSING = {"pass"}
HARD_FAIL = {"fail", "timeout", "error"}


@dataclass
class ConsoleDiff:
    console: str
    regressions: list[str] = field(default_factory=list)   # pass -> hard fail
    status_changes: list[str] = field(default_factory=list)
    check_changes: list[str] = field(default_factory=list)
    frame_drift: list[str] = field(default_factory=list)
    added: list[str] = field(default_factory=list)
    removed: list[str] = field(default_factory=list)

    def is_empty(self) -> bool:
        return not (self.regressions or self.status_changes or self.check_changes
                    or self.frame_drift or self.added or self.removed)

    def lines(self) -> list[str]:
        out = []
        for label, items in (("REGRESSION", self.regressions), ("status", self.status_changes),
                             ("check", self.check_changes), ("frames drift", self.frame_drift),
                             ("added", self.added), ("removed", self.removed)):
            out.extend(f"[{self.console}] {label}: {i}" for i in items)
        return out


def console_cards(doc: dict) -> dict[str, dict]:
    """Normalise a runner or run_all document to {console: scorecard}."""
    if "consoles" in doc and isinstance(doc["consoles"], dict):
        return doc["consoles"]
    if "console" in doc:
        return {doc["console"]: doc}
    return {}


def _check_key(c: dict) -> str:
    return f"{c.get('id', '')} {c.get('name', '')}".strip()


def diff_console(console: str, base: dict, cur: dict) -> ConsoleDiff:
    d = ConsoleDiff(console)
    b = {r["id"]: r for r in base.get("results", [])}
    c = {r["id"]: r for r in cur.get("results", [])}
    d.added = sorted(set(c) - set(b))
    d.removed = sorted(set(b) - set(c))
    for tid in sorted(set(b) & set(c)):
        rb, rc = b[tid], c[tid]
        sb, sc = rb.get("status"), rc.get("status")
        if sb != sc:
            msg = f"{tid}: {sb} -> {sc} ({rc.get('detail', '')})"
            if sb in PASSING and sc in HARD_FAIL:
                d.regressions.append(msg)
            else:
                d.status_changes.append(msg)
        cb = {_check_key(x): x for x in rb.get("checks", []) or []}
        cc = {_check_key(x): x for x in rc.get("checks", []) or []}
        for k in sorted(set(cb) | set(cc)):
            xb, xc = cb.get(k), cc.get(k)
            if xb is None:
                d.check_changes.append(f"{tid} CHECK {k}: new ({xc.get('status')})")
            elif xc is None:
                d.check_changes.append(f"{tid} CHECK {k}: gone (was {xb.get('status')})")
            elif (xb.get("status"), xb.get("got")) != (xc.get("status"), xc.get("got")):
                d.check_changes.append(
                    f"{tid} CHECK {k}: {xb.get('status')} got={xb.get('got', '')} -> "
                    f"{xc.get('status')} got={xc.get('got', '')} exp={xc.get('exp', '')}")
        fb, fc = rb.get("frames_used", 0) or 0, rc.get("frames_used", 0) or 0
        if sb in PASSING and sc in PASSING and fb and fc and fb != fc:
            d.frame_drift.append(f"{tid}: frames_used {fb} -> {fc}")
    return d


def diff_documents(base_doc: dict, cur_doc: dict) -> list[ConsoleDiff]:
    base, cur = console_cards(base_doc), console_cards(cur_doc)
    return [diff_console(con, base[con], cur[con]) for con in sorted(set(base) & set(cur))]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Diff two Veloce runner --json documents")
    ap.add_argument("baseline")
    ap.add_argument("current")
    ap.add_argument("--fail-on-regression", action="store_true",
                    help="exit 1 if a baseline PASS became FAIL/TIMEOUT/ERROR")
    args = ap.parse_args(argv)
    diffs = diff_documents(json.loads(Path(args.baseline).read_text()),
                           json.loads(Path(args.current).read_text()))
    regress = False
    for d in diffs:
        if d.is_empty():
            print(f"[{d.console}] identical per-test statuses and checks")
        for line in d.lines():
            print(line)
        regress |= bool(d.regressions)
    return 1 if (regress and args.fail_on_regression) else 0


if __name__ == "__main__":
    sys.exit(main())
