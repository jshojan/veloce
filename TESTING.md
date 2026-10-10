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

## Repository-owned register corpora

Original NES, GB, SNES, GBA, N64 and Virtual Boy ROMs are committed with their Python source generators.
Build reproducibility and source/image SHA256 are checked with
`python3 tests/roms-src/build.py --original-only --check`, without a downloaded
ROM, Docker, or an assembler. Run the suites with
`ctest --test-dir build -L owned --output-on-failure`.

The [NES ADC/SBC corpus](tests/roms-src/nes/alu_exhaustive/README.md) records
524,288 accumulator/status pairs through `$401E`. Its host oracle independently
checks every operand pair, carry input, and decimal-flag setting for zero-page
ADC/SBC. The ROM reports completion with an unknown verdict; the ordinary file
detector must not infer PASS from these raw `REG` lines. The dedicated runner
reports PASS only after the observations match and the entire expected corpus
is present. An opcode mutation and damaged file controls must fail.

The [GB register probe](tests/roms-src/gb/register_probe/README.md) sends observed
WRAM and ADD results through SB/SC. Its `REG`, `CHECK`, and `END` records are
scored by the ordinary file detector, and an injected wrong WRAM value must fail.
The [GB byte ALU corpus](tests/roms-src/gb/alu_exhaustive/README.md) observes
8,388,608 A/F pairs: ADD, ADC, SUB, SBC, AND, XOR, OR, and CP using register E,
all operand pairs, and all 16 incoming Z/N/H/C states. Its independent integer
oracle grades every result, including half carry/borrow and CP retaining A.
Completion has unknown status; a mutated opcode must fail, and damaged or
false-PASS result files must be measurement errors. SB/SC writes use the existing
test hook without waiting for physical serial delivery. Timing, other operand
locations, CGB operation, and retail boot are not established by this corpus.
The [GB unary corpus](tests/roms-src/gb/unary_exhaustive/README.md) observes
73,728 A/F pairs across 18 accumulator instructions, every byte, and every
incoming flag state. It includes all DAA inputs, accumulator and CB rotations,
shifts, INC/DEC and flag controls. An independent host oracle grades results;
an executed mutation and seven damaged-file controls verify failure detection.
Other operand locations, BIT/RES/SET, timing and physical hardware remain
separate unverified domains. The same SB/SC debug observation hook is used.
The [SNES math corpus](tests/roms-src/snes/math_registers/README.md) streams
65,536 multiplication products and 3,328 division quotient/remainder pairs through
`$21FE`. Its integer host oracle grades every settled observation, including
division by zero. A trigger mutation must fail and damaged files must be errors.
This suite checks settled results; division covers 13 boundary dividends with
all 256 divisors. The separate [SNES math timing ROM](tests/roms-src/snes/math_timing/README.md)
observes 504,832 individual register bytes at CPU phases 2–18: 16 multiplicand
patterns and 13 boundary dividends, all 256 second operands, and four result
registers. Its closed-form oracle checks intermediate and final states, including
old quotient bits and division by zero. A changed register sample must fail;
damaged files must be errors. `snes_math_pipeline_test` supplements it with
every multiplication pair at phases 0–8, selected division cases at phases 0–16,
busy writes, pending-state save/resume, cancellation, and actual-plugin state
roundtrips. DRAM refresh, DMA/HDMA, interrupts, and physical hardware remain
unverified. SNES states now use `VSNES002`; recreate old unversioned states.

The [N64 integer corpus](tests/roms-src/n64/integer_registers/README.md) executes
20 MIPS instructions through the experimental loadable core's marked homebrew
entry profile. It records 28,160 GPR/HI/LO observations; 28,127 defined results
match the independent oracle and 33 undefined divide inputs are unscored.
All three cartridge byte orders execute through frontend discovery. Mutation
and damaged-file controls check rejection. `n64_machine_test` separately checks
control flow/delay slots, basic exceptions, memory and plugin state continuation.
N64 retail boot, graphics/audio, analog/accessory controllers, remaining peripherals and timing remain
unimplemented or unverified. Its trailer cycle counter measures instruction
steps, and frames are scheduling quanta, not VI fields.
The [N64 doubleword multiply/divide corpus](tests/roms-src/n64/muldiv64/README.md)
adds 263,168 LO/HI observations for DMULT/DMULTU/DDIV/DDIVU. Its independent
128-bit integer oracle matches all 262,623 defined results. Every signed or
unsigned byte pair is exercised as 64-bit input, along with 16×16 full-width
boundary cases; 545 undefined divide inputs are unscored. Wrong executed
signed/unsigned instructions and seven damaged-file controls are rejected.
All three archive byte orders execute through the real frontend. Full 64-bit
operand enumeration, timing/hazards and physical hardware remain unverified.
The [N64 merge-memory corpus](tests/roms-src/n64/merge_memory/README.md)
checks 3,158,016 loaded-GPR/stored-memory observations for all eight merge
instructions, every byte lane, every repeated-byte value pair and full-width
boundaries. Its independent oracle uses byte-array copies; word sign extension,
preserved bytes and the adjacent word-store sentinel are graded. A two-opcode
mutation and seven damaged-file controls verify rejection. Unit tests separately
cover paired accesses in both orders, cross-block results, SP storage, masked
transactions without RMW reads, guard bytes, offsets/aliasing and partial-state
continuation. Timing, MMIO, other addressing modes and physical hardware remain
unverified. Run the `n64_merge_memory` CTest gate for the complete declared ROM
domain; the runner records files under `build/artifacts/owned/n64_merge_memory/`.
The [GBA ARM shift corpus](tests/roms-src/gba/arm_shifts/README.md) records
65,536 result/CPSR pairs over the `mgba` channel: four shifts, every low-byte
amount, all C/V inputs, and 16 operand patterns. Timing and the full source
operand domain remain unverified. An opcode mutation, damaged files, and a
channel enable/disable and buffer boundary probe exercise the measurement path.
It uses direct cartridge-entry boot and contains no retail BIOS logo.

