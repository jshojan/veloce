# Veloce Testing and Accuracy Framework

Current N64 evidence: [Copy-cycle original RAM/register grading and Mario title-head probe](cores/n64/README.md#copy-cycle-transfers-and-mario-title-head). Earlier increment reports are historical.

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

Original NES, GB, SNES, GBA, N64, Virtual Boy and DS ROMs are committed with their Python source generators.
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

The experimental [Virtual Boy core](cores/vb/README.md) runs seven original
register corpora through frontend plugin discovery: [integer operations](tests/roms-src/vb/integer_registers/README.md)
(6,512,640 result/PSW pairs), [multiply/divide](tests/roms-src/vb/muldiv/README.md)
(4,210,688 nine-word snapshots, including divide-zero entry and RETI),
[TRAP/illegal exceptions](tests/roms-src/vb/exceptions/README.md)
(4,685,824 thirteen-word snapshots) and [address traps](tests/roms-src/vb/address_traps/README.md)
(4,982,784 fourteen-word snapshots), plus [CAXI/port transactions](tests/roms-src/vb/memory_ops/README.md)
(17,055,744 nine-word snapshots) and [arithmetic bit strings](tests/roms-src/vb/bitstrings/README.md)
(14,680,064 fourteen-word snapshots across eight operations, byte pairs, offsets
and boundary lengths), plus [immediate instructions](tests/roms-src/vb/immediates/README.md)
(100,712,448 destination/PSW/source snapshots, all wide/short immediates for
twelve instructions on sixteen operand patterns and all incoming flags).
Bit-string overlap, interrupt abort/resume and buffer
bus timing remain unverified. Port callbacks share the marked test
profile's storage decoder; physical wiring/bus-lock timing remains unverified.
Each has an independent host oracle,
executed mutations and damaged-file rejection. The exception/address-trap
ROMs cover every writable PSW with NP clear in their declared address/opcode
subdomains; fatal paths are separate component evidence. Frames and cycles
measure scheduling quanta/step attempts, not VIP or pipeline timing. Retail
Virtual Boy games, graphics/audio/controllers and remaining hardware are
unimplemented or unverified. Run `ctest --test-dir build -R '^vb_' --output-on-failure`.

The experimental [DS core](cores/nds/README.md) executes two original ARM guest
programs in its [integer corpus](tests/roms-src/nds/integer_registers/README.md):
44,238,336 destination/CPSR/source snapshots across sixteen operations, all
repeated-byte pairs and full-width boundaries, every NZCV and both ARM9 Q states.
ARM9/ARM7 share main RAM and execute from their loaded main/private segments.
The host grades both output streams independently; an ADC-to-ADD mutation and
seven damaged files check failure detection. Component gates cover aliases,
shifters, WRAM banks, validation and exact plugin state replay. Firmware,
ARM block transfers, physical timing, graphics/audio/touchscreen, physical controller use and
DSi enhanced hardware in its separate plugin remain pending.
The [multiply corpus](tests/roms-src/nds/multiply_registers/README.md) adds
38,338,560 five-word observations across six multiply/accumulate forms, both S
states, all NZCV/ARM9 Q inputs and accumulator wraparound. Full CPSR is recorded;
only architecturally unpredictable ARM7 S-form C or C/V bits are unscored.
Executed signed/unsigned mutation, five CPSR mask controls and seven damaged
files validate the grader. Components add 1,179,648 per-bit product/alias cases.
The [Thumb corpus](tests/roms-src/nds/thumb_registers/README.md) matched
51,426,816/51,426,816 observations with zero mismatches over sixteen low-register ALU operations, register
ADD/SUB, and every immediate encoding for shifts and low register ALU operations. Every case switches to Thumb
and back and captures a Thumb PC read alongside result/CPSR/source registers.
The reduced 1,083,264-observation cartridge passed, its ADC-to-SBC mutant failed
only ADC groups, and flag/PC mutations plus seven malformed files checked the
independent grader. ARM7 MUL C is captured but unscored in 1,052,672 cases.
Component checks cover
high registers, PC/SP address forms, branch offsets, register BLX architecture
differences, state alignment and actual shared-plugin Thumb replay.
The [memory corpus](tests/roms-src/nds/memory_registers/README.md) matched
49,694,208/49,694,208 nine-word observations with zero mismatches for ARM halfword/signed and Thumb transfers;
every recorded bit is graded. It covers all halfword values, all immediate fields,
all NZCV/ARM9 Q inputs, addresses/writeback, source, memory guards and SP.
The reduced 213,504-case ROM passes; executed signed-load, store-width and missing-store mutants
are rejected. Every missing-store case must fail at both halfword lanes. Components add 393,216 ARM attempts (147,456 odd-half diagnostics),
128 ARM aliases, 524,288 Thumb register cases and 30,720 immediate/SP/PC cases.
The byte oracle checks width, sign extension, writeback and failed-transaction
atomicity. Odd halfwords and unaligned Thumb words are outside the hardware grade.
The [long-call corpus](tests/roms-src/nds/long_calls/README.md) matched
9,437,184/9,437,184 eight-word observations, zero mismatches, with every bit
graded. Valid adjacent BL/ARM9 BLX pairs cover each high and valid low field
individually, both halfword positions, sixteen LR seeds and all NZCV/ARM9 Q.
The reduced 9,472-case ROM passes; executed sign and link mutants fail 9,472
and 3,328 cases respectively. Eight single-field mutations fail and seven
malformed files return ERROR. Components cover 20,971,515 defined combined
encoding pairs separately from the ROM field domain, excluding five unpredictable
self-suffix targets, plus 6,720 boundary/flag/diagnostic attempts. Actual plugin
frames cut after a prefix on each CPU; restored suffix execution reproduces
whole states exactly, including ARM9 BLX. Dynamic code patches are backed up and
restored under the cache-free test profile; physical cache coherence, exception
handling and timing remain unverified.
The [stack/block corpus](tests/roms-src/nds/multiple_registers/README.md) matched
3,720,960/3,720,960 twenty-five-word snapshots, zero mismatches: every defined list/base encoding,
sixteen register/memory seeds, all NZCV/ARM9 Q and POP-PC target states.
Its reduced 19,200-observation ROM passes; executed missing-store and
missing-load mutants fail 7,680 and 11,520 cases respectively, with unaffected
groups passing. Twenty-five single-field mutations fail and seven malformed
files return ERROR. Components cover 3,327,744 defined transfers and 604,416
unpredictable-list diagnostics, plus unaligned model/undefined-target checks
and injected failures. Actual plugin stack loops save/replay whole CPU/RAM
states exactly. Static thunks avoid code patching; CP15/cache, abort behavior,
physical ordering and timing remain unverified.
The [keypad corpus](tests/roms-src/nds/keypad/README.md) matched all 4,096
digital combinations with zero mismatches through `INPUT_FILE`, using a Gray sequence with one button
change per frontend frame. Both CPUs capture KEYINPUT and ARM7 captures EXTKEYIN
with halfword/byte reads; ARM9 outputs their ten-word snapshots after a shared-RAM
handshake. The reduced 16-snapshot ROM passes; an executed polarity mutant must
fail all sixteen. Nine defined-field mutations must FAIL, five reserved-bit
controls must PASS, and seven malformed files must return ERROR. Unknown bits
remain visible but unscored. Components check all masks, read lanes/rights,
ignored writes, normalized saved input, reset and legacy-state migration.
Actual plugin tests verify the twelve-button layout, held/released input and
exact restoration. Lid/debug/touch/SPI, KEYCNT/IRQ and physical device testing
remain pending; this frame schedule establishes no physical polling timing.
All seven DS corpora serialize register words through emulator-only `$04FF0008`
while retaining byte framing and the same independent graders. Integer controls
compare 347,136 independently graded native/legacy observations byte-for-byte;
`legacy-byte.nds`, `.result` and `.json` remain with the integer artifacts.
Components cover each byte position/value, invalid transactions, register/flag
preservation and actual dual-CPU plugin output/completion replay. The transport
reduces printing instruction attempts; it does not establish physical timing.
Run `ctest --test-dir build -R '^nds_' --output-on-failure`.

All suites retain raw output and reports under `build/artifacts/owned/` for
local inspection. These results are separate from the existing external-ROM weighted
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
  run_all.py             root orchestrator -> cross-console roll-up + local gates
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
`tests/run_all.py` aggregate a platform roll-up and provide local regression gates.

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
python tests/run_all.py --min-overall 80          # local gate: fail under 80%
python tests/run_all.py --baseline scorecard.json --no-regressions
```

Each console's runner clones the public test-ROM repositories declared in its
`test_config.json` on first run; ROMs are never committed. A missing ROM reports
`SKIP` (excluded from the score) rather than a false fail.

### Continuous integration

Testing is wired in two tiers:

- **Fast gates** (`ctest --test-dir build -L fast`): the pure-logic testkit selftest and
  config validation, plus component and reproducibility checks, labelled `fast`.
  These are run locally; pure-logic checks need no ROMs or built emulator.
- **Accuracy suites** (`ctest --test-dir build -L accuracy`): the per-console
  runs, labelled `accuracy;slow`, with timeouts and a `REQUIRED_FILES`
  dependency on the `veloce` binary.

CI/CD is disabled for now. The GitHub Actions workflow definitions have been
removed; verification and artifacts are produced by the local commands above.

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

`nds_netplay_plugin_test` uses the same production-plugin harness with an original
[DS polling probe](tests/roms-src/nds/netplay_probe/README.md). The host owns one
DS keypad; the joining peer is a viewer. Both 100-frame full states match a
baseline after fresh-core preparation, and eight guest RAM words match an
independent load-frame/loop-position arithmetic oracle. Viewer rollback,
controller ownership, mismatch rejection and lifecycle pause/disconnect pass.
Single-controller component cases reject nonzero viewer confirmations and ignore
viewer local input; DS fast-state bounds/invalid buffers and ordinary state
identity are checked. Artifacts remain in `build/artifacts/netplay-nds/`.
No DS audio is produced or verified. DS wireless multiplayer, physical two-machine
use, rendered GUI and Internet/cross-platform behavior remain pending.

## Original GameCube and Wii PowerPC programs

`gc_integer_registers` and `wii_integer_registers` execute distinct original
marked images through the real frontend/plugins and `ppcdebug` file sink. Each
independent host oracle matches 49,152 six-word observations over the declared
arithmetic boundary domain. Executed wrong-result controls must fail all 768
reduced cases; each field mutation must fail and seven malformed streams must
return ERROR. Raw files and reports are in `build/artifacts/owned/gc_integer/`
and `build/artifacts/owned/wii_integer/`. `ppc_machine_test` additionally grades
2,752,512 arithmetic/register snapshots across both profiles plus flags,
RAM/branch behavior, marked image
validation, state atomicity/replay and both actual shared plugins.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|gc_integer_registers|wii_integer_registers)$' --output-on-failure
```

These are experimental supplied integer profiles. The ROMs contain no expected
answers or verdicts. Passing these domains does not verify retail startup,
physical timing, GPU/DSP, controllers, Wii MEM2/IOS or netplay. See the
[implemented subset and limits](cores/ppc/README.md).

### Conditional branch register corpora

`gc_branch_registers` and `wii_branch_registers` each grade 74,752 seven-word
BC/BCLR/BCCTR observations through the real frontend. Domains include canonical
BO, every BI, four CR patterns, CTR boundaries, both LK states and indirect
low-bit alignment. The host reads actual PC anchor sequences in the image and
grades branch decisions and captured LR/CTR/CR/XER/PC/sentinel independently.
Executed result, condition and LK mutations must fail exactly 256, 144 and 128
reduced cases. Seven field mutations must fail and seven malformed streams
must return ERROR. Files/reports are retained in
`build/artifacts/owned/gc_branches/` and `build/artifacts/owned/wii_branches/`.

`ppc_machine_test` now also matches 4,227,072 branch snapshots across both
profiles, covering every BD field with AA/LK and wrapping targets, all canonical
BO/BI and selected CR/CTR/LR boundaries. It verifies 256,000 invalid XL forms
as diagnostics, ignored z-bit behavior separately, and complete replay of an
original counted loop through both shared plugins. Noncanonical BO, newer BH,
physical prediction/timing and other console hardware are not hardware-scored.

```sh
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_(integer|branch)_registers)$' --output-on-failure
```

### Comparison and condition-register corpora

`gc_condition_registers` and `wii_condition_registers` each match 42,108,928
eight-word snapshots through the actual frontend: 131,072 CMP/CMPL, 33,554,432
CMPI/CMPLI, 8,388,608 CR logical, 2,048 MCRF and 32,768 MCRXR cases. The
[domain table](tests/roms-src/gc/condition_registers/README.md) specifies the
operand/CR/XER patterns; every immediate and logical bit-field encoding is
covered within those declared patterns. The independent host oracle checks
every bit of CR/XER, sources, stimulus instruction, initial seeds and sentinel.
Executed corruption/signedness/XER-clear controls fail exactly 864/68/8 reduced
cases; eight field mutations fail and seven malformed streams return ERROR.
Raw output and JSON reports remain in `build/artifacts/owned/gc_conditions/`
and `build/artifacts/owned/wii_conditions/`. Each full file is about 2.8 GiB.

The component gate matches 101,941,248 comparison/condition snapshots across
both profiles, forcing every defined logical truth input/field alias, every
compare immediate, ordinary GPR0 reads, all GPR/CR field encodings, MCRF nibble
pairs and MCRXR byte counts. It validates unsupported/reserved forms as
diagnostics and exact replay of known guest captures through both plugins,
including state restoration immediately before MCRXR clears XER flags.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_condition_registers)$' --output-on-failure
```

Runtime-generated instructions are supported by the supplied flat mapping.
Physical cache/pipeline synchronization, timing, exceptions, full 32-bit
operand/CR-word domains and most platform hardware are not verified. CI/CD
remains disabled; run these gates locally.

### Integer logical register corpora

`gc_logical_registers` and `wii_logical_registers` each match 6,324,224 nine-word
observations through the actual frontend: 32,768 register cases and 6,291,456
immediate cases. [The domain table](tests/roms-src/gc/logical_registers/README.md)
specifies operands, CR seeds, XER flags, Rc and fixed guest GPR fields. Every
16-bit immediate is covered for ORI/ORIS/XORI/XORIS/ANDI./ANDIS. on eight source
patterns/both SO states. The independent truth-table oracle grades every bit
of result, CR, XER, sources, instruction encoding, initial seeds and sentinel.
Executed result/CR/XER mutants fail exactly 512/192/512 reduced snapshots; nine
field mutations fail one row each and seven invalid streams return ERROR.
Raw files (~477 MiB each) and JSON reports remain in
`build/artifacts/owned/gc_logical/` and `build/artifacts/owned/wii_logical/`.

The component gate matches 21,856,256 integer logical snapshots across both
profiles: every register field encoding/alias (including GPR0), all bit truth
inputs, immediate values and boundary immediate register fields, preserved
flags and exact machine/plugin replay. Undefined-behavior sanitization passes.
Invalid high-XO forms diagnose without architectural register mutation.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_logical_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

These finite domains do not exhaust all operand/CR/XER words. Physical cache,
timing, exceptions and most console hardware remain pending; CI/CD stays disabled.

### Word rotate and mask corpora

`gc_rotate_registers` and `wii_rotate_registers` each match 9,437,184 ten-word
snapshots through the frontend: 4,194,304 RLWIMI, 1,048,576 RLWINM and 4,194,304
RLWNM cases. The [domain table](tests/roms-src/gc/rotate_registers/README.md)
defines source/destination patterns, all shift/mask fields, upper count bits,
CR seeds and incoming SO/Rc states. Independent grading checks every captured
bit, including preserved CR/XER and initial destination. Executed result,
mask and insert mutants fail exactly 3,584/1,472/496 reduced snapshots;
ten field mutations fail one row each and seven invalid streams return ERROR.
Raw files (~783 MiB each) and JSON reports remain in
`build/artifacts/owned/gc_rotates/` and `build/artifacts/owned/wii_rotates/`.

The component gate matches 113,258,496 rotate/mask snapshots across both
profiles: all masks/shifts within declared patterns/all XER flags, every GPR
encoding and alias on eight boundary masks, all walking source bits/shifts/masks,
all XER byte counts at a wrapped mask and exact machine/plugin replay.
Undefined-behavior sanitization passes in the machine and both loaded plugins.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_rotate_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/gc/rotate_registers/run.py --controls-only
```

