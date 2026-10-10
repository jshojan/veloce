# Original Wii multiply/divide register corpus

`multiply_registers.vwi` is repository-owned guest code containing stimuli,
generated instructions and register capture, with no expected answers or guest
pass verdict. The plugin executes through the headless frontend. Each full run
matches **8,421,376 nine-word observations**, including **8,420,320 defined
results** and **1,056 undefined division results whose defined flags and
preserved fields are graded**.

## Declared finite domain

| Family | Domain | Rows |
| --- | --- | ---: |
| MULHW/MULHWU | 2 forms × 16² operands × 8 SO/OV/CA states × 2 Rc | 8,192 |
| MULLW/DIVW/DIVWU | 3 forms × 16² operands × 8 SO/OV/CA states × 2 OE × 2 Rc | 24,576 |
| MULLI | 16 sources × 8 SO/OV/CA states × all 65,536 immediates | 8,388,608 |
| Total | Captured observations | 8,421,376 |

Operands are `0,1,2,15,16,127,128,255,7fff,8000,7fffffff,80000000,
fffffffe,ffffffff,55555555,89abcdef` (numeric-only small values are decimal,
others hexadecimal). CR seeds are `00000000,ffffffff,a5a5a5a5,12345678`,
selected by `(sourceA-index + sourceB-index + flags + OE + Rc) % 4` for
register forms and `(source-index + flags) % 4` for immediates. Every seed is
not crossed with every input. XER byte count is 127. RT=r11, RA=r3 and RB=r10
are fixed. For MULLI, r10 remains `2468ace0`.

MULHW/MULHWU return signed/unsigned high product words; MULLW and signed
MULLI return low words. MULLI sign-extends its immediate and preserves CR/XER.
DIVW truncates toward zero; DIVWU divides unsigned. Record forms compare the
32-bit result as signed, including MULHWU and DIVWU. OE updates OV and sets
sticky SO on overflow, preserving CA; Rc copies the updated SO into CR0.

For division by zero and signed MIN/-1, the architecture leaves quotient and
recorded CR0.LT/GT/EQ undefined. The host masks just these bits, while grading
CR0.SO, other CR fields, XER and all preserved capture fields. The supplied
profile chooses quotient zero and continues; that deterministic choice is
component fixture evidence, not a physical hardware result claim.

## Independent grading and controls

Every row captures nine words: result, CR, XER, source A, source B, original
instruction, initial CR, initial XER and r6 sentinel `12345678`.
The Python host uses arbitrary precision multiplication, high-word arithmetic,
absolute-magnitude quotient division and independent signed flag calculations.
It imports no CPU implementation or builder stimulus iterator; builder import
is for reproduction and executed controls. Parsing requires exact identity,
ROM CRC/channel, suite/domain, row width/count, completion markers, ungraded
status=-1 trailer and EOF.

The reduced positive image has 384 rows (328 defined results, 56 undefined).
Executed wrong-result, low-instead-of-high and stale-XER mutants fail exactly
328, 32 and 54 rows. Nine capture-field mutations each fail one row; seven
invalid streams return ERROR. Mutating every undefined quotient and recorded
LT/GT/EQ bit still passes. Separate mutations of defined XER.OV and CR0.SO on
undefined rows each fail one row. Raw files (~634 MiB per profile) and JSON
reports stay in `build/artifacts/owned/wii_products/`.

## Component coverage

Across both profiles, 26,443,776 component snapshots match: 26,163,136 defined
results and 280,640 deterministic undefined-result profile fixtures. The
independent C++ oracle uses four 16-bit multiplication limbs and restoring
unsigned division on magnitudes. Checks cover the original domain, every
RT/RA/RB encoding on selected operands, destination/source aliases, ordinary
GPR0, walking/complemented bits and all 128 XER byte counts. Complete GPR
arrays, CR/XER/LR/CTR, PC and instruction counts are checked. Reserved
high-multiply bit-10 variants reject without architectural register mutation.
Known original guest captures and exact plugin/state replay also pass,
including a snapshot immediately before MULLWO. updates OV/SO. The full
component gate passes undefined-behavior sanitization.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_multiply_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/wii/multiply_registers/run.py --controls-only
```

CI/CD stays disabled; these gates run locally. Every 16-bit immediate is
covered on the declared patterns; full 32-bit operand pairs are not exhausted.
Original fixed-field evidence is separate from component register/alias
coverage. Flat RAM exposes generated instructions without physical cache
synchronization. Timing, cache/MMU, physical exceptions, Gekko/Broadway quirks,
retail boot and most console hardware remain pending; see [core limits](../../../../cores/ppc/README.md).

Semantics and encodings follow the [NXP PowerPC instruction set manual](https://www.nxp.com.cn/docs/en/reference-manual/MPC82XINSET.pdf),
including its DIVW/DIVWU undefined-result rules. These references do not
establish measured console timing.
