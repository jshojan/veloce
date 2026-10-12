# N64 performance and equivalent-state benchmark

N64 0.85.0 accelerates the cached kernel idle pair `BEQ $zero,$zero,-1; NOP`.
It bypasses instruction decoding only when both words occupy the same valid
instruction-cache line. Uncached code, privilege faults, cache misses, different
instructions and cache-line crossings keep the normal interpreter path.

The machine groups idle CPU bookkeeping while preserving Count, Compare,
Random, delay slots and interrupt boundaries. With RSP halted and DP commands
settled, inactive clocks advance arithmetically to just before the next VI line,
AI fetch, output sample or RTC second. Event clocks use the ordinary device
path. DP pipeline counters retain their nominal RSP-clock increments. These
changes preserve the existing functional clock model; physical CPI and pipeline
timing remain unfinished. Host acceleration controls and counters are excluded
from save and rollback state.

## Measured SM64 continuation

Three alternating before/after pairs started from the same private US SM64
Bob-omb Battlefield walking checkpoint and advanced 60 VI fields each. Release
builds were used. Every pair ended in a byte-identical 9,529,672-byte native
state, with 94,022,500 CPU attempts and 48,140 observed stereo sample pairs.

| Core | Median fields/second | Median wall seconds for 60 fields |
|---|---:|---:|
| 0.84.0, before acceleration | 8.17 | 7.35 |
| 0.85.0, after acceleration | 12.75 | 4.70 |

The measured speedup was **1.56x (56%)** on this machine. Wall time includes ROM
and checkpoint loading, software scanout observation and final state writing.
It is a core benchmark and does not measure GUI/GPU presentation. Results vary
with host and game scene. This run remains below real time: RSP execution and
software RDP rendering still need performance work.

## RSP vector acceleration (0.86.0)

Builds targeting SSE2 now accelerate twelve vector multiply/accumulate and
twelve ALU/merge operations. The scalar path remains available on other hosts
and for the remaining operations. Source operands are captured before writes
so destination aliasing and element selection retain native behavior. Raw
48-bit accumulators, signed saturation, carry/borrow flags and all serialized
state are preserved. Backend selection is host-only and is not serialized.

Three alternating 60-field pairs compared 0.85.0 against 0.86.0 from the same
private SM64 walking checkpoint:

| Core | Median fields/second | Median wall seconds for 60 fields |
|---|---:|---:|
| 0.85.0, scalar vector arithmetic | 12.61 | 4.76 |
| 0.86.0, SSE2 vector arithmetic | 13.98 | 4.29 |

The measured improvement was **1.11x (10.8%)**, with identical complete native
states, CPU attempts and audio counts. This scene remains below real time.

The acceleration gate compares **2,359,296** complete vector/accumulator/flag/
scalar/DMEM continuations against the retained scalar path. All 65,536 seed values are
covered for 36 instruction families; seeds cycle through sixteen element
selections and four alias arrangements. This is a declared seed corpus, not
every possible operand tuple.
A forced scalar build also passes its 2,304-case fallback smoke gate. Six bounded
original ROM subsets grade **4,240 cases / 77,328 actual register words** through
independent file oracles and reject six executed capture mutants. Existing
hardware multiply, compare and ALU fixtures, netplay and the private course
continuation pass.

```sh
ctest --test-dir build -R '^n64_rsp_(simd_test|scalar_fallback_test|acceleration_roms)$' --output-on-failure
```

## Reproduce with local game data

Keep a copy of the previous core before rebuilding. The benchmark accepts
locally owned game data and a native checkpoint; no private ROM or state is
committed.

```sh
python3 tests/n64_performance.py /path/to/game.z64 \
  --checkpoint /path/to/walking.state \
  --before-core /path/to/before/n64.so \
  --core build/bin/cores/n64.so --fields 60 --runs 3
```

The script records source/binary hashes, individual observations, median
throughput, every paired speedup, the median paired speedup, the number of
faster pairs, and complete-state equivalence in
`build/artifacts/performance/n64/report.json`. It alternates run order and
rejects differences in complete native states, emulated attempts, audio counts
or frame counts. On Linux, `--cpu-affinity N` pins both probe processes to an
allowed CPU and records the mask used. Optional `--min-speedup` grades the
median **paired** ratio, so a ratio of independent medians cannot pass while
most pairs regress. Absolute FPS is not a portable test gate.