All masks/shifts are exhausted within these patterns; full operand pairs,
CR/XER words, physical cache/timing/exceptions and most console hardware remain
pending. CI/CD remains disabled.

### Shift, sign-extension and leading-zero corpora

`gc_shift_registers` and `wii_shift_registers` each match 4,416,528 nine-word
snapshots through the frontend: 196,608 register shifts, 8,192 SRAWI, 16,384
EXTSB, 4,194,304 EXTSH and 1,040 CNTLZW observations. The [domain table](tests/roms-src/gc/shift_registers/README.md)
defines full low-width/count fields and finite upper/source/CR/XER patterns.
Independent arithmetic/bit-length grading checks every captured bit. Executed
result/carry/count mutants fail exactly 3,856/640/1,280 reduced snapshots; the
count mutant uses a separate 3,072-case register-shift domain. Nine field
mutations fail one row each and seven invalid streams return ERROR. Raw files
(~333 MiB each) and JSON reports remain in `build/artifacts/owned/gc_shifts/`
and `build/artifacts/owned/wii_shifts/`.

The component gate matches 37,042,208 shift/unary snapshots across both profiles:
all declared patterns plus every GPR/count field and alias at boundaries,
walking source bits/all counts, all XER byte counts, reserved unary-field
rejection and exact machine/plugin state replay before CA changes. The machine
and both loaded plugins pass undefined-behavior sanitization.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_shift_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/gc/shift_registers/run.py --controls-only
```

This exhausts count and extension input fields within declared patterns, not
all full 32-bit operands/CR/XER words, physical cache/timing/exceptions or console
hardware. CI/CD remains disabled.

### Carry arithmetic and negation corpora

`gc_carry_registers` and `wii_carry_registers` each match 25,168,384 nine-word
snapshots through the frontend: 2,560 NEG/extended unary cases and 25,165,824
carry immediate observations. [The domain table](tests/roms-src/gc/carry_registers/README.md)
defines every immediate value on sixteen sources/all XER flags and all unary
OE/Rc states, finite CR seeds, fixed guest GPR fields and byte count. Independent
mathematical/unsigned-carry grading checks every captured bit. Executed result,
stale-CA and cleared-SO mutants fail exactly 272/120/140 reduced snapshots;
nine field mutations fail one row each and seven invalid streams return ERROR.
Raw files (~1.85 GiB each) and JSON reports stay in `build/artifacts/owned/gc_carry/`
and `build/artifacts/owned/wii_carry/`.

The component gate matches 56,083,456 carry arithmetic snapshots across both
profiles: declared patterns plus every GPR encoding/alias, walking sources,
all XER byte counts, reserved unary-field rejection and exact machine/plugin
replay before OV/SO changes. The machine and both loaded plugins pass
undefined-behavior sanitization.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_carry_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/gc/carry_registers/run.py --controls-only
```

