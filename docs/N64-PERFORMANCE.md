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
throughput and complete-state equivalence in
`build/artifacts/performance/n64/report.json`. It alternates run order and
rejects differences in complete native states, emulated attempts, audio counts
or frame counts. Optional `--min-speedup` is a local acceptance threshold;
absolute FPS is not a portable test gate.

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
physical timing are pending.
