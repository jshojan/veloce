# Original V810 immediate-instruction corpus

The standalone [builder](build.py) creates original reset-vector code and a
marked cartridge without an SDK, commercial assets, guest expectations or a
pass verdict. The [host grader](run.py) reads actual result/PSW/source snapshots
through shared-plugin discovery and uses independent unbounded integer
arithmetic. Definitions follow the
[NEC V810 architecture manual](https://www.virtual-boy.com/downloads/978644/get/978650).

## Declared domain

**100,712,448/100,712,448 three-word snapshots matched, zero mismatches**,
with all 16
incoming Z/S/OV/CY states on each of 16 full-width operand patterns:

| Instructions | Immediate domain | Snapshots |
|---|---|---|
| MOVEA, ADDI, ORI, ANDI, XORI, MOVHI | Every 16-bit immediate, 0–65,535 | 100,663,296 |
| MOV, ADD, CMP, SHL, SHR, SAR immediate forms | Every 5-bit immediate, 0–31 | 49,152 |

The patterns are 0, 1, 2, 15, 16, 127, 128, 255, `$7FFF`, `$8000`, `$7FFFFFFF`,
`$80000000`, `$FFFFFFFE`, `$FFFFFFFF`, `$55555555` and `$89ABCDEF`. Coverage is
exhaustive over the immediate encodings and incoming low flags for these
patterns, not over all 32-bit operands or the entire platform.

The ROM copies each encoded instruction from its original table into WRAM,
prepares registers and PSW, executes the instruction, and returns through its
own JMP stub. No cache executes in this profile. This exercises instruction
fetch from changing RAM, including every wide instruction tail. A host-side
shortcut does not execute the cases. The guest records destination r20, PSW
immediately after execution, and unchanged source/sentinel r25.

MOVEA sign-extends the immediate and preserves PSW; MOVHI adds its shifted
immediate and preserves PSW. ADDI/short ADD derive wrapping result, carry and
signed overflow. CMP preserves its destination. Logical wide immediates
zero-extend, preserve CY and clear OV; ANDI clears S. Short MOV sign-extends and
preserves PSW. Every shift amount, including zero with cleared carry, is graded.
All cases preserve non-arithmetic PSW fields with NP set in the profile.

The reduced positive control contains 73,728 snapshots with all short
immediates and 16 wide-immediate boundaries. Executed ADDI-to-ORI and MOV-to-SHL
mutations failed only their intended groups. Seven damaged result files returned
ERROR. Positive/mutation controls do not replace full-domain grading. Hand
reference vectors and domain completeness are checked before execution.

The guest streams observed registers through the emulator-only debug address.
No screenshots or guest verdicts are scored. Whole-image CRC32 identity, exact
group/row counts, completion and the frontend terminator trailer are mandatory.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce vb_plugin -j
ctest --test-dir build -R '^vb_immediates$' --output-on-failure
```

Raw files/reports remain in `build/artifacts/owned/vb_immediates/` and are uploaded
by owned-hardware CI. Source/image SHA256 are recorded in the manifest. The full
corpus takes longer than the existing compact domains and emits about 3 GiB.

## Limits

These observations verify the declared instruction domain against architectural
definitions. Register aliases/r0 and fixed vectors have separate component
checks; this ROM uses distinct nonzero registers. All operand words, branches,
RAM coherency with a physical cache, interrupts during instruction patching,
bus/pipeline timing, bit-string searches, remaining CPU/NVC instructions,
VIP/VSU, controllers and retail compatibility remain unverified or missing.
Full Virtual Boy completion is unknown.
