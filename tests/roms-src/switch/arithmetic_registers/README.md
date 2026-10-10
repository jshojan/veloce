# Original Switch AArch64 arithmetic guest

Original MIT Python assembler and uncompressed native NSO0 archive, built without
SDK, firmware, keys or commercial bytes. Native execution/physical timing is not
claimed; the guest runs on the emulator's documented fixed process profile.

The guest loads all 256 repeated-byte operand values from an original table and
executes ADD(S)/SUB(S) and carry instructions: specifically ADDS, SUBS, ADC, ADCS,
SBC and SBCS, in 32- and 64-bit forms. It repeats the pair domain with incoming
NZCV seeds **0x9** (carry clear) and **0xF** (carry set). That is
**256 × 256 × 2 × 2 × 6 = 1,572,864 observations**. All **7,864,320 words** match
the independent host oracle: source X0/X1, result X2, actual MRS NZCV X3, and the
executed context index X5. W results are zero-extended; source registers retain
their full 64-bit repeated-byte values.

Actual STR bus writes at emulator-only addresses publish records: byte text at
`0xFFFF0000`, completion at `+4`, hexadecimal encoding of an actual eight-byte
store at `+8`, and record start at `+16`. These addresses are instrumentation,
not Switch hardware. The device encodes values and row boundaries; it has no
expected arithmetic or guest pass verdict. The production frontend records the
core's actual CRC, scheduling count and instruction attempts in the file wrapper.
The Python oracle uses unbounded signed/unsigned arithmetic and checks exact
identity, row widths/count/order, completion, trailer and end-of-file.

The executed mutant performs EOR to flip one result bit before capture; all
**384 short-domain cases** fail. Five isolated field mutations fail exactly one
row each. Seven missing/malformed/domain/END/trailer/trailing-stream mutations
produce ERROR. Raw streams and JSON reports are retained in
`build/artifacts/owned/switch_arithmetic/`.

```bash
python3 tests/roms-src/build.py switch/arithmetic_registers --original-only --check
cmake --build build --target veloce switch_plugin -j
python3 tests/roms-src/switch/arithmetic_registers/run.py
```

This exhausts the stated repeated-byte/width/carry/form domain, not full-width
operand pairs, full incoming NZCV combinations in the guest, other instruction
families or native hardware. The component separately tests all incoming flags
and additional integer domains. [Implementation and remaining scope](../../../../cores/switch/README.md).
