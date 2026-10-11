# Original DS ARM BLX immediate register corpus

The MIT generator emits an original native `.nds` container with an ARM9
register driver and idle ARM7. No SDK, firmware, Nintendo logo, commercial
code, guest expected answers or guest pass verdict are included. The experimental
marked-segment loader supplies coherent main RAM; native firmware boot and
instruction-cache behavior are not established.

## Executed guest domain

The original corpus matches **16,777,088/16,777,088 eight-word observations**,
zero mismatches (134,216,704 actual guest words):
all low 16-bit immediate fields in prefixes `00`, `01`, `FE`, `FF`, both H
values and all 32 incoming NZCV/Q combinations. Four field/H combinations are
excluded because the six-byte observation trampoline would overlap the four-byte
call instruction: prefix FF, low fields FFFD/FFFE, either H. These are test
placement restrictions; the decoder accepts their architectural encodings and
the exhaustive decoder gate covers them. Their arbitrary target instruction
streams are not graded by this cartridge.

The driver writes a BLX into a fixed aligned call site, backs up two target RAM
words, installs a Thumb trampoline and enters the ARM call. The trampoline
captures actual LR and Thumb PC, then returns through a separate driver register.
Afterward the driver restores the target and call-site words and emits:

1. Captured LR (the instruction after the call).
2. CPSR after returning to ARM (all original flags preserved).
3. PC captured by the second Thumb instruction.
4. Unchanged initial link seed register.
5. Actual immediate field.
6. H stimulus.
7. Actual call-site address.
8. Restored call-site RAM word.

The host independently interprets the immediate as a signed mathematical
24-bit integer, multiplies by four, adds H×2 and the ARM pipeline PC offset.
It grades all eight words. Actual Thumb target execution complements the
component check of T=1 immediately after the branch. The subroutine returns
through a separate register; this corpus checks the architectural LR value,
not a BX-LR return path. Semantics follow
[ARM DDI 0100I, A4.1.8](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).

## Exhaustive decoder gate and controls

`arm_blx_decode_test` executes **all 33,554,432 immediate/H encodings**
(`2^24 × 2`) through the native ARM9 decoder and compares PC, LR and CPSR
with an independent signed-integer calculation. Each encoding uses one incoming
flag assignment derived from its field; this is not their full Cartesian product
with flags. Separate boundary checks cover all flags, five PC/wrap positions,
six displacement boundaries, both H values and ARM9/ARM11 profiles. The full
register array is checked there. ARM7 retains its supplied condition-1111 NV
skip behavior; other unsupported unconditional ARM9 encodings still stop
without register mutation. Physical undefined behavior is not asserted.

Before implementation the original driver stops at its first BLX with zero
completed observations. The corrected **1,920-observation** control passes;
an executed H-bit-flip mutant produces **1,680 mismatches**. Individual LR/PC
bit mutations fail one row each. Missing/malformed rows, false completion,
missing/unterminated trailers and extra data report ERROR. PASS requires the
whole declared stream, correct ROM identity and a strict frontend trailer.

Raw observations, negative-control captures and JSON reports remain in
`build/artifacts/owned/nds_arm_blx/`. Generator/image hashes are in the ROM
manifest. This covers branch arithmetic and mapped original target execution;
all original 24-bit field/flag combinations, every target instruction stream,
firmware/cache/timing and physical exceptions remain unverified. Shared DSi/3DS
support is documented; the original guest corpus is graded on DS ARM9. The full
CPU milestone remains incomplete, so completion percentages are unchanged.

```sh
python3 tests/roms-src/build.py nds/arm_blx_registers --original-only --check
ctest --test-dir build -R '^(nds_arm_blx_registers|arm_blx_decode_test)$' --output-on-failure
```
