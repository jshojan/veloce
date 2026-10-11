# Original Nintendo DS CLZ register corpus

The MIT generator emits a repository-owned `.nds` container with an ARM9
program and idle ARM7 segment. There is no SDK, firmware, commercial code,
Nintendo logo, expected result table or guest verdict. The core loads marked
segments directly; physical firmware and retail boot are not established.

Each guest observation writes three actual registers to the file channel:
result, full CPSR and the source register after execution. MRS captures CPSR
before output code changes flags. The frontend owns the identity and trailer.

## Finite domain

The full corpus matches **12,646,272/12,646,272 observations**, zero
mismatches (37,938,816 actual guest words):

- **12,582,912**: every value 0–65,535 placed at bit positions 0, 8 and 16,
  every incoming NZCV/Q combination and distinct/identical source/destination
  registers. These are 196,608 placements, including repeated values across
  placements; they are not all 32-bit operands.
- **63,360**: zero, all single-bit values, low masks of lengths 1–32 and
  alternating-bit words (66 distinct values), all fifteen ordinary conditions,
  all 32 NZCV/Q states and both alias forms.

The independent host uses Python integer bit length to obtain `32-bit_length`,
checks a separate Boolean condition table, and grades all three words without
masking bits. Literal anchors include zero, one, both top-bit cases, all ones
and an interior bit. CLZ changes no CPSR bits and zero produces 32, following
[ARM DDI 0100I, A4.1.13](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).
Instruction timing and physical exception behavior remain unverified.

A separate native decoder gate covers **936,000 ARM9/ARM11 executions**:
all 225 non-PC source/destination field pairs, zero and every single-bit/dense
low-mask position, and all flags. It checks every register and CPSR, and checks
ARM7/R15 diagnostic stops and skipped failed conditions. Diagnostic stops do
not claim physical undefined-instruction or unpredictable behavior.

## Negative controls and reproducibility

Before implementation the same cartridge stops at the first CLZ, with zero
completed observations. The reduced **64,896-observation** control retains all
condition and flag cases. An executed conditional result-plus-one mutant fails
**35,328 observations**; skipped conditions remain unchanged. Individual result
and CPSR Q-bit mutations fail exactly one row each. Missing/malformed rows,
false completion, missing/unterminated trailers and trailing data report ERROR.
PASS requires every declared row and strict frontend identity/completion.

Raw results, diagnostic logs, mutated captures and JSON reports stay in
`build/artifacts/owned/nds_clz/`. The source generator and archived image are
hashed in the ROM manifest. Shared decoder support includes DSi ARM9 and 3DS
ARM9/ARM11; this original guest corpus is graded on DS ARM9. Full DSP/CPU,
hardware timing and retail boot remain unfinished. No whole-CPU completion
milestone is awarded.

```sh
python3 tests/roms-src/build.py nds/clz_registers --original-only --check
ctest --test-dir build -R '^(nds_clz_registers|arm_clz_decode_test)$' --output-on-failure
```