The immediate field is exhausted within these patterns, not all full 32-bit
sources/CR/XER words or physical cache/timing/exceptions. Most console hardware
remains pending. CI/CD stays disabled.

## PowerPC multiply/divide coverage

`gc_multiply_registers` and `wii_multiply_registers` each match 8,421,376
nine-word observations: 8,420,320 defined results and 1,056 undefined division
rows with defined flags/preserved fields checked. The
[declared domain](tests/roms-src/gc/multiply_registers/README.md) covers
MULHW/MULHWU/MULLW/DIVW/DIVWU on sixteen operand patterns/all XER flags/OE/Rc
and MULLI on every 16-bit immediate. Independent arbitrary precision host
arithmetic masks undefined quotient and recorded CR0.LT/GT/EQ only. CR0.SO,
XER and preserved fields remain graded. Executed wrong-result/high-word/flags
controls fail exactly 328/32/54 reduced rows. Undefined result mutations pass;
defined SO/OV mutations on undefined rows each fail one row. Strict malformed
stream controls and nine capture-field mutations pass their rejection gates.

The component gate matches 26,443,776 snapshots across both profiles,
including 280,640 deterministic undefined-result profile fixtures. Its oracle
uses 16-bit product limbs and restoring division; register/alias/byte-count,
reserved encoding, known guest and state replay checks pass, including with
undefined-behavior sanitization. Raw files (~634 MiB/profile) and JSON reports
stay under `build/artifacts/owned/gc_products/` and `wii_products/`.
No full 32-bit operand-pair or physical cache/timing claim is made. CI/CD
remains disabled; run the local gates:

```sh
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_multiply_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

## PowerPC integer memory coverage

`gc_memory_registers` and `wii_memory_registers` each match 12,586,368 ten-word
observations through their production plugins. The
[declared domain](tests/roms-src/gc/memory_registers/README.md) covers fourteen
load/store displacement forms at every aligned signed 16-bit displacement on
eight patterns/three aliases, plus eighteen indexed forms on eight offsets.
Update and algebraic halfword loads and byte-reversed transfers are included.
CR/XER seeds are selected per group, not crossed with every input. The host
uses independent byte arrays and endian/sign arithmetic to grade every bit of
result, base, index, CR/XER, target word, surrounding guards, instruction and
source. Executed result/sign/update mutants fail exactly 1,968/120/768 reduced
rows; ten field mutations and seven strict invalid streams are rejected.

The component gate matches 27,560,000 snapshots across both profiles:
26,356,500 valid transfers plus 1,203,500 invalid-form/access diagnostics.
Complete GPR arrays, PC/count, special registers and sixteen neighboring bytes
are checked with a separate byte oracle. Register fields/aliases, ordinary
RB=0, special RA=0, reserved Rc and update restrictions are covered. A known
mixed-transfer guest and exact replay before STWUX's RAM/base update pass.
The full component gate passes undefined-behavior sanitization. Raw files
(~1.02 GiB/profile) and JSON reports stay in `build/artifacts/owned/gc_memory/`
and `wii_memory/`. Physical cache/MMU, exceptions and timing remain pending.
CI/CD stays disabled; use local gates:

```sh
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_memory_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

## DSi plugin and original RAM evidence

The [DSi plugin](cores/dsi/README.md) is now separately loadable, sharing the
DS ARM interpreter with a distinct loader/state/controller identity and 16 MiB
RAM profile. `dsi_ram_registers` matches 387,072 fourteen-word original captures:
three RAM-limit modes, four banks, either writing CPU, three store/read aliases,
all aligned width/lane pairs and 256 byte/complement patterns. An independent
host byte-array oracle grades every field. Executed bank/width/mode mutants
fail exactly 1,512/1,038/4,032 reduced rows; fourteen field mutations and seven
strict invalid streams are rejected. Raw output (~44 MiB) and reports stay in
`build/artifacts/owned/dsi_ram/`.

`dsi_machine_test` matches 67,109,008 byte/width bus observations, writing each
supplied RAM byte once with an address-derived value and reading three mirrors
through the other CPU. Boundary widths, RAM-limit controls/locks, high-RAM
code, atomic loader/state rejection, DS/DSi coexistence and exact replay pass.
The component gate also passes undefined-behavior sanitization. These are
supplied-profile byte checks, not physical chip-control/timing evidence.

