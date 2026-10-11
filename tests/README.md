# Veloce Unified Test & Accuracy Framework

This directory is the **shared, cross-console** test + accuracy-measurement
framework. The NES, SNES, GB, GBA and experimental N64 cores build on it. It is
the single source of truth for the config schema, result-detection protocols,
and the completeness-scoring methodology.

Repository-owned ROMs live in `roms-src/gb/register_probe/`, `roms-src/gb/alu_exhaustive/`, `roms-src/gb/unary_exhaustive/`, and
`roms-src/nes/alu_exhaustive/`, `roms-src/nes/netplay_probe/`,
`roms-src/snes/math_registers/`, `roms-src/snes/math_timing/`, and
`roms-src/gba/arm_shifts/` and `roms-src/n64/integer_registers/`, `roms-src/n64/muldiv64/`, `roms-src/n64/merge_memory/`. Verify their binaries and source ledger with
`python3 tests/roms-src/build.py --original-only --check`; execute them with
`ctest --test-dir build -L owned --output-on-failure`. The NES, GB ALU/unary, SNES, GBA and N64 runners grade raw
register observations using independent host oracles. The GB probe uses the
shared file detector on ROM-emitted checks. All include failure controls and
retain register files under `build/artifacts/owned/`. Their exact domain coverage
is documented in [HARDWARE-COVERAGE.md](../docs/HARDWARE-COVERAGE.md).

The separate C++ `netplay_rollback_test` runs the controller probe through the
actual NES plugin and checks two delayed sessions against an ordinary 100-frame
baseline, including full serialized state equality. It records guest registers
and states under `build/artifacts/rollback/`. This measures execution/replay
behavior. `netplay_network_test` runs two independent NES processes over
loopback TCP, verifies identical complete states, rejects ROM mismatch and four
fragmented malformed-message cases, and saves observations/reports/logs under
`build/artifacts/network/`. Build `netplay_network_peer`; local socket permission
is required. `netplay_plugin_test` repeats the network checks using the production
netplay shared plugin and the frontend's owned-frame/audio contract, adding
core/frame mutation checks. Build `netplay_plugin_peer`; reports and states are
retained under `build/artifacts/netplay-plugin/`. The application wires this
contract into its frame loop; rendered GUI and live two-machine verification
remain pending.

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
  validate_configs.py    Local schema validation of every test_config.json
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
| `result_detection` | `file` `cpu-trace` `screenshot-crc` (`memory` `serial`: deprecated aliases of `file`) | how pass/fail is read |
| `expected` | `pass` `known_fail` `<int>` `<crc hex>` | expected verdict |
| `priority` | `critical` `high` `medium` `low` | importance multiplier |
| `source_url`, `license` | string | provenance + license of the ROM |
| extras | `frames`, `screenshot_frame`, `reference_hash`, `trace_log`, `trace_limit` | detection-specific |
| `file` extras | `channel`, `require_channel`, `rom_variant`, `expected_checks`, `allow_empty`, `resets`, `input` | see [VELOCE-RESULT.md](../docs/testing/VELOCE-RESULT.md#8-testkit-and-config) |

Suites carry defaults (`subsystem`, `priority`, `repo`) that tests inherit.
Validate any config with `python tests/validate_configs.py <console>`.

---

## 2. Result-detection conventions

Implemented in `detect.py`; all return a `DetectionResult(status, detail,
status_code, progress)` (plus `checks`, `end_reason`, `frames_used`, `adapter`
for the file method).

* **`file` (all consoles).** The harness always sets `VELOCE_TEST_OUT`; the
  application writes the ROM's VELOCE-RESULT/1 stream (`CHECK`/`END` lines) to
  that file and `detect_result_file()` scores it with per-check detail. Spec:
  [docs/testing/VELOCE-RESULT.md](../docs/testing/VELOCE-RESULT.md). `memory` and
  `serial` below are deprecated aliases: the file wins once the core emits to it;
  until then the stdout parsers are the fallback (the only case where the
  harness still sets `DEBUG=1`).

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
   (`scorecard_to_dict`) so `tests/run_all.py` can aggregate it locally.

Fast sanity check for the framework itself: `python tests/veloce_testkit/selftest.py`.

## N64 copy-cycle and private controller diagnostics

The original [copy rectangle cartridge](roms-src/n64/rdp_copy_rectangles/README.md)
grades 1,572,864 cases / 20,447,232 actual framebuffer, hidden-RAM, guard and DP/MI
register words. Its executed wrong-capture, 13 field and seven malformed-stream
controls are mandatory. Component, executed production-fault and S028 replay
gates run through `ctest -R '^n64_rdp_copy_(test|mutants|checkpoint_test)$'`.
An optional [pinned renderer comparison](reference/n64-copy/README.md) stays
outside the repo. All 116 original archives reproduce without network or SDK.

`n64_retail_probe.py --input-script tests/input/n64-title-start.txt --capture
build/artifacts/retail/n64-copy-start.ppm` accepts a private ROM positional path
and `--frames 10000`. Each input line is decimal scheduling quantum followed by
hexadecimal port-zero virtual-button mask, with optional `#` comments. Transitions
must strictly increase and fall in 0..9999; masks are raw 32-bit values without
`0x`. Start is `40`, A is `1`, and a mask is retained until the next transition.
Both production probe and CPU trace read the same file; the report records its
SHA256. The trace uses 100,000 nominal CPU attempts per quantum. This is a bounded
digital diagnostic, with no analog, physical timing or playability verdict.
Capture metadata is recorded only when the current probe writes a capture.
Private bytes/captures stay in ignored build artifacts.

## N64 cached controller reads and S029

The [original cached-read cartridge](roms-src/n64/controller_cached_reads/README.md)
formats once and observes both reads across every digital combination/complement:
16,384 cases / 262,144 actual SI/RAM/MI words, strict 7/9-word chunks, executed
wrong-capture and 16 field / seven stream controls. The component checks all four
active ports and packet/control behavior; production checkpoint replay changes
host/client input. `ctest -R '^n64_pif_'` exercises these gates and ten executed
production faults. S029 appends forty retained-layout bytes, for 9,527,616 total;
older states import with channels disabled until the guest formats a new packet.
See the core README for the next private Mario one-cycle E4 stop and limits.
