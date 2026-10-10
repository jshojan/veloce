# Original Wii carry arithmetic register corpus

`carry_registers.vwi` is repository-owned guest code with input stimuli,
generated instructions, loops and register capture. It has no expected answers
or guest pass verdict. The actual plugin executes through the headless frontend;
the independent host grades every captured bit. Each full run matches
**25,168,384/25,168,384 nine-word snapshots**.

## Declared finite domain

| Family | Domain | Snapshots |
| --- | --- | ---: |
| NEG/ADDZE/ADDME/SUBFZE/SUBFME | 5 forms × 16 source patterns × all 8 incoming SO/OV/CA states × both OE × both Rc | 2,560 |
| ADDIC/ADDIC./SUBFIC | 3 forms × all 65,536 immediate values × 16 sources × all 8 incoming SO/OV/CA states | 25,165,824 |
| Total | independently graded observations | 25,168,384 |

Source patterns are `0,1,2,15,16,127,128,255,7fff,8000,7fffffff,80000000,
fffffffe,ffffffff,55555555,89abcdef` (small numeric-only values are decimal,
others hexadecimal). CR seeds are `00000000,ffffffff,a5a5a5a5,12345678`, chosen
by `(source-index + flags + OE + Rc) % 4` for register forms and
`(source-index + flags) % 4` for immediates. Every CR word is not crossed with
every input. XER byte count is 127. RT=r11 and RA=r3 are fixed; reserved unary
RB fields are zero. r10=`2468ace0` is an untouched source sentinel.

The carry immediates sign-extend 16-bit operands, overwrite CA and preserve
SO/OV; ADDIC. always records CR0, while ADDIC/SUBFIC preserve all CR fields.
Bit 0 is immediate data, not Rc. GPR0 is an ordinary source for these forms.
The four extended unary forms consume incoming CA and update carry out; NEG
preserves CA. OE=1 updates OV and only sets sticky SO on overflow, and Rc=1
records the final result and updated SO in CR0. Other CR fields and XER byte
count are preserved.

## Captures and independent grading

Every `REG r=` row captures nine 32-bit words:

1. RT result (r11).
2. CR.
3. XER.
4. Source A (r3).
5. Untouched r10 (`2468ace0`).
6. Instruction encoding (r4).
7. Initial CR (r5).
8. Initial XER (r9).
9. Untouched sentinel (r6=`12345678`).

The host uses signed mathematical arithmetic, unsigned no-borrow/carry
inequalities, modular 32-bit results and arithmetic flag/nibble replacement.
It imports no CPU code or builder stimulus iterator. The builder is imported
only for image reproduction and executed controls. Parsing requires identity/
CRC/channel, suite/domain, exact row width/count, completion marker, ungraded
status=-1 trailer and EOF.

The reduced positive image has 272 cases. Executed result/stale-CA/cleared-SO
mutants must fail exactly 272/120/140 snapshots. The stale-CA mutant replaces
only output CA with its initial value, preserving actual OV/SO and CR; the SO
mutant clears only SO after execution. Nine independent field mutations each
fail one row, and seven malformed, wrong-domain, truncated or false-verdict
streams return ERROR. Raw `.result` files (~1.85 GiB each) and JSON reports stay
under `build/artifacts/owned/wii_carry/`.

## Component coverage

Across both profiles **56,083,456** additional carry arithmetic component
snapshots match. These include the ROM domain, every unary RT/RA encoding on
sixteen sources/all flags/OE/Rc, every immediate RT/RA encoding on eight boundary
immediates/all flags, walking and complemented source bits, and all 128 XER
byte counts. Checks include ordinary GPR0, destination/source aliases, complete
GPR arrays, CR/XER/LR/CTR, PC and instruction count. Every nonzero reserved unary
RB field rejects before architectural register mutation. Known guest captures
and exact state replay pass through both plugins, including a machine snapshot
immediately before NEGO. sets OV/SO while preserving CA.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_carry_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/roms-src/wii/carry_registers/run.py --controls-only
```

CI/CD stays disabled; these gates run locally.

## Limits and references

All 16-bit immediate values are exhausted within the declared operand/flag
patterns; full 32-bit sources and CR/XER words are not. Original fixed-field
ROM evidence is separate from component alias/field coverage. Supplied flat
RAM exposes generated instructions without physical cache synchronization.
Marked test images and `ppcdebug` addresses are test conventions. Physical
cache/timing/exceptions, Gekko/Broadway-specific quirks, retail boot and most
console hardware remain pending; [core limits](../../../../cores/ppc/README.md).

Instruction semantics and encodings follow IBM's [Assembler Language Reference](https://public.dhe.ibm.com/systems/power/docs/aix/53/alangref.pdf)
and [instruction table](https://www.ibm.com/docs/en/aix/7.3.0?topic=reference-appendix-b-instruction-set-sorted-by-mnemonic).
Specific references include [ADDZE](https://www.ibm.com/docs/en/aix/7.1.0?topic=set-addze-aze-add-zero-extended-instruction),
[SUBFZE](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-subfze-sfze-subtract-from-zero-extended-instruction),
[SUBFME](https://www.ibm.com/docs/en/aix/7.1.0?topic=is-subfme-sfme-subtract-from-minus-one-extended-instruction),
[ADDIC](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-addic-ai-add-immediate-carrying-instruction)
and [ADDIC.](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-addic-ai-add-immediate-carrying-record-instruction).
These architecture references do not establish measured console timing.
