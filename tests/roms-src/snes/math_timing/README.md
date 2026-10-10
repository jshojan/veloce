# Original SNES math timing corpus

This original 32 KiB LoROM measures reads of `$4214–$4217` while the CPU's
multiply/divide unit is still running. The ROM captures one register byte per
independent trial at CPU phases **2 through 18**, including the eight-clock
multiply and sixteen-clock divide boundaries. It records **504,832 bytes**:

- Multiply: 16 multiplicand patterns × all 256 multipliers × four registers ×
  17 phases = 278,528 observations.
- Divide: 13 boundary dividends × all 256 divisors × four registers × 17 phases
  = 226,304 observations, including division by zero.

The patterns include zero, one, powers of two, low-bit runs, signed boundaries,
and the largest values. The separate [settled corpus](../math_registers/README.md)
continues to test all 65,536 multiplication operand pairs. These timing trials
cover selected multiplicands/dividends, not the complete timed operand domain.

## CPU phase and oracle

A write to `$4203` or `$4206` triggers the operation. After it, the ROM pads
with two-clock NOPs and three-clock direct-page loads. An 8-bit direct-page
result load samples after two fetch clocks; an absolute load samples after
three. DP is `$4200`, native mode uses 8-bit A and 16-bit X, and captured bytes
are saved in WRAM before printing. Interrupts and DMA are disabled and the
screen is forced blank. Input and output loops use ROM and WRAM access speeds;
latency advances by CPU clocks regardless of those master-clock costs.

The independent host oracle uses closed-form integer arithmetic:

- After k multiply clocks, RDMPY is multiplier × the multiplicand's lowest k
  bits; RDDIV is the concatenated multiplier/multiplicand divided by 2^k.
- After k divide clocks, the quotient prefix and remainder come from dividing
  the dividend by divisor × 2^(16−k). The prior RDDIV value is shifted through
  the upper quotient bits until sixteen clocks have elapsed.
- For a zero divisor, each quotient prefix bit is one and the dividend remains
  unchanged in RDMPY.

Every divide trial first establishes RDDIV=`$00A5` through a settled zero
multiplication, making intermediate reads independent of previous trials.
The oracle contains no emulator code and does not reproduce the implementation's
step loop. The timing model is grounded in [nocash hardware notes](https://problemkaputt.de/fullsnes.htm#snesmathsmultiplydivide)
and cross-checked against the phase behavior documented by
[bsnes CPU memory cycles](https://github.com/bsnes-emu/bsnes/blob/master/bsnes/sfc/cpu/memory.cpp),
[math clock edges](https://github.com/bsnes-emu/bsnes/blob/master/bsnes/sfc/cpu/timing.cpp),
and [trigger handling](https://github.com/bsnes-emu/bsnes/blob/master/bsnes/sfc/cpu/io.cpp).
Physical hardware execution remains unverified.

## Register files and controls

`REG bytes=` rows contain 32 observed bytes as hexadecimal, ordered by operation,
register, phase, lhs pattern, then rhs. Characters are written to the existing
optional `$21FE` observation port. `$21FF=1` declares completion with an unknown
verdict. There is no visual or ROM PASS score. The host requires matching ROM
identity, every group/row/count, completion, an unknown-status trailer, and no
trailing data.

```bash
python3 tests/roms-src/build.py snes/math_timing
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce snes_plugin snes_math_pipeline_test
ctest --test-dir build -R 'snes_math_(pipeline_test|timing|registers)' --output-on-failure
```

Raw register observations and JSON reports remain in
`build/artifacts/owned/snes_math_timing/`. Changing the first RDDIV sample to
RDMPY must fail only that group. Missing/malformed rows, reordered groups,
missing trailers, and trailing data must be measurement errors; those files
and reports are retained too. The builder emits its own instructions and
calculates cartridge header checksums using only Python. Its binary and
source SHA256 values are in `rom_manifest.json`; source and ROM are MIT licensed.

## Implementation and remaining work

The core now advances its shift/add and restoring-division state on CPU read,
write, and internal-idle edges. Bulk master-clock bookkeeping does not advance
it a second time. Busy trigger writes change the shared accumulator but do not
restart the operation; operand latches and pending state are serialized.
`snes_math_pipeline_test` checks 646,400 register-pair snapshots at every phase
from 0, pending-state save/resume, busy-trigger writes, cancellation, and state
format rejection. It also compares complete actual-plugin states after a
save/load/resume sequence. These unit checks supplement the ROM observations.

SNES internal state payloads now start with `VSNES002`; old unversioned states
are rejected and must be recreated. The emulator plugin ABI remains v2.
DRAM-refresh edges, DMA/HDMA interactions, CPU bus subphases, interrupt overlap,
additional addressing modes, FastROM hardware runs, and physical revision
variations remain unverified. This corpus does not establish whole-core timing
or completion.

Measured 2026-10-09: **504,832/504,832 register bytes matched**, with zero
mismatches across all 136 groups (100% of the declared operands/registers/phases).
The altered sample failed only its changed group; all five damaged-file controls
produced measurement errors. These counts do not measure full timing accuracy.
