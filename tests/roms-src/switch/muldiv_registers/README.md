# Original Switch AArch64 multiply/divide guest

Original MIT NSO0 and assembler source, using the existing original assembler.
No SDK, firmware, keys or commercial bytes. The production Switch plugin
executes the guest and records actual STR values through `vnxdebug`.

The table contains **256 repeated-byte 64-bit values plus 16 distinct full-width
boundaries**. The boundaries include signed minima/maxima, word limits,
full-width negative values and nonuniform bits; together all 272 values are
unique. Every ordered pair executes with two accumulator/NZCV seeds:
zero/`0x9` and all-ones/`0xF`. The fourteen forms are:

- MADD, MSUB, UDIV and SDIV in both 32- and 64-bit forms;
- SMADDL, SMSUBL, UMADDL and UMSUBL;
- SMULH and UMULH.

**272 × 272 × 2 × 14 = 2,071,552 rows / 12,429,312 actual captured words**
match the independent oracle. Each row stores source X0/X1, actual accumulator
X3, result X2, actual MRS NZCV X4, and context X5. Long forms consume the low
32-bit operands; high forms and division ignore the captured accumulator.
Guest values are observed rather than populated from host expectations. The
host reference uses unbounded signed/unsigned integers, with explicit
truncate-toward-zero division, zero-divisor results, signed-overflow wrapping
and fixed reference assertions. NZCV is preserved and W results zero-extend.

All inputs in this corpus have defined result bits, including division by zero
and signed MIN/−1. The executed EOR result-bit mutant fails all **448 short
cases**. Six field mutations fail exactly one row each; seven malformed streams
produce ERROR. Strict wrapper/suite/domain, row widths/count/order, completion,
trailer and EOF checks prevent truncated runs from being scored as passes.

```bash
python3 tests/roms-src/build.py switch/muldiv_registers --original-only --check
cmake --build build --target veloce switch_plugin switch_muldiv_test switch_plugin_state_test -j
ctest --test-dir build -R '^switch_muldiv' --output-on-failure
```

Artifacts: `build/artifacts/owned/switch_muldiv/`, including raw register streams,
JSON grades, actual frontend logs and executed mutant. The production state gate
also verifies identical register streams/full states after restoring mid-run.
The [optional LLVM encoding check](../../../reference/switch-muldiv/README.md)
validates the original opcode fields independently of the interpreter.

This exhausts the stated finite table-pair/form/seed domain. Full-width operand
pairs, native boot, physical hardware timing and other CPU/hardware families
remain unfinished; this is not full Switch compatibility. Emulator-only capture
addresses and NSO process-profile limitations are documented in the
[core README](../../../../cores/switch/README.md).
