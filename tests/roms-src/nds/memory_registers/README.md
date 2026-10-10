# Original Nintendo DS memory register corpus

This standalone MIT generator emits original ARM9 and ARM7 programs in a marked
`.nds` cartridge. It includes no SDK, BIOS, firmware, Nintendo logo, commercial
code, expected answers or guest pass verdict. The supplied cartridge profile
starts both CPUs in ARM System mode with interrupts masked. ARM9 signals through
main RAM before ARM7 emits its stream. This is an emulator test profile; physical
firmware boot and memory timing are not modeled.

## Finite domain

The full domain matched **49,694,208/49,694,208 nine-word observations**,
zero mismatches (100% of this finite domain): 33,129,472 ARM9
and 16,564,736 ARM7, across 15,326 groups per CPU. It covers:

- ARM STRH/LDRH/LDRSB/LDRSH: immediate and register offsets, additive/subtractive
  offset, pre-index writeback and post-index forms, all 256 immediate encodings
  and register offsets 0–255, with sixteen full-width operand patterns.
- Thumb register STR/STRH/STRB/LDRSB/LDR/LDRH/LDRB/LDRSH with offsets 0–255.
- Every 32-entry immediate field for Thumb word, byte and halfword loads/stores;
  every 256-entry SP-relative word and PC-relative literal field.
- All 65,536 halfword values for ARM and Thumb STRH/LDRH/LDRSH at both aligned
  halfword lanes. Inputs combine a low halfword with its complement above it.
- All 256 repeated-byte values for ARM LDRSB and Thumb STRB/LDRB/LDRSB at all
  four byte lanes. Word transfers use sixteen full-width patterns.
- Every NZCV input on both CPUs, and both ARM9 Q states.

Each case records destination, CPSR, base after writeback, original offset,
original source value, the containing memory word, two neighboring guard words,
and SP before the driver restores its synchronization pointer. Static instruction
stubs execute the transfer; Thumb stubs return to ARM before MRS captures CPSR.
The driver initializes memory and inputs without embedding expected output.
Before a store, every byte of the containing word is the complement of the
source low byte; the first written byte must change at every tested lane.
Literal loads use reserved data after their static instruction, so the corpus
makes no claim about instruction cache coherency.

The host independently models bytes with Python byte arrays and signed decoding.
It computes instruction addresses from segment headers and the marked stub
section, and grades every recorded bit. There are no unscored bits in this
corpus. It requires exact CPU/group order, row count, image CRC and frontend
completion. Execution evidence is recorded in the generated JSON report.

The [ARM architecture reference](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a)
defines odd halfword and unaligned Thumb word transfers as unpredictable on these
architectures. The ROM tests aligned words/halfwords only. Odd halfwords stop
diagnostically in this core; Thumb word alignment/rotation is a deterministic
model choice, outside this hardware grade. Writeback to PC, destination/base
writeback aliases, register-offset/base writeback aliases and reserved encodings
are excluded from the ROM domain and checked separately as diagnostic cases.

## Grader controls and artifacts

The reduced cartridge contains **213,504 observations**, retaining boundaries,
all instruction/address forms and selected immediate fields. Executed signed-to-
unsigned load and halfword-to-byte store mutants must FAIL with respectively
14,016 and 4,224 mismatches confined to the changed instruction families. A
missing-store mutant (legal Thumb MOV r8,r8) must fail every changed store case
at both lanes: 4,608 mismatches. A few values leave the second byte unchanged
and cannot individually distinguish store width; the remaining values reject
that mutation. Seven malformed files must return ERROR: missing or
malformed row, reordered group, false completion, missing or unterminated trailer,
and trailing data.

Raw register files and reports remain in `build/artifacts/owned/nds_memory/` and
are uploaded by owned-hardware CI. The manifest records generator/image SHA256.

```bash
python3 tests/roms-src/build.py nds/memory_registers --original-only --check
ctest --test-dir build -R '^nds_memory_registers$' --output-on-failure
```

Component checks cover register aliases, every encoded low destination,
PC alignment, odd-half diagnostics, reserved operands and failed-transaction
atomicity. Full 32-bit operand/offset/address domains, stack/block/doubleword
transfers, cache/TCM/MPU, exceptions, bus timing and physical
hardware comparisons remain unverified or unimplemented. Retail DS/DSi boot,
graphics/audio/input/peripherals and DS netplay remain incomplete. Finite domain
coverage establishes no whole-platform completion percentage.

## Observation transport

Guest STR to the emulator-only `$04FF0008` port serializes an observed register
word as eight lowercase hex digits. Text framing still uses the byte port at
`$04FF0000`. The frontend owns the file and completion trailer; the host derives
and grades expected values independently. Native serialization changes no
instruction domain or snapshot field. These ports are outside DS hardware and
provide no evidence of physical timing.

Adjacent Thumb BL and ARM9 BLX have a separate original
[long-call corpus](../long_calls/README.md); its finite field domain does not
establish physical cache coherence or exception behavior.