The CPU continuation gate covers scalar and batched execution, timer wraparound,
external interrupts on branch/delay boundaries, Wired/Random combinations,
privilege and uncached fallbacks, and instruction-cache invalidation. The full
machine gate compares 128 native continuations across VI, AI, RSP, DP, RTC and
interrupt boundaries. Existing original interrupt and 5,160-case VI scheduling
ROM observations, cache rollback checks, production netplay and the private
SM64 course continuation also pass.

```sh
ctest --test-dir build -R '^n64_(cached_idle|idle_clock)_test$' --output-on-failure
```

N64 stays **50% (10/20)** of the delivery checklist. The frontend and all core
completion documentation use the same ledger. CI/CD remains disabled.

## RGBA16 texture quad acceleration (0.87.0)

Unpaletted RGBA16 quad fetches gather the same four TMEM halfwords and expand
all four RGBA colors with SSE2. Row wrapping, odd-row bank swapping and coupled
neighbor coordinates retain their scalar behavior. Other formats, palettes and
hosts without SSE2 use the retained scalar path. No emulated state changes.

Five alternating 60-field pairs against 0.86.0 measured **3.9% more throughput**:

| Core | Median VI fields/second |
|---|---:|
| 0.86.0 | 13.35 |
| 0.87.0 | 13.88 |

All ten runs produced identical complete native states, 94,022,500 CPU attempts
and 48,140 stereo sample pairs. Results and hashes are in
`build/artifacts/performance/n64-rgba16-final/report.json`. These are local core
throughput measurements; performance remains below the 60-field NTSC target.

The independent expansion-table test checks all 65,536 packed colors in each
of four corners (262,144 colors). Existing independent quad oracles exercise
all formats, palettes, TMEM bases/line strides and coupled row carries; executed
faulty implementations are rejected. The same oracle passes with SIMD disabled.
The original two-cycle filtering ROM, private SM64 course continuation and
production netplay regression also pass.

```sh
ctest --test-dir build -R '^n64_rdp_(rgba16_simd_test|quad_scalar_fallback_test|texel_quad_test|texel_quad_mutants|two_cycle_filter)$' --output-on-failure
```

**N64 remains 50% (10/20)**; full-title playability, physical timing and further
performance work remain pending. CI/CD remains disabled.

## Settled DP clock acceleration (0.88.0)

After each RSP step, a settled DP command stream now advances the same 24-bit
clock and pipeline-busy counters directly. Command fetch, queued START/END,
partial packets, freezes, faults and RSP writes keep the normal DP tick. A
host-only switch selects the original path for differential testing and is
excluded from save and netplay state.

Five alternating 60-field private SM64 checkpoint pairs against 0.87.0 measured
**2.9% more throughput**, from **14.07 to 14.47 VI fields/second** on this
machine. All ten complete native states matched byte for byte; every run
executed 94,022,500 CPU attempts and observed 48,140 stereo sample pairs.
The result is still below the 60-field real-time target. Raw observations and
source hashes are in `build/artifacts/performance/n64-dp-idle-final/report.json`.

A differential gate checks 40,960 individual DP clocks across settled,
queued, frozen, blocked and RSP-written streams, plus 128 complete machine
states. The original DP register ROM and its host assertions, prior inactive
clock boundary gate, production netplay and private SM64 course continuation
also pass.

```sh
ctest --test-dir build -R '^n64_(dp_test|dp_idle_clock_test|idle_clock_test)$' --output-on-failure
```

**N64 remains 50% (10/20)**. Full-title playability, physical timing and
further performance work remain pending. CI/CD remains disabled.

## Active RSP idle-clock batching (0.89.0)

During a cached CPU idle loop, the core now executes every RSP and DP tick while
advancing quiet VI, AI, output-sample and cartridge RTC phases arithmetically.
Each batch stops before their next event. SP/DP interrupt delivery can stop it
earlier. The ordinary event clock handles those boundaries. RSP/DP work and
nominal counters stay in execution order; host controls and diagnostics are
excluded from save and netplay state.

