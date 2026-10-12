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
