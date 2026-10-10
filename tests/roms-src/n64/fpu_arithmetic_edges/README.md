# Original N64 fpu arithmetic edges corpus

This MIT licensed assembler emits original guest code and data; no SDK,
firmware or commercial bytes. The guest executes arithmetic, recovers FPE with
its own exception handler and streams actual observations. It computes no verdict.

**215,152 rows / 1,290,912 captured 64-bit words** cover:

Every S/D exponent field, both signs, eight functions and four rounding modes on a fraction pattern cycled among zero, one, all ones and the high bit (147,456 rows). All pairs of twelve special/boundary values, four sign combinations, four binary functions, both formats and four modes (18,432 rows). Twelve selected operand profiles × eight functions × four modes × all 32 enables × both FS settings × both formats (49,152 rows), with sticky flags and condition set. Fourteen reserved formats × eight functions (112 rows).

The six captures are destination, FCSR, handler marker, CPU exception code,
unchanged left source and unchanged right source. A separate exact-rational
Python oracle grades defined bits. Square roots use integer square comparisons;
no native floating-point rounding or emulator code is imported. Undefined upper
bits of successful single destinations are captured but ungraded.

An executed capture mutation fails all 1,024 reduced rows. Six field mutations
fail individually; seven malformed/incomplete streams return ERROR. Raw streams,
JSON reports and logs live in `build/artifacts/owned/n64_fpu_arithmetic_edges/`.
The image/generator hashes are in the committed ledger.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_fpu_arithmetic_test -j
ctest --test-dir build -R '^n64_fpu_arithmetic' --output-on-failure
```

The component adds 540,672 register/FR/format/function cases, 327,680 FCSR control
combinations, CU1 precedence, reserved formats, delay/nested/BEV context and exact
pending-arithmetic replay. Rules follow the [NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf)
and the hardware-test author's [arithmetic cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs).
Subnormal inputs and top-payload-bit-0 NaNs require assist; top-bit-1 NaNs raise
invalid and produce the legacy canonical NaN. Assist operands take precedence.
MOV copies all bits and preserves FCSR causes; other operations replace causes.
Trapped results preserve destination and old sticky flags. FS/enable controls
select assist or directed signed-zero/minimum-normal tiny-result outcomes.

These selected finite domains do not exhaust independent full-width operands.
No physical console has validated these cartridges. Undefined odd FR=0 operands
use an RI convention; nonzero unary ft fields use an assist convention. Physical
behavior of those encodings is ungraded. FPU comparisons, pipeline hazards,
precise CE timing, reverse endian and whole-core hardware remain pending.
N64 stays 40% on the fixed delivery checklist.
