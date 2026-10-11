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
* **`serial` (Game Boy).** Deprecated alias of `file`: the GB channel module
  forwards actual serial bytes and emits file adapters for mooneye, A000,
  title-armed HRAM and Blargg text. Explicit serial streams own their verdicts.
* **`serial` (GBA sub-variant).** Deprecated alias of `file`: the GBA plugin
  emits `mgba,r12` captures. R12 inference requires an actual ARM/Thumb
  self-branch; recognized legacy macro epilogues select the evaluated register.
  Strict owned tests independently grade raw register observations.
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

## N64 native VI scheduling and S030

New native sessions advance at functional VI field endings; legacy states and
homebrew diagnostics keep their old 100k CPU budget. S030 saves that choice
without growing the state. Reset preserves it, so legacy movie reset events
keep the same schedule. The original `n64_vi_field_schedule` integration gate
checks 5,160 native records / 46,440 output words, executed wrong-register and
old-budget controls, state compatibility and bounded startup/termination.
Private `tests/n64_retail_vi.py` verifies SM64 courtyard input and optional
VI movie artifacts without archiving private bytes. Full playability, physical
timing and practical normal-speed play remain unverified. See the N64 core
README and original cartridge README for exact coverage and reproduction.

## N64 original integer traps

`n64_integer_traps` grades an actual production plugin RAM/CP0 stream from an
original cartridge: 15,728,640 cases / 188,743,680 words, all immediate/software
code fields over the documented operand patterns. Guest handlers capture raw
state and ERET; the Python oracle owns every verdict. The 15,360-case executed
opposite-condition control, twelve field faults and seven damaged/order streams
must fail. `n64_integer_trap_test` adds 37,810,176 native component observations
including defined 32-bit operands, source aliases, branch annul and nested/BEV
behavior. Full output is about 1.51 GB, SHA-checked with bounded memory. See
`roms-src/n64/integer_traps/README.md`; LL/SC, full CPU and physical timing remain.


## 2026-10-11: N64 linked memory

N64 0.48.0 implements LL/LLD and SC/SCD with ERET link clearing and S031
live-link checkpoint persistence. The original cartridge grades 1,048,576 cases /
10,485,760 actual RAM words over every signed offset, both widths, four input
patterns and successful/ERET failure paths. Executed missing-ERET controls and
strict result/stream faults are rejected independently. Component checks cover
registers, privilege, faults and diagnostic LLAddr behavior. Native complete-state
rollback and S030 imports are verified. See the core and original cartridge
READMEs for commands and exact scope. Cache interference is a conservative
functional model; physical interference, complete CPU/timing and whole-title
retail playability remain unverified. N64 stays 45%; CI/CD stays disabled.

All **131 original archives** reproduce; private SM64 VI control continuations
pass on the current build. Completion scores are unchanged.

Private SM64 production movie verification on **0.48.0** also passes: **121 native
VI frames**, complete ordinary/record/play states and **six exact seek boundaries**;
an executed neutral-input movie differs. Production two-peer TCP netplay passes.


## 2026-10-11: N64 disabled COP2 exception priority

N64 **0.49.0** now raises coprocessor unusable (CpU=11, CE=2) for disabled
COP2 and its word/doubleword loads/stores, before reserved suboperations or data
address/translation errors. The component gate covers all 2^26 payloads for all
five major opcodes plus metadata contexts: **335,585,280 observations**, with
unchanged operands/HI/LO/BadVAddr and no data-bus accesses.

The original [COP2 cartridge](roms-src/n64/cop2_unusable/README.md)
exports **9,056,256 cases / 108,675,072 actual RAM/CP0 words**. The native domain
includes every memory offset over sixteen address patterns, all move register
encodings, all branch offsets and low sixteen operation-function bits, in ordinary
and actual taken-delay contexts. Its independent oracle checks raw handler
metadata and ERET; executed COP1 substitution, field/stream faults and full-state
ordinary/fast rollback are checked. See the cartridge README for exact exclusions.