Five alternating 60-field private SM64 checkpoint pairs against 0.88.0 measured
**9.5% more throughput**, from **14.36 to 15.73 VI fields/second** on this
machine. All ten complete native states matched byte for byte, with 94,022,500
CPU attempts and 48,140 observed stereo sample pairs in every run. Raw timings
and hashes are in `build/artifacts/performance/n64-active-batch-final/report.json`.
The core remains below the 60-field real-time target.

The expanded machine test compares 128 complete continuations against both an
unbatched idle clock and the ordinary per-instruction CPU path. Its cases cross
VI, AI, output sample, RTC, active RSP, BREAK and DP FullSync boundaries. Original
VI field scheduling, interrupt, RSP and DP register ROMs, production netplay and
private SM64 course continuation also pass.

```sh
ctest --test-dir build -R '^n64_(idle_clock_test|vi_field_schedule|interrupts|rsp_scalar_test|dp_test)$' --output-on-failure
```

**N64 remains 50% (10/20)**; full-title playability, physical timing and further
performance work remain pending. CI/CD remains disabled.

## Active RSP interrupt polling (0.90.0)

The active-RSP idle batch now checks SP and DP interrupt levels only after a
nominal RSP/DP tick. VI, AI, SI and PI cannot change before the next device
boundary in that batch; initial pending levels are checked before batching.
The ordinary clock still processes every device event, and the full CPU
interrupt check runs at each batch boundary.

Five alternating 60-field private SM64 checkpoint pairs against 0.89.0 measured
**4.0% more throughput**, from **16.46 to 17.12 VI fields/second** on this
machine. All ten complete native states matched byte for byte, with 94,022,500
CPU attempts and 48,140 audio sample pairs each. Raw observations and hashes
are in `build/artifacts/performance/n64-rsp-irq-final/report.json`.
Performance remains below the 60-field real-time target.

The 128-case complete-machine gate compares the optimized path against both
unbatched idle clocks and per-instruction CPU execution. It includes actual
RSP BREAK and DP FullSync interrupt continuations, as well as VI, AI, RTC,
output-sample and timer boundaries. Original interrupt register ROMs,
production netplay and private SM64 course continuation pass.

```sh
ctest --test-dir build -R '^n64_(idle_clock_test|interrupts)$' --output-on-failure
```

**N64 remains 50% (10/20)**. Full-title playability, physical timing and
further performance work remain pending. CI/CD remains disabled.

## Optional N64 profile-guided build (0.91.0)

GCC can train **only the N64 plugin** on locally owned game data and rebuild it
with recorded branch counts. This is opt-in: ordinary builds use `OFF`, and
neither ROM bytes nor generated profiles are committed. The compiled core keeps
the same plugin interface and native state format. A profile trained on one
scene may not speed up every game or host.

Five alternating 60-field private SM64 checkpoint pairs compared a normal
0.90.0 release build with a profile-trained N64 plugin. The local median rose
from **16.83 to 17.77 VI fields/second (5.6%)**. All ten complete native states,
94,022,500 CPU attempts and 48,140 audio sample pairs matched. The exact
profile-trained plugin was reproduced byte for byte after copying profile data
into the ignored local build artifacts. Raw measurements and hashes are in
`build/artifacts/performance/n64-pgo-release-final/report.json`. The result is
still below the 60-field real-time target.

From the repository root, with a ROM and a representative native checkpoint:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DVELOCE_N64_PGO=OFF
cmake --build build --target n64_plugin -j4
mkdir -p build/artifacts/performance/n64-pgo/profile
cp build/bin/cores/n64.so build/artifacts/performance/n64-pgo/before.so
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DVELOCE_N64_PGO=GENERATE \
  -DVELOCE_N64_PGO_DATA="$PWD/build/artifacts/performance/n64-pgo/profile"
cmake --build build --target n64_plugin -j4
build/bin/retail_core_probe build/bin/cores/n64.so "$ROM" 60 - - \
  --scheduling video-field --load-state "$CHECKPOINT" \
  --save-state build/artifacts/performance/n64-pgo/trained.state
