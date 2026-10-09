# Original GBA ARM register-shift corpus

This original 256 KiB cartridge image executes ARM `MOVS r0,r1,shift r2`
for LSL, LSR, ASR, and ROR. It captures the result and immediately reads CPSR
with MRS, then emits those observed registers through the mGBA debug buffer.
The host oracle uses integer arithmetic, signed floor division, and rotations;
it imports no emulator implementation.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce gba_plugin
ctest --test-dir build -R gba_arm_shifts --output-on-failure
```

Measured 2026-10-09: **65,536/65,536 result/CPSR pairs matched**, zero mismatches.

| Input | Enumerated values |
|---|---|
| Operation | Register-specified LSL, LSR, ASR, ROR |
| Shift amount | Every value 0–255 (100% of the low-byte amount domain) |
| Carry / overflow input | Every C/V combination; MOVS must preserve V |
| Operand | 0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA |

The domain is 4 operations × 16 patterns × 4 C/V states × 256 amounts.
It covers only 16 of the 2³² source operands. Amount-register upper bits are
zero; independence from nonzero upper bits remains unverified. The ROM seeds
N/Z clear and uses System mode with IRQ/FIQ disabled. It captures the entire
CPSR, including control bits. Timing remains unverified. Encodings and shift
behavior follow [ARM's ARM7TDMI data sheet, DDI 0029E, sections 4.5.1–4.5.3](https://blogs.igalia.com/aboya/files/2025/08/ARM7TDMI-Manual-1995.pdf#page=53).

## File channel and failure controls

The ROM enables `$04FFF780=$C0DE` and records its `$1DEA` reply. It writes
strings to `$04FFF600` and flushes with `$04FFF700=$103`, following
[mGBA's own register interface](https://github.com/mgba-emu/mgba/blob/master/opt/libgba/mgba.c).
Enabled messages reach `VELOCE_TEST_OUT` on the `mgba` channel. The Veloce-only
`$04FFF782=1` extension reports completion with unknown status, without a PASS
claim. The host requires exact identity, order, syntax, counts, completion,
and matching observations before PASS.

Changing one executed LSL to LSR in a smaller ROM produces 15 mismatches out
of 4,096 cases, confined to that group. Missing, malformed, reordered, and
truncated files must be measurement errors. A separate channel ROM verifies
disabled output is suppressed and a 255-character message is intact, with
its NUL in the final buffer byte. Register formatting exercises byte lanes.
The core now preserves neighboring characters on byte writes and bounds its
NUL scan rather than appending a terminator past a full buffer. The borrowed
sink is excluded from save-state serialization.

Raw files, reports, and control ROMs remain in `build/artifacts/owned/gba_shifts/`;
CI uploads them. The source generator and binary are committed with SHA256
ledger entries reproducible using Python 3 alone.

## Boot and remaining coverage

The ROM targets direct cartridge-entry boot through Veloce's BIOS HLE. It has
no copied Nintendo logo and has not been verified with retail BIOS boot or a
physical GBA. Header checksum and image size are checked. Thumb shifts,
immediate shifts/RRX, other instructions/register combinations, exceptions,
timing, memory/DMA, PPU/APU, cartridges, and link peripherals need their own
corpora. These results do not establish whole-core completion.