The experimental [Virtual Boy core](cores/vb/README.md) runs five original
register corpora through frontend plugin discovery: [integer operations](tests/roms-src/vb/integer_registers/README.md)
(6,512,640 result/PSW pairs), [multiply/divide](tests/roms-src/vb/muldiv/README.md)
(4,210,688 nine-word snapshots, including divide-zero entry and RETI),
[TRAP/illegal exceptions](tests/roms-src/vb/exceptions/README.md)
(4,685,824 thirteen-word snapshots) and [address traps](tests/roms-src/vb/address_traps/README.md)
(4,982,784 fourteen-word snapshots), plus [CAXI/port transactions](tests/roms-src/vb/memory_ops/README.md)
(17,055,744 nine-word snapshots). Port callbacks share the marked test
profile's storage decoder; physical wiring/bus-lock timing remains unverified.
Each has an independent host oracle,
executed mutations and damaged-file rejection. The exception/address-trap
ROMs cover every writable PSW with NP clear in their declared address/opcode
subdomains; fatal paths are separate component evidence. Frames and cycles
measure scheduling quanta/step attempts, not VIP or pipeline timing. Retail
Virtual Boy games, graphics/audio/controllers and remaining hardware are
unimplemented or unverified. Run `ctest --test-dir build -R '^vb_' --output-on-failure`.

All suites retain raw output and reports under `build/artifacts/owned/`, which
CI uploads. These results are separate from the existing external-ROM weighted
scorecards; exact domain coverage is documented in
[HARDWARE-COVERAGE.md](docs/HARDWARE-COVERAGE.md). Other CPU behaviors, timing,
video, audio, mappers, and remaining platforms still require original corpora.

## Controller integration evidence

Build `input_hotplug_test` and run
`ctest --test-dir build -L controllers --output-on-failure`. This uses actual SDL virtual game controllers to test
identical devices, duplicate hotplug events, custom mappings, capture isolation,
disconnect/reconnect, config persistence, selection after removal, ambiguous
identities, and axis endpoints. The test uses a temporary config directory and
requires no physical pad or display. SDL before 2.0.14 reports SKIP rather than
PASS. Physical USB/Bluetooth behavior and multiplayer routing remain unverified.

The [rollback execution test](plugins/netplay_default/README.md) loads the actual
NES plugin with an original controller-polling ROM. Two sessions receive inputs
three/five frames late, with final confirmations reversed. Both converge on
the ordinary 100-frame baseline, including every serialized state byte. The test
checks stalls, confirmation conflicts, stale/future bounds, serialization errors,
and replay audio handling. Guest registers and final states remain in
`build/artifacts/rollback/`. Run `ctest --test-dir build -L netplay --output-on-failure`.

`netplay_network_test` launches two independent NES processes using IPv4 TCP
loopback. Delayed inputs cause replay, and both complete states match the same
100-frame baseline. It also checks ROM mismatch and fragmented zero-length,
oversized, unknown-type, and truncated messages. Reports, guest registers,
logs, and states remain in `build/artifacts/network/`. Build `netplay_network_peer`
and run the same netplay label. Local socket permission is required.

`netplay_plugin_test` repeats those checks through the production netplay shared
plugin with an actual NES plugin in each process. Its host fixture uses the same
frontend contract: poll while paused, let the plugin own one live frame, and
gate streaming audio using `is_rolling_back()`. It verifies real connection
callbacks only after handshake, byte-identical final states, replay audio, and
disconnect/pause when the core pointer or frame changes. Before connecting, the
host runs 17 frames and the client runs 31; preparing a fresh core removes those
different histories. The application uses the same fresh-core preparation while
preserving the selected core and normal battery-save handling. Artifacts are saved in
`build/artifacts/netplay-plugin/`; build `netplay_plugin_peer` and use the netplay
label. The application now wires this contract into its GUI frame loop. These
tests do not exercise physical two-machine play, the rendered GUI, or Internet
latency/loss behavior.

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
variables (`VELOCE_TEST_OUT`, `HEADLESS=1`, `FRAMES`, `SAVE_SCREENSHOT`, `TRACE`;
`DEBUG=1` only for the deprecated `memory`/`serial` stdout fallback),
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
| `file` | every console (target for all scored tests) | The app writes the ROM's result channel to `VELOCE_TEST_OUT` in the VELOCE-RESULT/1 format (`CHECK id PASS\|FAIL name exp= got=`, `END pass/total`). Per-check detail, partial credit from the CHECK tally, early exit at `END`. Spec: [docs/testing/VELOCE-RESULT.md](docs/testing/VELOCE-RESULT.md). `memory`/`serial` are deprecated aliases that fall back to the stdout parsers below until the core emits to the file. |
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

The GitHub Actions workflow (`.github/workflows/accuracy.yml`, self-hosted runner
only; the project does not use GitHub-hosted runners) mirrors this: a
`fast-gates` job on every change, a per-console `accuracy` matrix that builds the
binary and uploads each scorecard JSON plus its `artifacts/` (result files,
screenshots, traces), and an
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