Enabled expansion-COP2 behavior remains unsupported with the existing RI
convention; COP2 is not the RSP vector unit. Physical exception/pipeline timing,
complete CPU/device behavior and full-title playability remain unfinished.
S031/state capacity and fixed completion scores are unchanged: **N64 stays 45%**.
CI/CD remains disabled. Rules follow NEC VR4300 §6.4.3 and §6.4.12.

Verification: **11 selected gates pass** (two COP2 gates, eight machine/integer/
linked-memory/native-checkpoint/TAS regressions, and production two-peer TCP
netplay). Private SM64 native VI walking/jumping/punching and stationary controls
pass on 0.49.0. All **132 original archives** reproduce; completion renderer and
whitespace checks pass. The full Release build passes.


## 2026-10-11: N64 CP0 physical Watch exceptions

N64 **0.50.0** implements physical eight-byte WatchLo address comparison, R/W
selection, EXL masking, Watch code 23 and register reserved-bit/readback masks.
Alignment/translation precede Watch; Watch precedes data bus access. Instruction
fetch and CACHE are excluded. Scalar, merge, FP and linked accesses share the
check. The existing S031 CP0 fields preserve an armed WatchLo without changing
format or the 9,527,616-byte capacity.

The original [Watch cartridge](roms-src/n64/watch_exceptions/README.md)
exports **4,194,816 cases / 58,727,424 RAM/CP0 words**: every eight-byte granule of
8 MiB RDRAM, read/write and ordinary/actual taken-delay contexts, plus 512
R/W/lane/matching/EXL controls with executed untrapped memory readback. Its armed
checkpoint compares twelve ordinary/rollback records and full final states.
Executed wrong-exception code, eight result faults and six malformed streams
are rejected independently.

The component domain covers every 2^29 granule of the full 32-bit physical
address space at read and write senses. Supplementary width/lane/flags/EXL,
register transfer, translated alias, bus priority and metadata checks use fixed
operand patterns. This is not all watch/target-address pairs or numerical values.
Nonzero WatchHi matching, failed conditional-store watches, physical/deferred
exception behavior, reverse endian and malformed legacy CP0 fields remain
unverified. Complete CPU/device behavior and full-title retail playability stay
unfinished; **N64 remains 45%** of fixed delivery milestones. CI/CD stays disabled.
See the cartridge README for commands and scope; rules follow NEC VR4300 §6.3.8,
§6.4.3 and §6.4.17.

Verification: **14 selected gates pass** (two Watch gates, eleven machine/cache/
integer/COP2/linked-memory/native checkpoint/TAS regressions and production
two-peer TCP netplay). The Watch component gate executes **1,073,745,425
observations**. Private SM64 native VI walking/jumping/punching and stationary
controls pass on 0.50.0. All **133 original archives** reproduce; full Release
build, completion renderer and whitespace checks pass.


## Four ordinary local N64 frontend ports (2026-10-11)

Run `ctest --test-dir build -R '^(input_hotplug_test|n64_local_ports_test)$' --output-on-failure`.
The frontend gate uses four identical SDL virtual gamepads, the production
Application/default input plugin and archived original `controller_netplay` ROM.
Its independent scenario/wire-bit oracle grades 194 cases / 1,164 real guest
RAM words: all 24 assignments; 14 controls on all four ports; signed-stick
endpoints/dead zone; executed neutral/capture controls. An additional actual
swapped-device assignment is rejected at two port replies. Six physical four-port
TAS frames match ordinary/recorded/playback complete states and three seeks;
an executed player-four movie mutation differs. Hotplug tests cover ownership
moves, ambiguous and unique identity restoration, capture and disconnected ports.

Artifacts: `build/artifacts/n64_local_ports/{local-ports.csv,local-ports.json,executed-wrong-route.json,provenance.json,frontend.log,four-local-ports.tas}`.
Before/after SHA-256 checks bind capture to unchanged binaries and original ROM.
No private ROMs or artifacts are committed. All 11 selected input/controller/
state/movie/TCP regression gates and the final four-port gate pass; full Release
build, completion renderer and whitespace checks pass. Physical USB/Bluetooth,
independent secondary remapping and full native accessories remain pending.
This completes native local routing, moving N64 to 50% of the fixed delivery
ledger; other scores remain unchanged. Actions stays disabled; no CI/CD files.