`dsi_netplay_plugin_test` passes production TCP rollback with two independent
100-frame peer states matching the 16,875,896-byte solo baseline and eight
guest words matching the independent oracle. Host owns one keypad; viewer
replay is supported, DSi wireless multiplayer is pending. DS component and
production netplay regression gates pass. Local TCP gates require loopback
socket permission; CI/CD remains disabled. All 64 original ROMs reproduce
against the source/image ledger. Firmware/retail boot, full SCFG/MBK and most
DSi hardware remain pending; full platform completion is unmeasured.

```sh
ctest --test-dir build -R '^(dsi_|nds_machine_test|nds_netplay_plugin_test)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

### 3DS plugin, original ARM11 reversal corpus and component evidence

`ctr_plugin` supplies a distinct experimental `.v3ds` core: two ARM11 integer
contexts and one ARM9, 128 MiB FCRAM, 512 KiB AXI and 1 MiB ARM9 private RAM,
plus twelve active-low digital HID controls. Native software/firmware boot,
remaining CPU/hardware, graphics/audio, physical timing and 3DS wireless/link multiplayer remain
pending. Overall hardware completion is not measured.

`ctr_reversal_registers` matches **25,165,824/25,165,824 seven-word original
ROM captures**: two ARM11s × ARM/Thumb × REV/REV16/REVSH × all 32 NZCVQ × all
65,536 complemented-half inputs. ARM fields are fixed Rd4/Rm0, condition AL.
The independent host byte oracle grades result, captured CPSR, source,
encoding, initial CPSR and two sentinels. Thumb returns through BX before MRS,
so T is normalized to zero; direct component checks cover Thumb T preservation.
This domain does not exhaust all 32-bit sources or full platform hardware.

Reduced executed result/width/sign/flag controls fail exactly
3,072/1,024/1,024/3,072 rows; seven field mutations fail one row each and seven
invalid streams produce errors. Header/ROM CRC/channel/count/row width,
ordered block completion, ungraded trailer and EOF are mandatory. Raw output
(~1.48 GiB), original/control images and JSON grades remain in
`build/artifacts/owned/3ds_reversal/`.

`ctr_machine_test` matches **36,123,648 reversal register snapshots** and
**541,065,300 RAM bus observations**. Register aliases, ARM conditions,
full-width patterns, walking/complemented bits, GE and full flag/GPR/PC
preservation are separate component evidence. Reserved/R15 forms and ARM9,
DS/DSi feature isolation are checked. Every supplied RAM byte is written once
with an address-derived pattern and read through all permitted contexts;
selected endian widths, boundaries, ownership and atomic rejection pass.
These checks do not cover every address/value combination or physical chip
behavior. All 4,096 digital HID inputs, readonly lanes, atomic image/state
validation, full 135,791,096-byte state replay, a three-context writing guest,
and the dynamic plugin lifecycle pass. The full component gate passes
undefined-behavior sanitization; existing DS/DSi component regressions pass.
All 64 original ROMs reproduce against the source/image ledger.

```sh
cmake --build build --target veloce ctr_plugin ctr_machine_test -j4
ctest --test-dir build -R '^ctr_' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

CI/CD stays disabled. Full corpus timeout is 1,800 seconds. See
[core implementation and limitations](cores/3ds/README.md) and
[original domain and controls](tests/roms-src/3ds/reversal_registers/README.md).

### 3DS production host/viewer rollback

The 3DS core now exposes `INetplayCapable` with one host-owned HID keypad and
joining viewer. Fast states use the complete existing 135,791,096-byte format,
writing directly into caller buffers; loads validate before mutation. Full
canonical-state hashing includes every RAM region. Component and sanitizer
checks cover null/short buffers, write guards, canonical bytes/hash, far RAM
and host-input replay. The core remains an experimental integer profile.

The [original probe](tests/roms-src/3ds/netplay_probe/README.md) polls HID_PAD
on all three CPU contexts. `ctr_netplay_plugin_test` compares two independent
production TCP peers with a 100-frame solo baseline: complete states match
byte-for-byte, and twelve published guest words match the independent host
arithmetic oracle. Nine words hold three sum/last/count triples; three are
zero guards. Load/store positions crossing frame boundaries are graded.
Executed zero-input control changes exactly six words; twelve field mutations
and four invalid register streams fail. ROM mismatch and four fragmented
invalid TCP messages reject. Different prior histories reset; core/frame
replacement disconnects and pauses. This mirrors one instance, with viewer
prediction/replay, rather than emulating 3DS wireless multiplayer.

```sh
ctest --test-dir build -R '^(ctr_machine_test|ctr_netplay_plugin_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts remain in `build/artifacts/netplay-3ds/`. Window 8 stores nine full
states (~1.14 GiB per peer) plus core/scratch memory; compression/page sharing
and sustained real-time performance remain unverified. The TCP gate needs
loopback socket permission. Internet, physical devices/two machines and
cross-platform peers remain unverified. CI/CD stays disabled.

### Wii U three-context integer plugin and original immediate ROM

The separately loadable `wiiu_plugin` executes repository-owned `.vwiiu`
programs on three PowerPC integer contexts. The shared interpreter now takes
its bus explicitly; GC/Wii loader/map/state behavior is retained. MEM1 is
32 MiB and MEM2 is the full 2 GiB physical profile, backed by sparse 4 KiB
pages. Missing pages represent zero bytes, with canonical nonzero-page states.
Native boot, full CPU/GPU/audio, services, physical timing, GamePad hardware
and native wireless multiplayer remain pending; overall completion is unmeasured.

`wiiu_immediate_registers` matches **25,165,824/25,165,824 nine-word original
captures**: three contexts × ADDI/ADDIS × four source values × RA=0/3 × all
8 incoming XER flag combinations × all 65,536 immediates. The host integer
oracle independently grades result, CR, XER, source, tested encoding, seeds,
sentinel and context tag from the file. Shared RAM orders blocks; no guest
verdict, image or stdout is scored. This does not exhaust 32-bit sources or
all instructions/platform hardware. Reduced executed result/width/CR controls
fail exactly 3,072/1,536/2,304 rows; nine field mutations and seven invalid
streams are rejected. Identity/CRC/channel/domain/width/order/completion,
ungraded trailer and EOF are required. Raw output (~1.85 GiB), controls and
JSON grades stay in `build/artifacts/owned/wiiu_immediates/`.

`wiiu_machine_test` matches **28,311,552 register snapshots** and
**8,724,152,428 RAM bus observations**. Every mapped byte is written once with
an address-derived pattern and read through all three contexts; this covers
one chosen value per address. Selected width/endian/page/region boundaries,
full RT/RA fields/aliases on selected sources/immediates, CR/XER/other-register
preservation, atomic invalid images/states, complete dense RAM round trip,
sparse continuation, high-end code, synthetic input and plugin checks pass.
The full gate passes undefined-behavior sanitization; GC/Wii component
regression passes after interpreter extraction. The largest canonical state
is 2,183,168,992 bytes; dense tests need several
GiB of memory and a 64-bit host. Digital input is an original latch convention,
not native VPAD/KPAD/GamePad or wireless controller emulation.

```sh
cmake --build build --target veloce wiiu_plugin wiiu_machine_test ppc_machine_test -j4
ctest --test-dir build -R '^(wiiu_|ppc_machine_test)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The full component and corpus timeouts are 1,200/1,800 seconds. CI/CD remains
disabled. See [core scope](cores/wiiu/README.md) and
[original domain/controls](tests/roms-src/wiiu/immediate_registers/README.md).

### Wii U sparse-state production netplay

`wiiu_plugin` version 0.2.0 now exposes `INetplayCapable` with one host-owned
original digital-input latch and joining viewer. Fast snapshots write directly
into caller buffers; a null-buffer query returns the current canonical sparse
size. The stable maximum is 2,183,168,992 bytes. Prefix/page format remains
`VWUS0001`; a 100-frame state is byte-identical to the previous serializer.
Hashing covers every canonical scalar/page/index byte, with missing pages
representing zero over the full 32 MiB MEM1/2 GiB MEM2 map.

