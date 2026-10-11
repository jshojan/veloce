# Original DS ARMv5TE DSP multiplication register corpus

The MIT generator builds an original marked native `.nds` container with an
ARM9 register driver and idle ARM7. It includes no SDK, Nintendo logo, firmware,
commercial code, guest expected answers or guest pass verdict. The experimental
loader supplies coherent main RAM; native boot and physical timing are pending.

## Instruction behavior and measured domain

All sixteen ARMv5TE signed DSP multiply forms are implemented: four SMULxy,
four SMLAxy, four SMLALxy, two SMULWy and two SMLAWy. Selected halfwords are
signed; unused halves are ignored. Word-by-halfword forms discard sixteen low
product bits, including for negative products. Accumulation wraps rather than
saturates. SMLA/SMLAW set sticky Q on signed 32-bit addition overflow;
SMLAL wraps modulo 2^64 without changing flags. NZCV is preserved throughout.
See [ARM DDI0100I A4.1.74/.77/.79/.86/.88](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).

The full corpus grades **15,454,208 seven-word snapshots**
(**108,179,456 actual guest words**), zero mismatches in the 201.66-second
full CTest run (including controls):

- **8,388,608 sweep observations:** every 16-bit value in the selected Rm
  lane for halfword-by-halfword forms, or the selected Rs lane for word forms;
  four partner boundaries, both incoming Q values and AL. Inactive halves
  carry nonzero guards. NZCV is zero in this sweep.
- **7,065,600 boundary observations:** eight full-width patterns squared,
  all 32 NZCV/Q assignments, all fifteen ordinary conditions and five alias
  layouts. Accumulating families use four 64-bit seeds; nonaccumulating
  families use one. Layouts cover distinct registers, destination=Rm,
  destination=Rs, shared sources and accumulator=Rm (both long destinations
  alias sources in the last long layout). Register aliases change effective
  accumulator values; the independently modeled pre-operation register array
  determines those values. Long destination registers remain distinct.

Each row contains actual R0–R5 followed by CPSR. R0/R1 hold sources, R2 the
short accumulate seed, R3 a preserved guard, R4 the ordinary destination or
long low half, and R5 the long high half or a preserved seed. Alias layouts
redirect destinations. The ROM captures CPSR before output routines change
flags. It supplies no expected result. The host calculates signed mathematical
products, Python floor division, modulo sums and predicates independently,
then grades every emitted bit, including guards and unchanged registers.
Literal anchors pin negative fractional rounding, signed overflow, long wrap
and sticky Q behavior.

This is not all 16-bit operand pairs, all 32-bit word operands or all 64-bit
accumulator values. The whole CPU milestone remains incomplete. No physical
multiplier timing, pipeline/early termination, exceptions, firmware, cache,
ARMv6 DSP extensions or hardware console comparison is claimed.

## Decoder checks and negative controls

The native decoder gate performs **3,538,944** checks. It visits every four-field
register encoding for all sixteen forms on ARM7/ARM9/ARM11 profiles. Each field
assignment has one derived incoming NZCV/Q setting; this is not a Cartesian
product with every flag value. Legal forms compare the complete register array
and CPSR against a separate integer oracle. ARM7, PC operands, equal long
destinations and nonzero SBZ fields stop without register mutation. Every
operation performs only its instruction fetch, with no data transaction.
Separate boundary checks cover eight operands squared, six accumulation seeds,
all 32 flag assignments and both supported profiles. Failed ordinary conditions
skip unavailable or invalid forms. Diagnostic stops do not establish physical
undefined-instruction exception behavior.

Before implementation the original cartridge emits zero observations. The
**994,240-row** reduced control passes; an executed wrong-Rs-half selector
mutant produces **130,056 mismatches**, including every instruction form.
Individual result/Q bit corruptions fail exactly one row each. Missing/malformed
rows, false completion, missing/unterminated trailers and extra data report
ERROR. PASS requires the entire declared ordered stream and exact ROM identity.

Raw captures and JSON reports stay in
`build/artifacts/owned/nds_dsp_multiply/`. Generator/image SHA256 are recorded
in the manifest. DS ARM9 runs the original cartridge; DSi and 3DS share the
implementation, while the component gate separately checks ARM11 semantics.
Completion percentages are unchanged because remaining CPU/device milestones
are still unfinished.

```sh
python3 tests/roms-src/build.py nds/dsp_multiply_registers --original-only --check
ctest --test-dir build -R '^(nds_dsp_multiply_registers|arm_dsp_multiply_decode_test)$' --output-on-failure
```