## Independent secondary controller profiles (2026-10-11)

`n64_local_ports_test` additionally grades 3,026 cases / 18,156 original guest
SI RAM words: 1,008 released/held button-remap cases, 2,016 axis/polarity/threshold
cases and two persisted-map/device-move cases. Actual SDL trigger/stick values
are checked before guest execution; the Python oracle independently builds the
scenario order and native reply bits. An executed wrong profile is rejected.
Six remapped physical four-port TAS frames match canonical inputs and complete
ordinary/recorded/playback states, with three exact seeks and a differing actual
player-four movie mutant. `input_hotplug_test` verifies isolated saved/cleared
maps, unique and ambiguous identity, old-file migration, current-platform
startup defaults and seven malformed profiles rejected before live mutations.
Artifacts add `player-profiles.csv`, `player-profiles.json` and
`executed-wrong-profile.json` to the existing ignored N64 frontend directory.
No private ROM, capture or movie is committed; the original archived cartridge
is reused. Analog remapping and physical USB/Bluetooth validation remain pending.
Delivery percentages are unchanged (N64 50%); no CI/CD is added.

Final verification: 11/11 selected input/controller/state/movie/TCP gates pass,
including the expanded frontend profile corpus. Full Release build, completion
renderer and whitespace checks pass; original archived images are unchanged.

## Original N64 integer mode restrictions (2026-10-11)

Run `ctest --test-dir build -R '^n64_doubleword_privilege(_test)?$' --output-on-failure`.
The [original cartridge](roms-src/n64/doubleword_privilege/README.md) grades
5,768,792 real mapped guest cases / 69,225,504 RAM/CP0 words for 28 doubleword
integer/LWU families. The component domain checks 37,186,848 encoding, mode,
exception metadata, operand preservation and no-data-access observations. The
actual NOP mutant fails every 3,032 short case; result/sequence and malformed
streams are rejected. Twelve records and complete states match native fast
rollback. Artifacts remain in `build/artifacts/owned/n64_doubleword_privilege/`.

Fourteen selected CPU/input/controller/state/movie/TCP gates pass, combining the
13 passing regression gates with the fresh machine gate after its historical
one-port assertion was updated to the verified four-port layout. Private SM64 VI
controls pass on 0.52.0; all 134 original archives reproduce. Full Release build,
completion renderer and whitespace checks pass. Non-linked 32-bit address
normalization, remaining CP0/COP1 rights, physical timing and complete hardware
remain pending. N64 stays 50%; no CI/CD is added.

## N64 Kernel32 wide integer addresses

```sh
ctest --test-dir build -R '^(n64_kernel32_addresses|n64_kernel32_address_test)$' --output-on-failure
```

The original cached-data cartridge grades 4,718,592 cases / 56,623,104 actual
RAM/CP0 words: all immediates of nine wide integer memory families, eight lanes,
four Kernel32/Kernel64 upper-base profiles and direct/taken-delay contexts.
The independent Python oracle checks load/merge/store results, SCD success,
alignment/address faults and preserved operands/EPC/BD/BadVAddr. Real NOP
substitution is rejected in all 1,152 short cases; twelve field and six stream
mutants reject. Twelve ordinary/native fast rollback records and complete
states match. The component gate checks 4,730,235 address/mode/bus observations,
including carry/segment boundaries and ordinary word/FP invalid-address controls.
See [source, artifact paths and limits](roms-src/n64/kernel32_addresses/README.md).

On 0.53.0 all 15 selected CPU/machine/cache/TLB/FPU/controller/frontend/TAS/TCP
regression gates pass. Private SM64 native VI movement/action/neutral controls
pass; all 135 original archives reproduce. Full Release build, completion
renderer and whitespace checks pass. Full hardware and retail playability
remain unfinished; N64 remains 50%.