`RollbackSession` now sizes each slot from the current query and reuses capacity,
rather than reserving the dense upper bound for every frame. Frontend snapshots
use the same contract. `netplay_rollback_test` verifies 100-frame delayed and
reordered histories with states growing/shrinking between 16 bytes and 64 KiB
under a 2,183,168,992-byte declared bound. Both exact states and hashes converge;
zero/oversized queries, oversized write results and allocation failure stop
before an unsaved frame advances. Existing fixed-state behavior remains tested.

The [original Wii U probe](tests/roms-src/wiiu/netplay_probe/README.md) polls input
on all three contexts and writes/reloads an input bit at the high end of MEM2.
The page becomes nonzero or clears, exercising canonical growth/shrinkage.
`wiiu_netplay_plugin_test` passes two-process production TCP replay: both full
100-frame states equal the solo baseline and twelve guest words match the
independent arithmetic oracle. Final state is 17,392 bytes; the largest probe
snapshot is 21,492 bytes. Executed zero-input control differs in six words;
twelve field mutations and four invalid streams fail. ROM mismatch, four
fragmented malformed TCP messages, different prior histories and core/frame
replacement checks pass. Artifacts remain in `build/artifacts/netplay-wiiu/`.

```sh
ctest --test-dir build -R '^(wiiu_machine_test|wiiu_netplay_plugin_test|netplay_rollback_test)$' --output-on-failure
ctest --test-dir build -L netplay --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The variable-state rollback gate also passes undefined-behavior sanitization.
The full Wii U component gate passes undefined-behavior sanitization, including
dense/sparse canonical fast bytes/hash, guard checks and replay. All seven
netplay gates pass; all 64 original ROMs reproduce against the ledger.

Native GamePad/KPAD/VPAD, wireless multiplayer, retail boot, Internet, physical
devices/two machines, cross-platform peers and sustained performance remain
pending or unverified. High-water capacity is retained in each history slot;
dense workloads still require large states. Page sharing/delta histories are
pending. TCP gates need loopback socket permission. CI/CD remains disabled.

## GameCube/Wii settled digital controller gates

`gc_controller_serial` and `wii_controller_serial` each independently grade
4,096/4,096 digital combinations, sixteen guest register words per row. The
actual frontend input schedule and loaded core execute repository-owned ROMs.
One executed command mutation, sixteen field mutations and seven malformed
stream controls are rejected per platform. `ppc_serial_test` matches 262,144
selected-port/input/connection snapshots and 196,608 transfer-length snapshots,
checks receive preservation/errors, W1C, invalid accesses, legacy/current
states and actual plugin input replay. `ppc_machine_test` continues to pass.
These gates passed with undefined-behavior sanitization for the component code.

```bash
ctest --test-dir build -R '^(ppc_(serial|machine)_test|(gc|wii)_controller_serial|input_hotplug_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/update_completion.py --check
```

The ledger now includes 64 original ROMs. Controller polling, analog axes,
origin calibration, VI/wire timing, CPU SI interrupts, accessories, four-port
frontend input and Wii Remote support remain pending. CI/CD remains disabled.
The [delivery completion ledger](docs/CORE-COMPLETION.md) is separate from
hardware-domain pass fractions and legacy external-suite accuracy scores.
The optional `tests/n64_retail_probe.py` takes a locally supplied N64 ROM;
Super Mario 64 now loads and reaches 5,897,464 IPL3 step attempts, then faults at the unimplemented VI Current register; boot/playability remain unverified. Its ROM bytes never enter git.

## N64 cartridge DMA and IPL3 handoff

The [PI corpus](tests/roms-src/n64/pi_dma/README.md) matches 524,288 cases and
8,388,608 guest register values across all even lengths 2–65,536, four cartridge
offsets, two RDRAM addresses and both PI mask states. The [handoff corpus](tests/roms-src/n64/ipl3_handoff/README.md)
matches twelve startup captures without bundling firmware. Executed guest mutants,
all captured-field mutants and seven invalid streams per corpus are rejected.
All twenty-four N64 CTests pass; the PI component also passes undefined-behavior sanitization.
Settled aligned ROM reads and explicit HLE startup are implemented; RI/RDRAM
initialization, native retail boot, PI busy/misalignment/peripheral behavior,
graphics/audio and physical timing remain pending. The ledger contains 64 original
ROMs. CI/CD remains disabled.

## N64 RI configuration register evidence

The [original RI corpus](tests/roms-src/n64/ri_registers/README.md) matches
524,288 cases and 3,145,728 guest reads. Every 19-bit refresh value and every
mode/configuration/selection combination is covered; latency is correlated.
Executed selection mutation, six field mutants and seven invalid streams reject.
The component checks current-load latching, access/state failure atomicity,
legacy conversion, exact replay, reset and loaded-plugin execution. RI refresh
scheduling, analog calibration, bus-error production, native RAM remapping and
physical timing remain pending. Super Mario 64 advances to an unimplemented register read
(unimplemented VI Current register 803275F4, EPC 803275F4, Cause 1C) after 5,897,464 startup step attempts; it is
not bootable/playable. The corpus ledger contains 64 original ROMs.

## N64 MI control evidence

The [original MI corpus](tests/roms-src/n64/mi_controls/README.md) matches
393,216 rows / 3,145,728 guest captures: mode command words across all eight
flag states and interrupt-mask commands across all 64 masks, with actual SI/PI
DMA requests pending. Executed count mutation, eight field mutants and seven
invalid streams reject. The component checks 16,777,216 mode transitions, all
mask transitions, injected DP delivery/acknowledgment/ERET, exact replay, legacy
conversion, atomic rejection and production-plugin execution; it also passes
undefined-behavior sanitization. All twenty-four N64 gates pass. MI repeat/upper device-register effects exist; RAM-repeat/EBus transactions
and native RAM remapping remain pending.
Super Mario 64 stops at unimplemented VI Current register (EPC 803275F4, Cause 1C) after 5,897,464 startup
step attempts; retail gameplay, graphics/audio and physical timing remain pending.
All 64 original ROMs reproduce. CI/CD remains disabled.

## N64 RDRAM device evidence

The [original device corpus](tests/roms-src/n64/rdram_devices/README.md) matches
14,336 cases / 86,016 guest values across four devices, all writable delay
fields and repeated-byte storage patterns. Executed mutation, six field mutants
and seven invalid streams reject. Components check all device ID encodings,
delay/repeat combinations, selectors/register aliases, discovery, MI upper/repeat,
state replay and rejection, legacy conversion and loaded-plugin execution.
All twenty-four N64 gates pass; device components pass undefined-behavior sanitization.
Native RAM remapping, analog calibration, cache timing/errors, physical bus timing, repeat RAM
and EBus memory remain pending. Super Mario 64 reaches unimplemented VI Current register after
5,897,464 step attempts (EPC 803275F4, Cause 1C); gameplay/video/audio are unverified.
All 64 original ROMs reproduce. CI/CD stays disabled.

## N64 functional primary cache evidence

The [original cache corpus](tests/roms-src/n64/cache_coherence/README.md) matches
2,097,152 cases / 12,582,912 guest values across every D-cache index, byte lane
and initial byte, with cached/uncached alias mutation and dirty writeback. An
executed writeback mutation, six field mutants and seven invalid streams reject.
Components additionally cover all physical tag values, instruction staleness/refill,
dirty eviction, widths/merged stores, PI DMA staleness, CACHE operations/privilege,
exact dirty/stale state replay and legacy conversion. All twenty-four N64 gates
pass; cache components pass undefined-behavior sanitization. Timing/hazards,
TLB/address modes, cache errors, native RAM remapping and device bus transactions
remain incomplete. Super Mario 64 stops at unimplemented VI Current register 803275F4 after 5,897,464 attempts
(EPC 803275F4, Cause 1C); video/audio and gameplay remain unverified. All 64
original ROMs reproduce; CI/CD remains disabled.


## N64 SP interface evidence

The [original SP controls](tests/roms-src/n64/sp_controls/README.md) match
196,608 cases / 1,572,864 guest values over every writable status/interrupt
seed, all twelve command pairs and four command states. The
[original SP DMA](tests/roms-src/n64/sp_dma/README.md) matches 65,536 cases /
393,216 guest values across every raw length, both directions/banks and four
wrap boundaries. Executed mutations, all field mutations and seven malformed
streams reject per corpus. Component evidence checks full DMA bytes/guards,
262,144 count/stride transfers, PC/semaphore masks, CPU interrupt delivery/ERET,
cache staleness, states, reset and the loaded shared plugin. SP components pass
undefined-behavior sanitization; all twenty-four N64 CTests pass. All 64 original
ROMs reproduce. CI/CD stays disabled.

RSP scalar/vector execution, DMA queues/clocks, unaligned stride, open-bus DMA,
IO-full and exact address/length readback physical validation remain pending.
Super Mario 64 reaches unimplemented VI Current register `$803275F4` after 5,897,464 startup
attempts (EPC `$803275F4`, Cause `$1C` hex). Gameplay/video/audio remain
unverified. N64 is 35% on the delivery checklist; no complete CPU/device
milestone is claimed.


## N64 AI audio evidence

The [FIFO ROM](tests/roms-src/n64/ai_fifo/README.md) matches 2,048 cases /
22,528 guest values. The [PCM ROM](tests/roms-src/n64/ai_pcm/README.md)
matches nine guest FIFO/IRQ captures; the production plugin outputs 66,169 stereo
pairs covering all 65,536 signed sample values per channel to register files.
Independent host oracles reject executed mutations and malformed streams.
Components cover all length/address/rate fields, dividers, two-buffer FIFO,
start/promotion IRQs, delayed carry, playback-time RAM/cache visibility, pause,
IRQ/ERET, bounded host output, state replay/rejection and identical buffered/
streaming samples and states. The AI component passes undefined-behavior
sanitization. All twenty-four N64 gates pass; all 64 original ROMs reproduce.

N64 now has functional guest PCM output and **35%** delivery-checklist completion.
Physical CPU/DAC timing, serial/analog behavior, status clock fields, dynamic rate
phase behavior, RSP microcode, graphics and native retail playability remain
unverified. Super Mario 64 reaches unimplemented VI Current register after 5,897,464 attempts
(EPC 803275F4, backing word 8D8D0010, Cause 1C hex). No commercial ROM bytes enter
git. CI/CD stays disabled.

## N64 FPU controls and branch corpus (2026-10-10)

CFC1/CTC1 and BC1F/BC1T/BC1FL/BC1TL now implement the defined VR4300
control-register profile. CU1-disabled instructions raise coprocessor unusable
with CE=1. CTC1 commits FCR31 before raising enabled cause or unimplemented
operation FPE. Read-only FCR0 selects implementation 0x0B/revision zero.

The [original control ROM](tests/roms-src/n64/fpu_controls/README.md)
matches **8,192/8,192** cases / **65,536** guest register values, with actual
guest exception capture. An executed mutation fails every reduced row; eight
field mutations fail and seven incomplete/malformed streams are rejected.
The component covers **1,048,576** defined control combinations,
**524,288** branch offset/condition/kind combinations and **1,024** CU1-disabled
format/control-index cases, plus GPR transfers, delay/BEV/nested exception
context, pending-exception replay and atomic state rejection. S010 appends an
8-byte FCR31 scalar; S009 converts with neutral FCR31 and preserves pending
AI/FIFO/host phase state. Earlier state conversions remain supported.

All **65** repository-owned ROMs reproduce from the ledger. The FPU component
passes undefined-behavior sanitization. Raw register streams and independent
reports are under `build/artifacts/owned/n64_fpu_controls/`.
N64 stays **35%** on the delivery checklist: FPR data transfers, arithmetic,
conversions, comparisons, NaN/denormal behavior and physical pipeline timing
remain pending; no full CPU or hardware-accuracy milestone is awarded.
This original cartridge has not been validated on physical hardware.

Super Mario 64 now passes CFC1 and reaches unimplemented VI Current register after
**5,897,464** attempts (EPC `803275F4`, instruction-cache and backing word
`8D8D0010`, Cause `1C` hex). The production plugin still reports uniform video
and zero retail PCM pairs. Boot and playability remain unverified. No commercial
ROM bytes enter the repository. CI/CD remains disabled.

All **26/26 N64 CTests pass** (345.82 seconds); the complete run includes every
original N64 register ROM, PCM ABI oracle and all nine component tests.

## N64 TLB translation and fault corpus (2026-10-10)

TLBR/TLBWI/TLBWR/TLBP now control a real 32-entry paired-page TLB. The CPU
translates fetch/load/store/merge/CACHE access with seven page sizes,
ASID/global/valid/dirty checks, 32/64-bit region privileges, ERL bypass, xkphys
CCA, masked CP0 fields and DMFC0/DMTC0. Translation faults record
BadVAddr/EntryHi/Context/XContext and use refill/general/extended vectors.
Virtually indexed cache aliases retain separate stale data until maintenance.

The [original ROM](tests/roms-src/n64/tlb_translation/README.md) matches
**114,688/114,688** cases / **1,261,568** guest register values. An executed
ASID mutation fails all 256 reduced rows; eleven field mutations fail; seven
malformed streams are rejected. The component checks **3,670,016** ASID/global/
half cases, **2,097,152** physical frame selections, **79,188** offsets and
**4,096** mask inputs and **229,376** independent permission/CCA cases, plus
controls, CPU faults/fetch/cache aliases and state
continuation. S011 stores 32 entries in 1,024 bytes and accepts S010 with neutral
TLB entries. Old state conversions preserve their existing fields.

Duplicate-hit shutdown/TS behavior, physical timing and CP0/TLB hazards remain
pending. Neutral reset tags, rejection of undefined masks and RI on duplicate
translation matches are explicit emulator conventions. Unsupported physical
bus access produces IBE/DBE in this bus profile; physical open-bus behavior is
unverified. This cartridge has not been tested on a physical console.
N64 remains **35%** on the delivery checklist; full hardware completion is unknown.

Mario 64 now passes TLBWI and reaches unimplemented VI Current at `A4400010`
after **5,897,464** attempts (EPC `803275F4`, actual decoded/backing word
`8D8D0010`, Cause `1C` hex / DBE). The tracer distinguishes actual CPU exception
entry from software EXL writes and permits nonterminal guest-handled exception
classes. The production probe still sees uniform video and zero retail PCM;
boot/playability remain unverified. Commercial bytes are kept out of git.

All **28/28 N64 CTests pass** (328.64 seconds), including all original N64
register ROMs, PCM ABI oracle and ten component tests. The final DMCP0
32-bit-mode restriction and independent permission checks are verified by the
affected TLB gates and the updated component sanitizer run. All **66** original
ROMs reproduce from their source/image ledger. CI/CD remains disabled.

## Video Interface and first framebuffer

The N64 core now exposes functional VI registers, scanline/field progression,
five-field H_LEAP selection, CURRENT acknowledgement and VI → MI → CPU interrupts.
The NTSC crystal advances at 48,681,818 / 93,750,000 per nominal CPU attempt;
physical CPI, DMA arbitration and sub-line sync timing remain unverified.
Odd V_TOTAL preserves field parity; even V_TOTAL alternates parity. Interrupt
comparison includes the documented odd-field early-match and V_INTR=0 behavior.
TYPE 0 holds CURRENT at zero; reserved TYPE 1 uses an emulator blank convention.

The production ABI derives an **unfiltered RGBA16/RGBA32 framebuffer** from RAM,
ORIGIN, WIDTH, visible spans and fixed-point scale/offset fields. Dirty CPU data
cache remains invisible until writeback. Host observation leaves guest state
unchanged. Host alpha is opaque because source low bits represent coverage.
Inactive/invalid visible spans return a 320×240 black framebuffer; unavailable
source pixels are black. Active dimensions follow the programmed visible spans.
Y offset's upper two stored bits do not affect rendering. Rendering is a snapshot,
not accumulated physical scanout. Antialiasing/divot/dither/gamma, coverage filters,
TEST_MODE staged data, pixel advance, extreme-scale errata, mid-frame changes,
RSP/RDP rendering and physical timing remain pending.

The [register ROM](tests/roms-src/n64/vi_registers/README.md) matches **65,536
rows / 983,040 guest values**. The [framebuffer ROM](tests/roms-src/n64/vi_framebuffer/README.md)
matches **24 guest values** and **131,072 actual production ABI pixels** across
two formats. Executed mutations and strict file/channel/framing controls reject
incorrect results. The component checks all **16,777,216 ORIGIN values**, all
**1,048,576 vertical-total/interrupt-target sequences** over multiple fields,
**4,096 horizontal periods**, all **65,536 RGBA16 words**, register bits, leap
selection, fractional scaling, memory/cache bounds, IRQ routing and save replay.
These are declared finite domains, not their full independent product or physical
hardware certification. No physical-console run has validated these cartridges.

`VN64S012` appends 160 validated VI state bytes. Legacy S011 restores reset VI;
earlier conversions retain their documented neutral state. Phase, field, leap
pattern and IRQ continuation replay exactly; malformed states reject atomically.
N64 is now **40% (8/20 delivery milestones)**: guest framebuffer output earns the
graphics milestone. This is not a hardware-accuracy or retail-compatibility score.

The private Super Mario 64 USA trace now reaches **6,212,303 attempts**, then
unsupported `LDC1`: EPC `80325978`, actual instruction-cache word `D7400130`,
Cause `10000028` (RI code 10). The backing word `44844000` differs because the
cache is noncoherent; the diagnostic records both. The trace passed 22 earlier
nonterminal exception entries. The 120-quantum production probe still observes
uniform video and zero audio pairs. Boot and playability remain unverified;
commercial ROM bytes are never committed. Earlier increment reports below/above
retain their historical blocker and percentage at the time of that increment.

Rules follow the [VI register reference](https://n64brew.dev/wiki/Video_Interface),
[original Nintendo VI manual](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm),
and [hardware timing reference](https://github.com/meauxdal/N64-Refresh-Rate-Reference/blob/main/N64_Timing_Reference.md).

Verification: **32/32 N64 CTests passed** in 668.56 seconds, including all original register ROMs, production PCM/video oracles and eleven component gates. The full build and VI undefined-behavior sanitizer passed; all **68 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU register and memory data transfers

The core implements all eight FPU data-transfer instructions:
MFC1/MTC1/DMFC1/DMTC1 and LWC1/LDC1/SWC1/SDC1. FR=0 pairs low words of even
registers with high words addressed by odd register numbers; FR=1 exposes 32
independent 64-bit registers. MFC1 sign-extends its word, MTC1 ignores high GPR
bits, and data movement preserves payload bits without interpreting NaNs or
changing FCR31. Doubleword moves are valid in 32-bit kernel mode and 64-bit
user/supervisor mode. Memory transfers use the CPU's existing translation/cache
path and enforce aligned 4-/8-byte accesses. CU1 unusable takes priority over
operand decoding or data access; failed loads leave FPR state untouched and
faulted stores preserve backing memory. EPC/BD, nested and BEV context are tested.

The [original register ROM](tests/roms-src/n64/fpu_data/README.md) matches
**131,072 cases / 1,179,648 actual 64-bit values**. Its independent host oracle
rejects an executed capture mutant, nine field mutants and seven malformed-stream
controls. The component checks **4,194,304 word-register/payload cases** and
**3,145,728 defined doubleword-register/payload cases**, **262,144 signed-offset
cases**, all GPR indices/zero, all 64 data-bit positions in every FR=1 register,
reserved transfer fields, privilege/CU1, alignment/TLB/bus exceptions and state
continuation. These are declared finite domains, not full independent products or
a complete FPU/hardware corpus. No physical-console run has validated this ROM.

`VN64S013` appends 32 raw 64-bit FPRs (256 bytes). S012 and earlier versions
restore zero FPR storage. Existing cache/VI/AI/TLB state and legacy conversions
retain their documented behavior. Undefined FR=1 upper word-write bits are
preserved; undefined odd FR=0 doubleword forms raise RI as deterministic emulator
conventions. Raw storage is retained across FR view changes, with physical
mode-transition behavior ungraded. Transfer pipeline hazards, physical timing,
reverse-endian mode and FPR arithmetic/conversion/comparison remain pending.
Rules follow the [NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf),
sections 7.2, 17.3 and the corresponding instruction entries.

Super Mario 64 USA now passes LDC1 and reaches **6,212,336 startup attempts**,
then unsupported `CVT.S.W` at EPC `80325984`, actual instruction-cache and backing
word `468042A0`, Cause `10000028` (RI code 10). It passed 22 earlier nonterminal
exception entries. The 120-quantum production probe still observes uniform video
and zero audio pairs; boot/playability remain unverified. Commercial ROM bytes
are never committed. Earlier increment records retain their historical blockers.
N64 remains **40% (8/20 delivery milestones)**; the whole CPU milestone is pending.

Verification: **34/34 N64 CTests passed** in 437.02 seconds, including twenty original register-ROM gates, two production audio/video oracles and twelve component gates. The full build and final FPU-data undefined-behavior sanitizer passed; all **69 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU conversion and fixed rounding family

The core now implements CVT.S/D/W/L and ROUND/TRUNC/CEIL/FLOOR W/L using integer
bits, without host floating-point state. It supports nearest-even, toward-zero,
positive and negative directed rounding; fixed-mode instructions preserve FCSR
rounding controls. FCSR causes replace previous causes, sticky flags accumulate
only on nontrapping results, and enabled exceptions/software assist suppress
writes while preserving sources and old flags. FR views, long-target privilege,
CU1 precedence, delay/nested/BEV exceptions and S013 state replay are tested.

The [integer ROM](tests/roms-src/n64/fpu_integer_conversions/README.md) matches
**524,288 rows / 2,621,440 captured words**. The
[edge ROM](tests/roms-src/n64/fpu_conversion_edges/README.md) matches
**289,536 rows / 1,447,680 captured words**, including every S/D exponent field
on four fraction patterns and both signs, all conversion functions, selected full
enable/FS/rounding products, integer bit boundaries and reserved source formats.
Independent exact-rational host grading checks actual CPU/FPR/FCSR captures;
undefined upper result bits are recorded and ungraded. Executed capture mutants,
five field mutations per ROM and strict malformed-stream controls reject incorrect
results. The component checks **4,194,304 exact integer/index cases**, **229,376
exception-control combinations**, **98,304 register-pair/FR/format/function cases**,
fixed modes, CU1/privilege, cause replacement, exception context and state replay.

Rules follow the [NEC manual](https://hack64.net/docs/VR43XX.pdf), chapters 7–8,
conversion instruction entries and Appendix D, with ambiguities resolved by the
hardware-test author's [conversion cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs)
and [raw constants](https://github.com/lemmy-64/n64-systemtest/blob/main/src/cop1.rs).
Subnormal inputs require assist. Floating precision conversion preserves infinity;
NaNs with top payload bit 1 raise invalid and return a canonical legacy value,
while top-bit-0 NaNs require assist. Integer conversions reject both NaN types,
infinity and out-of-range results with assist. Long inputs support [-2^55, 2^55);
long outputs require magnitude below 2^53. Tiny results use FS/enable/rounding
rules, including rounding to the minimum normal and signed zero.

No physical console has validated these original cartridges. Their selected finite
domains do not exhaust the full independent operand product or whole FPU.
Undefined odd FR=0 arithmetic operands retain the RI convention; nonzero unary
`ft` encodings use an assist convention with physical behavior ungraded. Pipeline
hazards, precise CE timing, reverse-endian mode and FPU arithmetic/comparison
remain pending. State stays `VN64S013`; conversions add no serialized fields.
N64 remains **40% (8/20 delivery milestones)**; the whole CPU milestone is pending.

Super Mario 64 USA now passes CVT.S.W and reaches **6,212,339 startup attempts**,
then unsupported `DIV.S` at EPC `803259A0`, actual instruction-cache/backing word
`460A3483`, Cause `10000028` (RI code 10). It passed 22 earlier nonterminal exception
entries. The 120-quantum production probe still sees uniform video and zero audio
pairs; boot/playability remain unverified. Commercial ROM bytes are never committed.
Earlier increment reports retain their historical blockers and counts.

Verification: **37/37 N64 CTests passed** in 475.97 seconds, including twenty-two original register-ROM gates, two production audio/video oracles and thirteen component gates. The full build and final conversion undefined-behavior sanitizer passed; all **71 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU arithmetic family

The core implements ADD/SUB/MUL/DIV/SQRT/ABS/MOV/NEG in single and double
precision using portable integer significands, guard/sticky rounding, long
division and a restoring integer square root. It uses no native floating-point
state or compiler-specific 128-bit type. All four rounding modes, legacy NaNs,
signed zeros, infinity, invalid, division-by-zero, overflow and FS/enable-dependent
tiny results are handled. Assist takes precedence over invalid operands. MOV
copies bits without interpreting NaNs/subnormals or changing existing FCSR causes.
Traps update causes, preserve sticky flags and suppress destination writes.

The [pair ROM](tests/roms-src/n64/fpu_arithmetic_pairs/README.md) matches
**131,072 rows / 786,432 captured words**. The
[edge ROM](tests/roms-src/n64/fpu_arithmetic_edges/README.md) matches
**215,152 rows / 1,290,912 captured words**. Both stream six actual destination,
FCSR, handler, CPU exception and unchanged-source observations per row. An
independent exact-rational oracle grades arithmetic; square roots use integer
square comparisons, without native FP. Each ROM rejects an executed capture
mutant, six individual field mutations and seven malformed-stream controls.

The component passes **540,672 register/FR/format/function cases** and **327,680
rounding/enable/sticky/FS/condition combinations**, including aliased operands,
MOV preservation of enabled old causes, minimum-normal rounding rescue, CU1
precedence, reserved formats,
delay/nested/BEV exceptions and exact pending-arithmetic state replay. A separate
30,000-case seeded raw-bit differential check matches the rational oracle.
State stays `VN64S013`; this family adds no serialized fields.

Rules follow the [NEC manual](https://hack64.net/docs/VR43XX.pdf), chapters 7–8
and arithmetic instruction entries, supplemented by the hardware-test author's
[raw-bit arithmetic cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs).
These original cartridges have not been validated on a physical console. Their
finite domains do not exhaust all independent operand combinations. Undefined
odd FR=0 operands use the documented RI convention; nonzero unary ft fields use
an assist convention whose physical behavior is ungraded. FPU comparisons,
pipeline/interlock/CPI behavior, precise CE timing and reverse endian remain pending.
N64 stays **40% (8/20 delivery milestones)**; the whole CPU milestone is incomplete.

Super Mario 64 USA passes the former DIV.S blocker and reaches **12,000,000
startup attempts**, the bounded trace limit, without a terminal CPU fault.
It passed 51 nonterminal exception entries and ends at PC `80246DDC`, Status
`FF01`. The 120-quantum production probe observes uniform video and 480 audio
sample pairs; audible game sound, boot and playability remain unverified.
Commercial ROM bytes are never committed. Earlier increment reports retain
their historical counts and blockers.

Verification: **40/40 N64 CTests passed** in 246.85 seconds, including twenty-four original register-ROM gates, two production audio/video oracles and fourteen component gates. The full build and final arithmetic undefined-behavior sanitizer passed; all **73 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## RSP scalar execution

The RSP now executes 32-bit scalar arithmetic, logic, comparisons, immediate and
variable shifts, conditional branches/link/jumps with delay slots, byte/halfword/
word DMEM loads/stores and shared MFC0/MTC0 SP fields. IMEM PC/targets wrap at
4 KiB; unaligned data accesses wrap each byte within DMEM. ADD/SUB/ADDI wrap
without CPU overflow traps. BREAK sets HALT/BROKE and the configured SP interrupt;
MI delivers that real interrupt to the CPU. Semaphore and settled DMA use the
same SP interface as CPU accesses. Single-step NOPs settle HALT.

RSP instructions advance at two per three nominal CPU interpreter attempts;
this is a functional scheduling approximation, not physical CPI, dual issue,
interlocks, bus timing or debugger pipeline behavior. Missing vector/DP operations
halt with an explicit blocked instruction and retained PC, without manufacturing
BROKE, a task-completion IRQ or a CPU hardware exception. Illegal/undefined
encodings and branches within delay slots are not physically graded.

The [ALU cartridge](tests/roms-src/n64/rsp_scalar_alu/README.md) matches
**704,512 rows / 4,931,584 actual captured words**, including all repeated-byte
pairs for ten functions and all fixed/variable shifts on 256 patterns. The
[immediate cartridge](tests/roms-src/n64/rsp_scalar_immediates/README.md) matches
**1,572,864 rows / 11,010,048 words**: every immediate for eight forms on three
operand patterns. Original RSP code writes actual GPRs to DMEM, and original CPU
code streams those values plus zero, SP PC/status and MI pending. A separate
mathematical host oracle grades every word. Each suite rejects an executed capture
mutant, seven field mutations and seven malformed-stream controls.

The component passes **4,194,304 immediate cases**, **327,680 register triples**,
**9,437,184 byte-address/pattern/load-store cases**, **262,144 branch/BREAK cases**,
all low-byte shift counts and all PC slots for J/JAL/JALR. It covers alias/zero
semantics, SP control/semaphore/real DMA bridges, CPU IRQ acknowledgment, explicit
unsupported-operation diagnostics, reset and state atomicity/replay.
The earlier SP-control cartridge now waits for single-step NOPs to settle and
halts before PC write/read to avoid execution races; all 196,608 rows still grade.

State `VN64S014` appends 304 bytes: 32 scalar GPRs, next PC, retired count, delay
state, blocked flag/word and the fractional clock phase. S013 and older states
preserve implemented fields while starting neutral scalar registers/phase and
sequential execution from the restored SP PC. Existing memory/debug prefix
positions stay stable. RSP state rejection is atomic, including invalid zero GPR,
word widths, alignment, flags and phase. Pending branch/BREAK replays exactly.

Rules follow the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
and the hardware-test author's [scalar cases](https://github.com/lemmy-64/n64-systemtest/tree/main/src/tests/rsp).
Published raw cases clarify SLTIU unsigned comparison, LWU as a word load,
unaligned DMEM wrapping, unconditional REGIMM links and BREAK in delay slots.
These original cartridges have not been validated on a physical console.
Selected domains do not exhaust all independent 32-bit operands or the whole RSP.

The local Mario trace reaches 12,000,000 CPU attempts with 51 nonterminal
exception entries and no terminal CPU fault. It retires **52 RSP scalar
instructions**, then blocks at **SP PC `090`, `MFC0 DPC_STATUS`, word `40055800`**.
Status `41` records HALT plus interrupt-on-break, without BROKE. The production
probe still observes uniform video and 480 audio sample pairs, which do not prove
audible game sound. Boot/gameplay remain unverified; commercial bytes are excluded.
N64 remains **40% (8/20 delivery milestones)**. Vector/RDP/game rendering, RSP
audio microcode, physical timing, FPU comparisons, analog and netplay remain pending.

Verification: **43/43 N64 CTests passed** in 325.15 seconds, including twenty-six original register-ROM gates, two production audio/video oracles and fifteen component gates. The corrected RSP ROM/component gates also passed in 78.34 seconds. The full build and final scalar undefined-behavior sanitizer passed; all **75 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and GitHub Actions disabled.
