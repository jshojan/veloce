# Original Nintendo DS saturating arithmetic register corpus

The MIT Python generator emits an original native `.nds` container with an
ARM9 register program and an idle ARM7 segment. It contains no SDK, firmware,
commercial code, Nintendo logo, expected answers or guest pass verdict. The
experimental core loads the marked segments directly; physical firmware boot
and retail compatibility are outside this test profile.

## Domain

QADD, QSUB, QDADD and QDSUB execute in ARM state. Every observation records
four actual guest words: destination result, full CPSR, r0 and r1. CPSR is saved
immediately after the instruction, before printing changes flags. The raw file
comes from guest MMIO writes; the frontend supplies identity and completion.

The full finite corpus matches **10,846,208/10,846,208 observations**,
zero mismatches (43,384,832 actual guest words):

- **8,388,608**: every pair of 256 repeated-byte 32-bit operands, four operations,
  all 32 incoming NZCV/Q combinations, AL condition and distinct registers.
- **2,457,600**: every pair of sixteen full-width patterns around signed/doubling
  boundaries, all 32 NZCV/Q combinations, all fifteen ordinary conditions and
  five register arrangements: distinct, destination equals left/right, same
  sources, and destination equals both sources.

The patterns are zero, one, minus one, signed extrema and their nearest inward
neighbors, both doubling thresholds and neighbors, alternating bits, and a
full-width guard. Condition `1111` is outside the ordinary condition domain;
ARM7 lacks these ARMv5TE instructions. R15 operands are unpredictable. Separate
native decoder checks verify diagnostic stops for ARM7 and R15 without register
mutation, and failed conditions skip execution. These diagnostic stops do not
model physical exceptions. ARM9/ARM11 accept ordinary non-PC operands.

## Independent grading

The host interprets 32-bit inputs as mathematical signed integers, doubles and
clips the second operand where requested, performs add/subtract, then clips the
final result. It sets Q if either stage clips, preserves an already set Q, and
checks every other CPSR bit. An independent Boolean truth table selects the
condition; source/destination aliases are accounted for after execution.
No bits are masked out. Literal oracle anchors include an intermediate clipping
case whose final result is in range. Semantics follow the primary
[ARM DDI 0100I reference](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a),
sections A4.1.46, A4.1.50–52. Timing is not graded.

The reduced **347,648-observation** cartridge keeps all flags, conditions and
aliases with four byte values and six boundary values. An executed mutant
encodes QSUB in place of QDSUB; **25,664 observations** fail, confined to QDSUB
groups. Individual result/Q bit mutations each fail exactly one row. Missing or
malformed rows, false completion, missing/unterminated trailers and extra data
report ERROR. PASS requires the entire declared stream and strict frontend
identity/completion, never an emulator-private guest verdict.

Before implementation the same original cartridge stopped at its first QADD
with zero completed observations. Raw observations and JSON reports remain in
`build/artifacts/owned/nds_saturating/`. The corpus measures this finite domain;
it does not exhaust all 32-bit pairs, full DSP instructions or ARM hardware.
DS/DSi/3DS completion percentages remain unchanged.

```sh
python3 tests/roms-src/build.py nds/saturating_registers --original-only --check
ctest --test-dir build -R '^(nds_saturating_registers|arm_saturation_decode_test)$' --output-on-failure
```