cmake -S . -B build -DVELOCE_N64_PGO=USE \
  -DVELOCE_N64_PGO_DATA="$PWD/build/artifacts/performance/n64-pgo/profile"
cmake --build build --target n64_plugin -j4
python3 tests/n64_performance.py "$ROM" --checkpoint "$CHECKPOINT" \
  --before-core build/artifacts/performance/n64-pgo/before.so \
  --runs 5 --fields 60 --artifacts build/artifacts/performance/n64-pgo/check
```

The USE build requires profile data for every N64 translation unit and fails
if it is missing. To return to the ordinary portable build, configure with
`-DVELOCE_N64_PGO=OFF` and rebuild `n64_plugin`. Native save/rollback, original
register ROMs, production netplay and the private SM64 course continuation pass
with the trained plugin. CI/CD remains disabled.

**N64 remains 50% (10/20)**; complete hardware, full-title playability and
physical timing remain unfinished.

## Perspective divider floor shift (0.92.0)

Sampling a 300-field private SM64 Bob-omb Battlefield continuation in an
ordinary optimized build located repeated signed division in the RDP
perspective texture divider. The divider now computes floor division by a
power of two with shifts of nonnegative values. Its identity also holds at
`INT64_MIN`; it does not depend on a compiler's signed-right-shift behavior.

The independent exhaustive divider oracle passed **4,294,967,296 ST/W pairs**
and all **524,288** saturation encodings. Five alternating 60-field runs
measured **1.2%** more throughput, and a separate nine-pair confirmation
measured **3.1%** more throughput (**14.90 to 15.36 VI fields/second**).
Each pair had byte-identical complete native states, CPU attempts and audio
counts. Raw results and source hashes are in the local ignored artifacts
`build/artifacts/performance/n64-floor-shift-{prototype,confirmation}/`.
Five more pairs using the full production build measured **3.2%** more
throughput (**16.65 to 17.19 VI fields/second**) against the prior ordinary
plugin, again with byte-identical complete states. A fresh local PGO profile
trained on 0.92.0 measured an additional **3.8%** over ordinary 0.92.0 in
five pairs (**16.63 to 17.26 VI fields/second**), with identical states.
Profiles from older source must be retrained. Those measurements are in the
ignored `n64-floor-shift-production/` and `n64-floor-shift-pgo/check/`
artifacts. The original perspective texture ROM, private SM64 course
continuation, trained-plugin perspective checkpoint and production netplay
passed. Performance remains below the 60-field real-time target.
The fixed N64 delivery checklist remains **50% (10/20)**. Physical timing
and full-title playability remain unverified.

## Common RDP combiner selectors (0.93.0)

A 60-field private SM64 course checkpoint used two cycle-one selector
patterns for about **86% of its shaded pixels**. The N64 combiner now evaluates
those patterns directly: COMBINED feedback and TEXEL0 RGB multiplied by SHADE
RGB with SHADE alpha. The guard checks only cycle-one selector bits, so other
cycle-zero settings retain the same exact arithmetic. All remaining selectors
keep the general evaluator.

The independent routing oracle checks **525,312 nine-bit common-profile source
cases**, including distinct channel values, special negative encodings and
both cycle-zero variants. The original shaded-texture cartridge grades
**262,144 guest cases / 3,407,872 actual framebuffer and register words**,
with zero mismatches; its executed wrong-capture control is rejected. Five
alternating production-build SM64 checkpoint pairs measured **3.1% more
throughput (17.97 to 18.54 VI fields/second)**, with byte-identical complete
states, CPU attempts and audio counts. The local release build measured **5.4%**
in a separate five-pair run. Raw results are in ignored
`build/artifacts/performance/n64-common-combiner-masked*/` artifacts.
Freshly trained local GCC PGO added another **4.3%** over ordinary 0.93.0 in
five pairs (**18.20 to 18.99 VI fields/second**), again with identical complete
states, CPU attempts and audio counts. Its ignored report is at
`build/artifacts/performance/n64-common-combiner-pgo/check/report.json`.
The private SM64 course continuation, production netplay and trained-plugin
RDP checkpoint pass.
Performance is still below the 60-field real-time target; **N64 remains
50% (10/20)** of the fixed delivery checklist.

## Cached RSP instruction fetch (0.94.0)

The RSP caches a fetched 32-bit IMEM word by aligned PC and a host-only IMEM
generation. CPU IMEM stores and SP DMA into IMEM advance the generation; reset
and state restore discard old entries. This leaves the native state format and
guest timing unchanged. Five alternating 300-field SM64 course-checkpoint
pairs on ordinary optimized builds measured **3.3% more throughput (18.39 to
19.00 VI fields/second)** than 0.93.0. All complete native states, CPU attempt
counts and audio counts matched. Raw results are in ignored
`build/artifacts/performance/n64-rsp-imem-cache-graded/report.json`. The RSP
unit suite, original RSP acceleration ROM corpus, private course continuation
and production netplay pass. The core remains below the 60-field real-time
target and **N64 remains 50% (10/20)**.

## Optional host CPU optimization

`VELOCE_N64_NATIVE_OPTIMIZE=ON` applies `-march=native` only to the N64 plugin
when building locally with GCC or Clang. The default is `OFF`, so normal
packages remain portable. A plugin built with this setting may use CPU
instructions unavailable on another machine; build it where Veloce will run.
The option changes host code generation only, not native state or plugin ABI.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DVELOCE_N64_NATIVE_OPTIMIZE=ON
cmake --build build --target n64_plugin -j4
```

