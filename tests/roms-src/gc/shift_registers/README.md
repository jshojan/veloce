# Original GameCube shift and unary register corpus

`shift_registers.vgc` is repository-owned guest code with input stimuli,
loops, generated instructions and register captures. There are no expected
answers or guest pass verdicts. The actual plugin executes through the headless
frontend; the independent host grades every captured bit. Each full run matches
**4,416,528/4,416,528 nine-word snapshots**.

## Declared finite domain

| Instruction | Domain | Snapshots |
| --- | --- | ---: |
| SLW/SRW/SRAW | 3 forms × 16 sources × all 64 six-bit counts × 4 upper-count patterns × all 8 incoming SO/OV/CA states × both Rc | 196,608 |
| SRAWI | 16 sources × all 32 immediate shifts × all 8 flags × both Rc | 8,192 |
| EXTSB | all 256 byte values × 4 upper-source patterns × all 8 flags × both Rc | 16,384 |
| EXTSH | all 65,536 halfword values × 4 upper-source patterns × all 8 flags × both Rc | 4,194,304 |
| CNTLZW | 65 source stimuli covering zero and all leading-zero counts × all 8 flags × both Rc | 1,040 |
| Total | independently graded observations | 4,416,528 |

Shift sources are `0,1,2,15,16,127,128,255,7fff,8000,7fffffff,80000000,
fffffffe,ffffffff,55555555,89abcdef` (small numeric-only values are decimal,
others hexadecimal). Register-count upper patterns are `00000000,00000040,
80000000,ffffffc0`, added to every low-six-bit count. Byte upper-source patterns
are `00000000,12345600,80000000,ffffff00`; halfword upper-source patterns are
`00000000,12340000,80000000,ffff0000`. CNTLZW uses zero, each of 32 single-bit
words and each of 32 low-bit-filled prefixes (some words repeat); this includes
every possible leading-zero result from 0 through 32.

CR seeds are `00000000,ffffffff,a5a5a5a5,12345678`. Shift-register seeds use
`(source-index + upper-index + flags + Rc) % 4`; immediate shifts omit the upper
index, extensions omit the source index, and CNTLZW uses the stimulus index.
Every CR word is not crossed with every operand. XER byte count is 127.
RS=r3, RA=r11 and register-count RB=r10 are fixed. SRAWI and unary forms preserve
r10=`2468ace0`. All other stimulus registers are ordinary GPRs.

## Captures and independent grading

Each `REG r=` row captures nine 32-bit words:

1. Result in RA (r11).
2. CR.
3. XER.
4. Source A (r3).
5. Shift-count source B (r10).
6. Instruction encoding (r4).
7. Initial CR (r5).
8. Initial XER (r9).
9. Untouched sentinel (r6=`12345678`).

The host uses integer multiplication/division, signed floor division and
remainders, modular sign extension and integer bit length. It imports no CPU
implementation or builder stimulus iterator. The builder is imported only for
reproduction and executed failure controls. Parsing requires identity/CRC/
channel, suite/domain, exact width/count, completion marker, ungraded status=-1
trailer and EOF.

The reduced positive image has 3,856 cases. Executed result/carry/count mutants
fail exactly 3,856/640/1,280 snapshots. The count mutant uses a separate 3,072-case
register-shift domain, masks the execution count to five bits and preserves the
captured original count and instruction. The carry mutant restores initial XER
after the tested operation. Nine independent field mutations fail one row each;
seven malformed, wrong-domain, truncated or false-verdict streams return ERROR.

## Component coverage and semantics

Across both profiles **37,042,208** additional shift/unary component snapshots
match. Checks repeat the ROM domain, enumerate every register shift RS/RA/RB
encoding on eight count boundaries/all flags/Rc, every SRAWI RS/RA/SH encoding,
all unary RS/RA encodings on sixteen source patterns, walking/complemented
source bits at all register counts, and all 128 XER byte counts at boundaries.
Complete GPR arrays, CR, XER, LR, CTR, PC and instruction count are checked.
All aliases and ordinary GPR0 reads are included. Every nonzero reserved unary
RB field is rejected without architectural register mutation. Known guest
captures and exact state replay pass through both loaded plugins, including a
machine snapshot before SRAWI changes CA from zero to one.

SLW/SRW use the low six count bits; counts 32–63 give zero, while upper bits are
ignored. Arithmetic right shifts replicate the sign and update only XER.CA:
CA is set for negative inputs with discarded ones, cleared otherwise, including
zero shifts. SRAWI uses all five encoded shift bits. EXTSB/EXTSH ignore source
bits above their widths; CNTLZW on zero returns 32. Rc=1 records the final result
and incoming SO in CR0, preserving other CR fields. Other than arithmetic-shift
CA, these instructions preserve XER.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_shift_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/gc/shift_registers/run.py --controls-only
```

Raw `.result` files (~333 MiB each) and JSON grading reports remain under
`build/artifacts/owned/gc_shifts/`. CI/CD stays disabled; run locally.

## Limits and references

All extension input bits and count fields are exhausted within the declared
upper/source/flag patterns. Full 32-bit source/count pairs and CR/XER words are
not exhausted. Original fixed-field ROM evidence is separate from component
field/alias coverage. Supplied flat RAM exposes generated instructions without
physical cache synchronization. Marked test images and `ppcdebug` addresses are
test conventions. Physical cache/timing/exceptions, Gekko/Broadway-specific
quirks, retail boot and most console hardware remain pending;
[core limits](../../../../cores/ppc/README.md).

Encodings and architecture semantics follow the [NXP instruction manual](https://www.nxp.com/docs/en/reference-manual/MPC82XINSET.pdf),
pp. 31, 57–58 and 118–121. IBM also documents [SRAW](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-sraw-sra-shift-right-algebraic-word-instruction).
These references do not establish physical console timing.
