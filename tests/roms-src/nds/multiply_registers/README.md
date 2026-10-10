# Original Nintendo DS multiply register corpus

The standalone MIT Python generator emits an original `.nds` cartridge with two
ARM-state guest programs. It contains no SDK, firmware, BIOS, Nintendo logo,
commercial code, expected answers or guest pass verdict. The experimental DS
plugin loads marked segments directly; this supplied emulator test profile is
not a physical firmware boot claim. Both segments execute from main RAM; ARM9
signals completion through shared RAM before ARM7 emits its observations.

## Domain and grading

The full corpus matched **38,338,560/38,338,560 five-word observations**,
zero mismatches (100% of this finite domain):
25,559,040 ARM9 and 12,779,520 ARM7. It executes MUL, MLA, UMULL, UMLAL, SMULL
and SMLAL, each with S clear and set. Each CPU runs all sixteen incoming NZCV
states; ARM9 also runs both Q states. Sixty groups per CPU comprise:

- Every pair of 256 repeated-byte 32-bit values, using accumulator `$FFFFFFFFFFFFFFFF`.
- Every pair of sixteen full-width patterns, with accumulator seeds zero,
  `$FFFFFFFFFFFFFFFF`, `$8000000000000000` and `$55555555AAAAAAAA`.

For short multiplication the accumulator is the seed's low word; the high word
is an unchanged guard. For long multiplication both result words are observed.
Every row records low result, high result/guard, full CPSR and both unchanged
source registers. Ordinary distinct non-PC operands are used. The guest saves
CPSR immediately after the operation, before any printing instructions modify
flags. It streams only observed values via emulator debug writes; the frontend
owns the raw file identity and completion trailer.

The independent host uses unbounded Python signed/unsigned multiplication and
addition, then reduces to the instruction's width. Signed accumulation is
modulo 64 bits, including wraparound. N/Z depend on the full result width; all
other defined CPSR bits are preserved. The primary
[ARM architecture reference](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a)
defines C as unpredictable for ARMv4 MULS/MLAS, and C/V as unpredictable for
ARMv4 long S forms. These bits are captured but excluded from scoring:
6,389,760 ARM7 snapshots contain 10,649,600 unscored CPSR bits. All result/source
bits and every defined CPSR bit are graded; ARM9 and all non-S forms have no
mask. The model preserves the unpredictable bits deterministically, without
claiming their physical values. Q is preserved on ARM9.

The reduced control cartridge retains all boundary groups and uses byte values
0/127/128/255, giving 599,040 observations. An executed signed-to-unsigned mutant
must fail only signed long-multiply groups. Five individual CPSR mutations
verify that undefined ARM7 bits are accepted while defined V, ARM9 C and
non-S C are rejected. Seven damaged files must report ERROR (missing/malformed
row, reordered group, false completion, missing/unterminated trailer, extra
data). PASS requires every declared observation and a strict frontend trailer.
Raw files and JSON reports remain in `build/artifacts/owned/nds_multiply/`.

```bash
python3 tests/roms-src/build.py nds/multiply_registers --original-only --check
ctest --test-dir build -R '^nds_multiply_registers$' --output-on-failure
```

Full 32-bit operand/64-bit accumulator domains, physical multiplier latency,
early termination, pipeline/bus scheduling, destination/source aliases, reserved
encodings, Thumb multiplication and ARM9 DSP extensions are outside this ROM
claim. Component checks cover selected aliases separately. Retail compatibility,
physical-hardware comparison and whole-platform completion remain unknown.
