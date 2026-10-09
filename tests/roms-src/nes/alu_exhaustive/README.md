# NES ADC/SBC register corpus

This is original Veloce code under the repository's MIT license. The committed
16 KiB NROM image and iNES header are emitted by `build.py`, using Python 3.
There is no downloaded ROM, assembler, expected-result table, or core-side
pass heuristic.

```bash
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/nes/alu_exhaustive/run.py --emulator build/bin/veloce
# Or, after building the application and nes_plugin:
ctest --test-dir build -R nes_alu_exhaustive --output-on-failure
```

The ROM executes zero-page ADC and SBC for every A/operand pair (`256 * 256`),
both input carry states, and both decimal-flag states: **524,288 observations**.
It starts each case with I=1 and V=1, then captures the resulting A and the
status byte pushed by PHP before doing output work. This covers N, V, Z, C,
preserved D/I, and PHP's exposed B/unused bits. It sends hexadecimal register
bytes to `$401E`; reading `$401F` probes the attached test channel. Writing 1
to `$401F` signals completion with an unknown verdict.

Each `BEGIN <op>_c<carry>_d<decimal>` is followed by packed `REG ap=` rows.
Every four hex digits describe one observed A/P pair. A full-size row carries
32 pairs (128 digits), below the sink's 255-byte line limit. For each group,
the order is `A=0..255`, with `operand=0..255` inside that loop. The host requires
all eight groups in order, exactly 65,536 pairs per group, a completion marker,
the correct ROM identity, and a terminator trailer. It rejects missing,
malformed, reordered, or additional observations.

The host computes expectations with integer arithmetic and signed range checks,
independently of the core. Its reference is MOS Technology's
[MCS6500 Programming Manual](https://syncopate.us/books/Synertek6502ProgrammingManual.html),
sections 2.2.1, 2.2.2, and 3.6–3.8. The NES-specific exception is that the
decimal flag is retained but arithmetic stays binary, as documented in the
[2A03 hardware reference](https://www.nesdev.org/wiki/2A03).

The runner also changes the first ADC opcode to SBC in a small ROM and requires
FAIL. It tests damaged register files to ensure incomplete evidence cannot pass.
It retains every observation and a JSON report under
`build/artifacts/owned/nes_alu/`. On 2026-10-09, all 524,288 pairs passed on the
Linux Release build; the opcode mutation produced 256 mismatches.

This is 100% of the stated ADC/SBC operand/carry/decimal domain, executed through
zero-page addressing. Other addressing modes, instruction timing, interrupts,
other opcodes, PPU, APU, DMA, controllers, and mapper behavior need their own
corpora. This percentage is not whole-core completion and is not added to the
legacy external-ROM weighted scorecard.
