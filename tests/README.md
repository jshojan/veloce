# Veloce Unified Test & Accuracy Framework

This directory is the **shared, cross-console** test + accuracy-measurement
framework. The four per-console agents (NES, SNES, GB, GBA) build on it. It is
the single source of truth for the config schema, result-detection protocols,
and the completeness-scoring methodology.

> **Ownership rule.** Console agents own ONLY `cores/<c>/tests/test_config.json`
> (and a ~10-line runner shim). They do **not** edit `veloce_testkit/` or the
> emulator source. Schema / detection / scoring changes are reviewed here.

```
tests/
  veloce_testkit/        shared importable package (the framework)
    schema.py            test_config v2 schema + loader/validator
    detect.py            the 5 result-detection protocols
    scoring.py           completeness-scoring methodology (the headline number)
    harness.py           drives the headless `veloce` binary, applies detection
    runner.py            reference per-console CLI (run_console_main)
    selftest.py          pure-logic unit test (no ROMs/emulator)
  run_all.py             root orchestrator: all cores -> aggregate scorecard
  validate_configs.py    CI schema validation of every test_config.json
  CMakeLists.txt         CTest registration (fast gates + accuracy suites)
```

---

## 1. Standard `test_config.json` schema (v2)

Every console config conforms to `veloce_testkit/schema.py`. It is
**backward-compatible**: legacy keys (`path`, `expected`, `test_type`,
`screenshot_frame`, `reference_hash`, `notes`) still load. New required-going-
forward keys are inferred when missing (see the loader), but agents SHOULD set
them explicitly.

Per-test fields (`TestSpec`):

| field | values | meaning |
|-------|--------|---------|
| `id` | string | stable unique id (defaults to `<suite>.<file-stem>`) |
| `file` (alias `path`) | string | ROM path under the suite's repo dir |
| `subsystem` | `cpu` `ppu` `apu` `timing` `memory` `mapper` `misc` | scoring bucket |
| `accuracy_type` | `functional` `timing` `cycle-accurate` `visual` | rigor of the test |
| `result_detection` | `memory` `serial` `screenshot-crc` `cpu-trace` | how pass/fail is read |
| `expected` | `pass` `known_fail` `<int>` `<crc hex>` | expected verdict |
| `priority` | `critical` `high` `medium` `low` | importance multiplier |
| `source_url`, `license` | string | provenance + license of the ROM |
| extras | `frames`, `screenshot_frame`, `reference_hash`, `trace_log`, `trace_limit` | detection-specific |

Suites carry defaults (`subsystem`, `priority`, `repo`) that tests inherit.
Validate any config with `python tests/validate_configs.py <console>`.

---

## 2. Result-detection conventions

Implemented in `detect.py`; all return a `DetectionResult(status, detail,
status_code, progress)`.

* **`memory` (Blargg, NES/SNES).** ROM writes status to `$6000` (`0x00`=pass,
  `0x01-0x7F`=fail code, `0x80`=running, `0x81`=needs reset), signature
  `0xDE 0xB0 0x61` at `$6001-3`, text at `$6004+`. Binary (with `DEBUG=1`) prints
  `BLARGG_STATUS: 0xNN` / `Status code: N (PASSED|FAILED)`.
* **`serial` (Game Boy).** Blargg GB ROMs echo `Passed`/`Failed` over link-port
  serial -> stdout. Mooneye ROMs use the Fibonacci register fingerprint + `LD B,B`
  breakpoint; binary prints `MOONEYE: PASS|FAIL`.
* **`serial` (GBA sub-variant).** jsmolka/alyosha ROMs hold the failing test # in
  R12 (0 = all pass); binary prints `[GBA] PASSED` / `[GBA] FAILED - Failed at
  test #N`. Selected automatically when `console == gba`.
* **`screenshot-crc` (visual).** Run with `SAVE_SCREENSHOT=<frame>`; CRC32 the PNG
  and compare to `reference_hash`. Exact match required (intentional for
  pixel-perfect tests). `--generate-refs` prints measured hashes to paste back.
  Regenerate refs if output resolution or PNG encoder changes.
* **`cpu-trace` (nestest).** Run with `TRACE=1`; compare each instruction line to
  the golden `trace_log` (whitespace-normalized). Verdict = first divergent line;
  `progress` = fraction of matching lines, which feeds **partial credit**.

---

## 3. Completeness-scoring methodology (the headline number)

Defined in `scoring.py`. The goal: a defensible "**the X core is N% accurate**"
the project can publicly stand behind and point a skeptic at.

**Per test:** `rigor_weight = ACCURACY_WEIGHTS[type] * PRIORITY_WEIGHTS[priority]`
and `credit` (1.0 pass; partial for cpu-trace; 0 otherwise).

```
ACCURACY_WEIGHTS  functional 1.0 | timing 2.0 | visual 2.5 | cycle-accurate 3.0
PRIORITY_WEIGHTS  critical 2.0 | high 1.5 | medium 1.0 | low 0.5
SUBSYSTEM_WEIGHTS cpu 1.00 | ppu 0.90 | timing 0.85 | memory 0.70 |
                  mapper 0.65 | apu 0.55 | misc 0.30
```

**Per subsystem:** weighted average of credit over *scored* tests:
`Σ(rigor·credit) / Σ(rigor)`.

**Per console:** importance-weighted average over subsystems that have ≥1 scored
test: `Σ(importance·subscore) / Σ(importance)`.

**Honesty rules that make it defensible:**

* A passing **cycle-accurate/timing** test counts far more than a functional
  smoke test, so the score can't be inflated with easy ROMs.
* **`RUNS`** (ran, no pass/fail signal) = *unverified-but-implemented*: **0 credit
  and excluded from the denominator**; surfaced separately so coverage gaps are
  visible rather than silently counted as passes.
* **`SKIP`** (missing ROM/prereq) is likewise excluded and reported.
* **`known_fail`** (documented HW quirk / accepted WIP) is excluded from the
  headline denominator but counted in a separate column — so failures can't be
  laundered into a higher score by relabelling them.
* **Hard fails / timeouts / crashes** = `not-implemented`-equivalent: credit 0,
  fully in the denominator.
* Subsystems with **no scored tests** are dropped from the roll-up *and* listed
  as `uncovered`, caveating a high score built on thin coverage.

**Scorecard presentation** (`render_scorecard`): per-subsystem table (importance,
score%, pass/fail/known/unverified) + bold **OVERALL ACCURACY** + totals +
uncovered-subsystem caveat. `tests/run_all.py` renders the cross-console roll-up.

---

## 4. How a console agent plugs in

1. Migrate `test_config.json` to v2 (add `subsystem`, `accuracy_type`,
   `result_detection`, `priority`, `source_url`, `license`). Run
   `python tests/validate_configs.py <console>`.
2. Either keep your runner and `import veloce_testkit.{detect,scoring}` for
   consistent verdicts/scores, or replace it with a shim calling
   `veloce_testkit.runner.run_console_main(<console>, Path(__file__).parent,
   rom_provider=...)`.
3. `run_tests.sh --json` must emit the scorecard JSON last on stdout
   (`scorecard_to_dict`) so `tests/run_all.py` and CI can aggregate it.

Fast sanity check for the framework itself: `python tests/veloce_testkit/selftest.py`.