## N64 CP0 read-only register writes

```sh
ctest --test-dir build -R '^(n64_cp0_readonly|n64_cp0_readonly_test)$' --output-on-failure
```

The original cartridge grades 12,582,912 cases / 150,994,944 actual RAM/register
words: two CP0 write widths, BadVAddr/PRId/CacheErr, four complete sixteen-bit
source positions, four surrounding patterns and direct/taken-delay placement.
A real fault and ERET seed BadVAddr. Independent grading checks word/full reads,
all three register values, source and Status. An executed wrong CP0 read rejects
in all 3,072 short cases, twelve field and six malformed stream faults reject,
and twelve ordinary/fast rollback records and complete states match. The
component gate covers 22,020,097 source/mode/metadata observations, including
CU0-before-width priority, source allocation and hardware BadVAddr updates.
[source, file evidence and limits](roms-src/n64/cp0_readonly/README.md).

On 0.54.0 all 16 selected CPU/machine/cache/TLB/FPU/controller/frontend/TAS/TCP
regression gates pass. Private SM64 native VI movement/action/neutral controls
pass; all 136 original archives reproduce. Full Release build, completion
renderer and whitespace checks pass. N64 remains 50%; complete CPU/hardware
and full-title retail playability remain unfinished.

## N64 Config fields and actual K0 cache/bypass reads

```sh
ctest --test-dir build -R '^(n64_config_fields|n64_config_fields_test|n64_cache_coherence|n64_cache_test)$' --output-on-failure
```

The original Config cartridge grades 16,777,216 cases / 201,326,592 actual
RAM/register words with EP=0/BE=1 held. An independent oracle checks both CP0
write widths, selected source domains, CU/K0 settings, direct/taken-delay
contexts and actual cached/bypass reads around an uncached memory mutation.
Executed wrong reads reject in all 4,096 short cases; twelve field and six stream
controls reject. Twelve ordinary/fast rollback records and complete states match.
The component gate checks 7,356,417 field/source/privilege observations. The older
cache-coherence ROM preserves big endian mode when enabling caching. See
[source, file paths and limits](roms-src/n64/config_fields/README.md).

On 0.55.0 all 20 selected gates pass: 18 CPU/machine/cache/controller/frontend/
TAS/TCP gates and two fresh PI/original IPL handoff gates. The corrected cache
archive still grades 2,097,152 cases / 12,582,912 guest values and rejects its
executed wrong-writeback control. Private SM64 native VI movement/action/neutral
checks pass; all 137 original archives reproduce. Full Release build, completion
renderer and whitespace checks pass. N64 remains 50%; full hardware and
full-title retail playability remain unfinished.

## N64 Status write fields

```sh
ctest --test-dir build -R '^(n64_status_fields|n64_status_fields_test)$' --output-on-failure
```

The original cartridge grades 4,194,304 cases / 50,331,648 actual RAM/register
words: both writes, selected source fields/surrounds and direct/taken-delay
contexts with EXL held for safe capture. The independent oracle checks word/full
readbacks, previous/restored Status, source and unaffected PRId/Config. Wrong
reads reject in all 1,024 short cases; twelve field and six stream faults reject.
Twelve ordinary/fast rollback records and complete states match. The component
gate checks 14,680,066 unconstrained source/mode/TS observations. Full downstream
shutdown/trace/power/reverse-endian behavior remains pending. See
[source, artifacts and limits](roms-src/n64/status_fields/README.md).

## N64 TagLo/TagHi/PErr fields (0.57.0)

```sh
ctest --test-dir build -R '^(n64_diagnostic_fields|n64_diagnostic_fields_test)$' --output-on-failure
```

The original cartridge grades 12,582,912 actual guest records / 150,994,944 words;
32,505,856 component observations include every physical tag through defined
CACHE store/load consumers. See the [domain, oracle and exclusions](roms-src/n64/diagnostic_fields/README.md).

## Saved analog profiles through actual N64 guest RAM

