# Veloce Testing and Accuracy Framework

This is the entry point for everything about how Veloce's emulator cores are
tested and how the project derives a defensible "this core is N% accurate"
number. It covers the testing philosophy, the shared harness architecture, the
result-detection protocols, how to run the suites, and exactly how the
completeness percentage is computed.

For the per-console scorecard (the actual numbers and where the gaps are) see
[COMPLETENESS.md](COMPLETENESS.md). For the framework's internal API and the
schema reference, see [tests/README.md](tests/README.md), which is the source of
truth this document summarizes rather than duplicates.

## Testing philosophy

Veloce targets accuracy where it matters for speedrunning and TAS, so the test
suite is built to *measure and publish* that accuracy honestly rather than to
produce a flattering headline. Three rules drive everything:

1. **Every claim is reproducible from data.** Each test contributes a
   `(weight, credit)` pair and the score is a weighted average. There is no
   hand-tuned "looks about 90%" number anywhere; if you disagree with a score
   you can point at the exact tests that produced it.
2. **Rigor and importance are weighted, so the number cannot be inflated.** A
   passing cycle-accurate timing test counts roughly twelve times as much as a
   passing low-priority functional smoke test, and a broken CPU costs far more
   than a broken obscure mapper. Piling on easy ROMs does not move the headline.
3. **The unknown is reported, not hidden.** A test that runs but produces no
   pass/fail signal earns zero credit and is *excluded from the denominator*,
   surfaced separately as "unverified". A documented hardware quirk the core
   deliberately does not chase is counted in its own column, never laundered
   into a pass. The published number reflects only what was actually verified.

A direct consequence: some cores publish a *lower* number than their previous
hand-written README claimed, because the previous claim counted tests that were
never actually detected. That is the framework working as intended.

## Harness architecture

All four console runners build on one shared, importable Python package,
`tests/veloce_testkit/`, so detection and scoring are identical platform-wide. A
console agent owns only its `cores/<c>/tests/test_config.json` and a thin runner
shim; it never edits the testkit or the emulator source.

```
tests/
  veloce_testkit/        shared framework (single source of truth)
    schema.py            test_config v2 loader/validator (backward-compatible)
    detect.py            the result-detection protocols (uniform DetectionResult)
    scoring.py           the completeness-scoring methodology
    harness.py           drives the headless `veloce` binary, applies detection
    runner.py            reference per-console CLI (run_console_main)
    selftest.py          pure-logic unit tests (no ROMs, no emulator)
  run_all.py             root orchestrator -> cross-console roll-up + CI gates
  validate_configs.py    schema validation of every test_config.json
  CMakeLists.txt         CTest registration (fast gates + accuracy suites)

cores/<console>/tests/
  test_config.json       the console's owned test catalog (schema v2)
  run_tests.sh           thin shim -> the shared runner
```

The harness drives the prebuilt headless `veloce` binary using environment
variables (`HEADLESS=1`, `DEBUG=1`, `FRAMES`, `SAVE_SCREENSHOT`, `TRACE`),
applies the correct detector per test, and emits scored data points. The same
JSON scorecard shape comes out of every console, which is what lets
`tests/run_all.py` aggregate a platform roll-up and gate CI.

