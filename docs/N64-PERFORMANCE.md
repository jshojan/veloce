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
