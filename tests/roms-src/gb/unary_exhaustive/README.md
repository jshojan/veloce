# Original Game Boy unary instruction corpus

The original unbanked 32 KiB ROM executes INC A, DEC A, DAA, CPL, SCF, CCF,
RLCA, RRCA, RLA, RRA, and CB-prefixed RLC A, RRC A, RL A, RR A, SLA A,
SRA A, SWAP A, and SRL A. Each instruction receives all 256 accumulator
values and all 16 incoming Z/N/H/C combinations. This is
**18 × 256 × 16 = 73,728 A/F pairs**, including every DAA input even when
it cannot result from valid packed-BCD arithmetic.

Input A/F is restored through PUSH BC; POP AF. The stimulus executes directly,
then PUSH AF; POP BC captures both actual registers before output routines run.
Groups are ordered by operation, flags, then ascending input A. Each `REG af=`
row contains 32 pairs as lowercase hexadecimal; the reserved low four F bits
must be zero. The accumulator rotates must clear Z even for a zero result,
whereas CB rotates must derive Z from the result. INC/DEC preserve C, CPL
preserves Z/C, and SCF/CCF preserve Z. DAA clears H and preserves N.

The host integer oracle is independent of emulator code. It computes shifts
using integer division/remainders and determines both DAA corrections from the
original A/F, rather than reproducing the core's sequential widened adjustment.
Hand-calculated reference cases check zero, carry preservation, half carry,
subtraction, DAA on unusual flag inputs, signed shifts, and accumulator/CB
rotate differences. Instruction definitions come from
[Pan Docs](https://github.com/gbdev/pandocs/blob/master/src/CPU_Instruction_Set.md)
and the [RGBDS instruction reference](https://github.com/gbdev/rgbds/blob/master/man/gbz80.7).

## Register channel and limits

The ROM disables interrupts, LCD and sound, and uses a WRAM stack. Characters
are written to SB ($FF01), then SC ($FF02)=$81. The existing debug hook observes
the start request without waiting for physical serial delivery. Neither link
transfer timing nor physical cable operation is measured. The ROM includes no
Nintendo logo and uses the core's post-boot entry; retail boot and physical
hardware execution remain unverified. No screenshot or stdout verdict is scored.

This covers accumulator forms only. Other register/memory locations, BIT/RES/SET,
instruction timing, interrupts, CGB modes, peripherals and whole-core completion
remain unmeasured. A 100% result means only this declared finite domain matched.

The ROM declares `LOG complete cases=73728` and `END observations_complete`
without declaring PASS. The sink trailer must have unknown status (-1). The
host requires the correct ROM CRC/core/channel, group order, row sizes, complete
counts, completion and trailer records, and no trailing data before grading.

## Reproduce

```bash
python3 tests/roms-src/build.py gb/unary_exhaustive
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce gb_plugin
ctest --test-dir build -R '^gb_unary_exhaustive$' --output-on-failure
```

The builder needs only Python and emits all instructions, strings, checksums
and label fixups. Its source and binary SHA256 are in `rom_manifest.json`.
Repository-owned sources and ROM are MIT licensed. The runner retains register
observations and JSON group counts, mismatches, examples, ROM hash, frame count
and cycle count under `build/artifacts/owned/gb_unary/`.

A separate 16-value mutant executes DEC in the first INC group; only that group
must fail. Missing rows, malformed rows, reordered groups, false-PASS completion,
missing/unterminated trailers, and data after the trailer must be measurement
errors. All raw negative files and reports are retained. `run.py --smoke` covers
values 0–15 and all flags (4,608 pairs); it is not the full corpus.

Measured 2026-10-09: **73,728/73,728 pairs matched**, zero mismatches across
288 groups, completing after 874 frames. The executed mutant failed only its
changed group and all seven damaged-file controls produced measurement errors.
No CPU correction was required by this corpus.