Nine alternating, CPU-pinned 60-field SM64 course-checkpoint pairs on this
machine measured median throughput of **17.35 fields/second** for the portable
release plugin and **18.27 fields/second** for the N64-only native build, a
**5.3%** median-time improvement. Eight of nine individual pairs favored the
native build. Host-load variation was visible; an unpinned five-pair run was
inconclusive. Three longer 300-field pairs measured **18.47 to 18.99
fields/second** by median time, with mixed individual pair results. Every pair
had byte-identical complete native states and identical CPU attempt and audio
sample counts. Ignored reports are at
`build/artifacts/performance/n64-native-option-{pinned,long}/report.json` in
the N64 lane. Original RSP acceleration ROMs, the native-state netplay gate
and production TCP netplay pass with the optimized plugin. Performance remains
below the 60-field target; **N64 remains 50% (10/20)**.

## Table-driven RDP coverage derivatives (0.95.0)

Every eight-bit RDP coverage mask maps to a compile-time packed count and first
covered sample. The independent coverage oracle checks all 256 masks and more
than 134 million edge pairs. Original two-cycle filter and shaded-texture
cartridges pass. Five alternating, CPU-pinned 300-field Super Mario 64
course-checkpoint pairs measured a **1.9% median paired speedup**, with all
five pairs faster. Median throughput was **18.52 to 19.00 VI fields/second**
by run time. Every complete native state and work count matched byte for byte.
The ignored local report is at
`build/artifacts/performance/n64-coverage-table-final/report.json` in the N64
lane. This is a small improvement; the game is still choppy and below the
60-field real-time target. **N64 remains 50% (10/20)**.

## Predecoded RDP combiner cycles (0.96.0)

The triangle renderer now decodes both RDP combiner cycles once per draw and
passes them to the pixel evaluator. This preserves the existing source routing
and common selector arithmetic while avoiding repeated cycle field extraction
for every shaded pixel. Five alternating, CPU pinned 60-field Super Mario 64
course checkpoint pairs measured a **1.1% median paired speedup**, with all
five pairs faster. Median throughput was **18.59 to 18.80 VI fields/second**;
every pair had byte identical complete native states and identical CPU attempt
and audio sample counts. The local ignored report is at
`build/artifacts/performance/n64-predecoded-cycle/report.json` in the N64 lane.
Combiner routing, math, triangle texture and machine component gates pass. Play remains below the 60-field
real-time target. **N64 remains 50% (10/20)**.

## Reused draw invariants (0.97.0)