`n64_local_ports_test` now independently grades another 4,035 stick-profile
cases / 24,210 RAM words. Source/polarity/dead-zone/range controls feed the
ordinary SDL/default-plugin/frontend path into the original SI reply cartridge.
Binary/source/ROM hashes stay fixed during capture. The executed wrong source
rejects; twelve malformed profiles reject, legacy fields migrate, saved maps follow
ports, and remapped sticks replay full movie states. `input_hotplug_test` covers
restart, ambiguous/unique reconnect and player reset isolation.

## ARM11 extension register files (2026-10-11)

3DS 0.3.0 adds twelve ARM signed/unsigned scalar/dual-byte extension/accumulate
forms and four Thumb scalar extensions. The original image independently grades
13,631,488 seven-word snapshots / 95,420,416 register words on both ARM11s.
Selected sixteen-bit value domains, rotations, two flag surrounds and accumulator
seeds are exhaustive within the documented slice. Component coverage is
10,081,152 observations; register aliases/conditions/NZCVQ/GE and pre-v6/profile/
reserved/PC isolation are checked. Executed negative controls, seven field faults,
seven malformed streams and original archive reproduction are required. Three
native checkpoints in the first sweep compare two-frame continuation output and
complete states. Full ARMv6/media/system behavior, timing/MMU/exceptions, firmware/
retail boot and platform peripherals remain pending. 3DS stays 30%. See the
[original source and domain](roms-src/3ds/extend_registers/README.md).

## ARM11 saturation register files (2026-10-11)

3DS 0.4.0 adds ARMv6 SSAT/USAT and SSAT16/USAT16, including every encoded
width, LSL/ASR control and sticky Q. NZCV/GE are unchanged; aliases read sources
before writes. Earlier CPU profiles and PC operands stop diagnostically.
The original dual-ARM11 image independently grades **12,582,912 snapshots /
88,080,384 actual register words**. Scalar inputs cover sign-extended bytes at
all controls; dual inputs sweep each halfword with complemented lane pairs.
This is bounded coverage. The component gate checks **64,101,056 observations**,
including wider selected inputs, conditions, all flag states, aliases and CPU
profile restrictions; the prior decoder fails it. Short guest controls reject
all 132,096 wrong-result/wrong-Q cases and 71,184 wrong-source cases. Seven
field faults and seven malformed streams reject. Three native checkpoints
compare actual continuation text and complete 135,791,096-byte states.
Full CPU/media/system support, timing/MMU/exceptions, firmware/native retail,
graphics/audio and analog/touch/wireless remain pending. No delivery milestone
is newly complete; 3DS remains **30%**. See [source and finite domains](roms-src/3ds/saturation_registers/README.md).

## ARM11 parallel arithmetic and GE selection (2026-10-11)

3DS 0.5.0 adds 36 ARMv6 wrapping/saturating/halving byte/halfword arithmetic
forms and SEL, plus CPSR_s/fs register/immediate writes for GE. Wrapping signed
GE reflects the unwrapped result's sign; unsigned GE reflects carry/no borrow.
Parallel saturation preserves Q; halving keeps the wide arithmetic bit before
discarding bit zero. SEL consumes actual GE and leaves flags unchanged.
The original dual-ARM11 image independently grades **19,922,944 snapshots /
179,306,496 actual register words**: complete byte pairs at each constrained
lane, full left halfwords at two right boundary words, and all 16 SEL masks.
This is bounded coverage, not every independent lane or halfword operand pair.
The component gate checks **32,198,238 observations**, including aliases,
conditions, flags, reserved encodings, profile isolation and GE writes.
Executed wrong-result/Q/GE/SEL controls reject all 2,432 short cases; wrong-source
rejects 2,312. Nine field faults and seven malformed streams reject. Native
checkpoint prefixes compare actual text and complete 135,791,096-byte states.
Full CPU/media/system/MMU/exception behavior, timing, firmware/retail boot,
graphics/audio and native devices remain pending. State format and ABI are
unchanged; 3DS remains **30%**. See [original source and domains](roms-src/3ds/parallel_registers/README.md).
