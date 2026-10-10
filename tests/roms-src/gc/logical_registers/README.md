# Original GameCube integer logical register corpus

`logical_registers.vgc` is a repository-owned program containing input
stimuli, instruction-generation loops and register capture. It contains no
expected results or guest pass verdict. The actual ABI v2 plugin executes it
through the headless frontend; `run.py` independently grades every captured bit.
The full run matches **6,324,224/6,324,224 nine-word snapshots**.

## Declared finite domain

| Family | Domain | Snapshots |
| --- | --- | ---: |
| AND/ANDC/OR/ORC/XOR/NAND/NOR/EQV | 16 operand patterns squared × all 8 incoming SO/OV/CA states × both Rc forms | 32,768 |
| ORI/ORIS/XORI/XORIS/ANDI./ANDIS. | all 65,536 immediates × 8 source patterns × both incoming SO states | 6,291,456 |
| Total | independently graded observations | 6,324,224 |

Register operand patterns are `0,1,2,15,16,127,128,255,7fff,8000,7fffffff,
80000000,fffffffe,ffffffff,55555555,89abcdef` (hex for values containing letters
or more than three digits). CR seeds are `00000000,ffffffff,a5a5a5a5,12345678`,
selected by `(source-A-index + source-B-index) % 4`; the corpus does not cross
every CR seed with every operand pair. Incoming XER byte count is 127.
Immediate source patterns are `00000000,00000001,7fffffff,80000000,ffffffff,
55555555,aaaaaaaa,89abcdef`; OV/CA are both set, and the CR seed is `a5a5a5a5`
for SO=0 or `12345678` for SO=1. Immediate bit 0 is operand data, including for
ANDI./ANDIS., which always update CR0. The shifted forms zero the low 16 operand
bits; no immediate is sign-extended.

The ROM uses RS=r3, RA=r11, RB=r10. Component tests separately cover every
RS/RA/RB encoding, all aliases (including GPR0), both Rc forms/all XER flags on
selected operands, every bit's four truth inputs, all immediate encodings on
the declared patterns, and every immediate RS/RA encoding on eight boundary
immediates. Across both profiles **21,856,256** integer logical component
snapshots match. Complete GPR arrays, CR, XER, LR, CTR, PC and instruction count
are checked. Known aliased guest captures and exact state replay pass through
both plugins, including a snapshot immediately before aliased EQV.

## Register output and independent oracle

Each `REG r=` row captures nine 32-bit words in this order:

1. RA result (r11).
2. CR, including recorded CR0 and preserved other fields.
3. XER, which all these instructions preserve.
4. Source A (r3).
5. Source B (r10).
6. Executed instruction encoding (r4).
7. Initial CR (r5).
8. Initial XER (r9).
9. Untouched sentinel (r6=`12345678`).

The host uses numeric truth tables over byte pairs, and arithmetic CR-nibble
replacement; it imports no CPU implementation or builder stimulus iterator.
The builder is imported only to reproduce the ROM and execute failure controls.
The strict parser requires identity/CRC/channel, suite/domain, exact row width
and count, completion marker, ungraded status=-1 trailer, and end of file.
Three executed reduced-image mutants must fail exactly 512 result, 192 CR and
512 XER snapshots. Each of nine independent field mutations must fail one row;
seven malformed/truncated/wrong-domain/false-verdict streams must return ERROR.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_logical_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Raw `.result` observations and JSON grading reports are retained under
`build/artifacts/owned/gc_logical/`. Each full result is about 477 MiB.
CI/CD is disabled; these commands run locally.

## Limits and instruction references

The supplied flat RAM profile exposes runtime-generated instructions without
physical instruction-cache synchronization. The original marked format and
synthetic `ppcdebug` capture addresses are not retail formats or Nintendo MMIO.
This verifies the declared finite instruction domain, not every 32-bit operand
pair, every CR/XER value, physical cache/timing/exception behavior, or console
completion. Gekko/Broadway-specific differences, retail boot and most hardware
remain pending. See [core coverage](../../../../cores/ppc/README.md).

Instruction encodings follow IBM's [PowerPC instruction appendix](https://www.ibm.com/docs/de/aix/7.1.0?topic=reference-appendix-f-powerpc-instructions).
Record and preservation semantics follow [ANDC](https://www.ibm.com/docs/en/aix/7.1.0?topic=set-andc-complement-instruction),
[EQV](https://www.ibm.com/docs/de/ssw_aix_71/assembler/idalangref_eqv_instrs.html)
and the [NXP MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf).
These references establish architecture semantics, not measured console timing.