The per-test catalog (`test_config.json`, schema v2) tags every test with a
`subsystem` (cpu / ppu / apu / timing / memory / mapper / misc), an
`accuracy_type` (functional / timing / cycle-accurate / visual), a `priority`
(critical / high / medium / low), a `result_detection` method, an `expected`
verdict, and ROM provenance (`source_url`, `license`). The full field reference
lives in [tests/README.md](tests/README.md#1-standard-test_configjson-schema-v2).

## Result-detection protocols

A test only counts if the harness can read an unambiguous verdict from the
running emulator. There are four protocols, all implemented in `detect.py` and
returning a uniform `DetectionResult(status, detail, status_code, progress)`.

| Method | Used by | How the verdict is read |
|--------|---------|-------------------------|
| `memory` | Blargg NES / SNES ROMs | ROM writes a status byte to `$6000` (`0x00` pass, `0x01-0x7F` fail code, `0x80` running, `0x81` needs-reset) with signature `0xDE 0xB0 0x61`. Under `DEBUG=1` the binary prints `BLARGG_STATUS: 0xNN` / `Status code: N (PASSED\|FAILED)`. |
| `serial` (GB) | Blargg GB (ASCII), Mooneye / SameSuite / Wilbertpol | Blargg GB ROMs echo `Passed`/`Failed` over the link-port serial to stdout. Mooneye-family ROMs use the Fibonacci register fingerprint (`B=3 C=5 D=8 E=13 H=21 L=34`) at an `LD B,B` breakpoint, surfaced as `MOONEYE: PASS\|FAIL`. |
| `serial` (GBA) | jsmolka / alyosha / nba ROMs | The ROM spins with `R12` holding the failing test number (`0` = all pass). The plugin detects the stable-PC spin and prints `[GBA] PASSED` / `[GBA] FAILED - Failed at test #N`. Auto-selected when `console == gba`. |
| `screenshot-crc` | Visual / pixel-perfect tests (acid2, Mealybug, krom/PeterLemon SNES, full_palette) | Run with `SAVE_SCREENSHOT=<frame>`, CRC32 the resulting PNG, and compare against a stored `reference_hash`. Exact match required. `--generate-refs` prints measured hashes to seed the config. |
| `cpu-trace` | nestest golden trace | Run with `TRACE=1`, compare each instruction line against the golden `trace_log` (whitespace-normalized). The verdict is the first divergent line; the matched-line fraction feeds **partial credit**. |

Two recurring measurement limits to be aware of (both surfaced in
[COMPLETENESS.md](COMPLETENESS.md)):

- `screenshot-crc` tests need a `reference_hash` captured on a trusted reference
  emulator or real hardware before they can score. Until then they ship as
  `known_fail` / `SKIP` with an empty hash and contribute **zero**, so the core
  is never scored against its own output.
- `cpu-trace` (nestest) and the Mooneye `serial` fingerprint each require a hook
  in the binary that some cores do not yet emit. Where that hook is missing, the
  affected tests resolve to RUNS (unverified), not pass.

## Running the suites

Every console exposes the same CLI through its `run_tests.sh` shim.

```bash
# Per console (from cores/<console>/tests/)
./run_tests.sh                 # all suites, human-readable scorecard
./run_tests.sh cpu ppu apu     # filter by subsystem key or suite id
./run_tests.sh -v              # per-test PASS / FAIL / KNOWN / RUNS lines
./run_tests.sh --json          # scorecard JSON (consumed by tests/run_all.py)
./run_tests.sh --generate-refs # print measured CRCs to seed screenshot-crc refs
```

```bash
# All cores at once, with a cross-console roll-up
python tests/run_all.py                 # run every console, print platform scorecard
python tests/run_all.py nes snes        # subset
python tests/run_all.py --json          # machine-readable aggregate
python tests/run_all.py --min-overall 80          # CI gate: fail under 80%
python tests/run_all.py --baseline scorecard.json --no-regressions
```

Each console's runner clones the public test-ROM repositories declared in its
`test_config.json` on first run; ROMs are never committed. A missing ROM reports
`SKIP` (excluded from the score) rather than a false fail.

### Continuous integration

Testing is wired in two tiers:

- **Fast gates** (`ctest --test-dir build`): the pure-logic testkit selftest and
  config validation, labelled `unit;fast`. These run on every push and PR and
  need no ROMs or built emulator.
- **Accuracy suites** (`ctest --test-dir build -L accuracy`): the per-console
  runs, labelled `accuracy;slow`, with timeouts and a `REQUIRED_FILES`
  dependency on the `veloce` binary.

The GitHub Actions workflow (`.github/workflows/accuracy.yml`) mirrors this: a
`fast-gates` job on every change, a per-console `accuracy` matrix that builds the
binary and uploads each scorecard JSON plus screenshots as artifacts, and an
`aggregate` job wired for `run_all.py --baseline --no-regressions` once a
baseline scorecard is committed.

A quick local sanity check of the framework itself:

```bash
python tests/veloce_testkit/selftest.py    # pure-logic unit tests, no ROMs
python tests/validate_configs.py           # schema-validate every config
```

## How the completeness percentage is computed

The scoring methodology lives in `tests/veloce_testkit/scoring.py`; this section
states the formula so the headline number is auditable.

**Per test.** Each test gets a rigor weight and a credit:

```
rigor_weight(t) = ACCURACY_WEIGHTS[t.accuracy_type] * PRIORITY_WEIGHTS[t.priority]
credit(t)       = 1.0 (pass) | partial fraction (cpu-trace near-miss) | 0 (fail/timeout/crash)
```

The weight tables, chosen so that a passing critical cycle-accurate test
outweighs a passing low-priority functional test by about 12x:

```
ACCURACY_WEIGHTS   functional 1.0 | timing 2.0 | visual 2.5 | cycle-accurate 3.0
PRIORITY_WEIGHTS   critical 2.0   | high 1.5   | medium 1.0  | low 0.5
SUBSYSTEM_WEIGHTS  cpu 1.00 | ppu 0.90 | timing 0.85 | memory 0.70 |
                   mapper 0.65 | apu 0.55 | misc 0.30
```

**Per subsystem.** A weighted average of credit over *scored* tests only:

```
subsystem_score = sum(rigor_weight * credit) / sum(rigor_weight)   over scored tests
```

**Per console.** An importance-weighted average over subsystems that have at
least one scored test:

```
console_score = sum(SUBSYSTEM_WEIGHTS[s] * subsystem_score[s])
              / sum(SUBSYSTEM_WEIGHTS[s])      over covered subsystems
```

**What counts, and what does not.** This is what makes the number defensible:

| Status | Meaning | Effect on the headline score |
|--------|---------|------------------------------|
| `PASS` | Verified correct | Full credit, in the denominator |
| `FAIL` / `TIMEOUT` / `ERROR` | Wrong or crashed; equivalent to not-implemented | Zero credit, in the denominator (counts against the score) |
| cpu-trace near-miss | Matched a fraction of the golden trace then diverged | Partial credit (capped below a pass), in the denominator |
| `RUNS` | Ran but emitted no pass/fail signal (unverified-but-implemented) | Zero credit, **excluded** from the denominator, reported separately |
| `SKIP` | ROM or prerequisite missing | Excluded and reported |
| `known_fail` | Documented hardware quirk or accepted WIP | Excluded from the headline denominator, counted in its own column |

Subsystems with no scored tests are dropped from the roll-up *and* flagged as
"uncovered", so a high score built on thin coverage is visibly caveated rather
than silently inflated.

This design means the headline number answers "of what we could verify, how much
is correct, weighted by how much it matters" while the separate
unverified / known-fail / uncovered columns answer "and how much could we not yet
verify". Both halves are published. The exact rendering (per-subsystem table,
bold overall, totals, uncovered caveat) is produced by `render_scorecard()` and
mirrored as JSON by `scorecard_to_dict()`.

## Where to go next

- [COMPLETENESS.md](COMPLETENESS.md) - the per-console accuracy scorecard, every
  number justified against what is verified versus unverified.
- [tests/README.md](tests/README.md) - the framework's schema, detection, and
  scoring internals in full detail.
- Per-console testing notes: [NES](cores/nes/README.md#testing),
  [SNES](cores/snes/README.md#testing), [Game Boy](cores/gb/README.md#testing),
  [GBA](cores/gba/README.md#test-status).