The triangle renderer now computes the cycle-one second-tile decision and
texture conversion coefficients and weights once per draw. The same values
were previously recomputed for every shaded pixel. Three alternating,
CPU-pinned 300-field Super Mario 64 course-checkpoint pairs measured a
**1.0% median paired speedup**; two of three pairs were faster. Median
throughput was **19.00 to 19.14 VI fields/second**. Complete native states,
CPU attempt counts and audio sample counts matched in every pair. The
original two-cycle filter cartridge and combiner, conversion, texture filter
and triangle texture component gates pass. The local ignored report is at
`build/artifacts/performance/n64-draw-invariants/report.json` in the N64
lane. This is a small gain; N64 remains below the 60-field real-time target
and **50% (10/20)** on the delivery checklist.

## Exact RGBA16 triangular filter route (0.99.0)

The SM64 course checkpoint sends about 6.17 million of 6.48 million shaded
pixel visits through textured triangular filtering; about 6.14 million use
RGBA16 tiles. For RGBA16 without the optional middle-pixel mode, the renderer
now evaluates the same signed, rounded triangle weights directly. YUV,
conversion and other modes keep the general filter.

An independent component oracle checks the new route over **67,108,864
eight-bit corner-pair and 5-bit S/T fraction combinations**. Original
one-cycle textured-triangle, two-cycle filter and two-cycle shaded-texture
cartridges pass their guest framebuffer/register oracles. Three alternating,
CPU-pinned 300-field SM64 course-checkpoint pairs measured **18.94 to 19.35
VI fields/second** by median time, a **2.1% throughput gain**; all three
pairs were faster and complete native states, CPU attempts and audio counts
matched byte for byte. The ignored local report is at
`build/artifacts/performance/n64-rgba16-triangle-filter/report.json` in the
N64 lane. This remains well below the 60-field real-time target, and N64
remains **50% (10/20)** on the delivery checklist.

## Contiguous RSP vector-memory transfers (0.100.0)

For RSP vector loads and stores whose bytes stay inside both DMEM and the
selected vector register, the core now copies the contiguous range directly.
Transfers that cross the 4 KiB DMEM wrap or the 16-byte vector boundary still
use the per-byte mapping. This changes host work only; the guest instruction,
register and memory results remain the same.

The original load and store cartridges each grade **786,432 guest cases**;
together their independent file oracles check **45,613,056 actual captured
words** across all twelve forms, every element and low alignment, and byte
pattern seeds near both ordinary and wrapped DMEM addresses. Both full
corpora and the RSP vector component test pass. Three alternating, CPU-pinned
300-field SM64 course-checkpoint pairs measured **17.62 to 18.13 VI
fields/second** by median time, a **2.9% throughput gain**; two of three
pairs were faster, while all complete native states, CPU attempt counts and
audio counts matched byte for byte. A separate five-pair 60-field run favored
the change in all five pairs. The ignored local report is at
`build/artifacts/performance/n64-rsp-vector-copy/report.json` in the N64 lane.
SM64 remains below the 60-field real-time target and the N64 delivery
checklist remains **50% (10/20)**.

## Simple RDP tile coordinates (0.101.0)

The private SM64 course checkpoint made 6,167,745 texture-coordinate samples;
all used zero S/T shift and origin, no mirror, and four to six mask bits. For
tiles with those properties and up to eight mask bits, the triangle renderer
prepares clamp and wrap data once per draw and uses a direct coordinate path.
Other tile formats keep the general path. An independent coordinate oracle
checks **3,145,728** S/T sample pairs over every 16-bit coordinate, four mask
widths, three high bounds and all clamp combinations, plus fallback checks.

Five alternating, CPU-pinned 60-field SM64 course-checkpoint pairs measured
**19.21 to 19.76 VI fields/second** by median time, a **2.9%** gain; all five
pairs favored the change. Three 300-field pairs measured **19.45 to 19.79 VI
fields/second**, a **1.8%** gain by median time; two of three pairs were faster.
Every pair produced byte-identical complete native states and matching CPU
attempt and audio counts. The ignored local reports are at
`build/artifacts/performance/n64-simple-tile-coordinates/` in the N64 lane.
SM64 remains below the 60-field real-time target; the N64 delivery checklist
remains **50% (10/20)**.
