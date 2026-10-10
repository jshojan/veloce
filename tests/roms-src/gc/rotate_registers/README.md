# Original GameCube word rotate/mask register corpus

`rotate_registers.vgc` is a repository-owned program with input stimuli,
generated instructions, guest loops and register capture. It contains no
expected answers or pass verdict. The actual plugin executes it through the
headless frontend; the independent host grades every captured bit. Each full
run matches **9,437,184/9,437,184 ten-word snapshots**.

## Declared finite domain

| Instruction | Domain | Snapshots |
| --- | --- | ---: |
| RLWIMI | all 32 SH × all 1,024 MB/ME pairs × 8 sources × 4 initial destinations × both SO × both Rc | 4,194,304 |
| RLWINM | all 32 SH × all 1,024 MB/ME pairs × 8 sources × both SO × both Rc | 1,048,576 |
| RLWNM | all 32 low-five-bit shifts × 4 upper-count patterns × all 1,024 MB/ME pairs × 8 sources × both SO × both Rc | 4,194,304 |
| Total | independently graded guest snapshots | 9,437,184 |

Source words are `00000000,00000001,7fffffff,80000000,ffffffff,55555555,
aaaaaaaa,89abcdef`; insert destinations are `00000000,ffffffff,12345678,
a5a5a5a5`. RLWNM upper-count patterns are `00000000,00000020,80000000,
ffffffe0`, added to every low-five-bit count. Upper bits do not affect the
rotation. RLWINM/RLWNM start RA at zero. CR seeds are `00000000,ffffffff,
a5a5a5a5,12345678`, chosen by `(source-index + destination-index + SO + Rc) % 4`;
all seeds are not crossed with every case. Incoming OV/CA are both set, and XER
byte count is 127. The ROM uses RS=r3, RA=r11, RB=r10 (ordinary GPRs).

All mask pairs include full/wrapped and single-bit boundaries; generated
masks always have at least one selected bit. PowerPC MB/ME number bits from the
most significant end, inclusively. MB>ME wraps through bit 31 then bit 0;
MB=ME+1 gives a full mask. Zero shifts remain defined. RLWIMI preserves the
unselected old destination bits. Rc=1 updates CR0 from the final result and
incoming XER.SO; other CR fields and all XER bits are preserved.

## Observations and grading

Each `REG r=` row captures ten 32-bit words:

1. Result in RA (r11).
2. CR.
3. XER.
4. Source A (r3).
5. Shift source B (r10); immediate forms preserve `2468ace0`.
6. Initial destination (r16).
7. Instruction encoding (r4).
8. Initial CR (r5).
9. Initial XER (r9).
10. Untouched sentinel (r6=`12345678`).

The host builds masks by walking bit positions in a circle, rotates words with
integer multiplication/division, and replaces CR0 with arithmetic. It imports
no CPU code or builder stimulus iterator. The builder is imported only to
reproduce the image and execute mutation controls. Parsing requires file
identity/CRC/channel, suite/domain, exact widths/count, completion marker,
ungraded status=-1 trailer and EOF. The reduced positive control has 3,584 cases.
Executed result/mask/insert mutants fail exactly 3,584/1,472/496 snapshots;
ten independent field mutations each fail one row, and seven malformed,
truncated, wrong-domain or false-verdict streams return ERROR.

Component tests additionally match **113,258,496** rotate/mask snapshots across
both profiles: the declared main patterns with all eight SO/OV/CA combinations,
every RS/RA/RB-or-SH field encoding on eight boundary masks, every walking source
bit/shift/mask with both Rc states, and all 128 XER byte counts at a wrapped
mask. Aliases include RS=RA, RA=RB and all-three equal, including ordinary GPR0.
Complete GPR arrays, CR, XER, LR, CTR, PC and instruction count are checked.
Known guest captures and exact replay pass through both plugins, with a machine
state restored immediately before an RLWNM with RA=RB.

```sh
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_rotate_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
# Run only executed controls and parser rejection checks:
python3 tests/roms-src/gc/rotate_registers/run.py --controls-only
```

Raw `.result` files (~783 MiB each) and JSON reports remain under
`build/artifacts/owned/gc_rotates/`. CI/CD remains disabled; run locally.

## Limits and references

This exhausts encoded shifts/masks within the declared operand/flag patterns;
it does not exhaust all 32-bit source/destination pairs or CR/XER words. The
original ROM uses fixed GPR fields; exhaustive field/alias checks are component
evidence. The supplied flat RAM exposes generated instructions without
physical cache synchronization. Original marked images and `ppcdebug` output
addresses are test conventions. Retail boot, physical cache/timing/exceptions,
Gekko/Broadway-specific differences and most console hardware remain pending;
[core limits](../../../../cores/ppc/README.md).

The [NXP instruction manual](https://www.nxp.com/docs/en/reference-manual/MPC82XINSET.pdf)
(pp. 113–116) supplies encodings, mask/insert semantics and CR0 behavior; the
[MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf)
Table 2-25 identifies the three rotate forms. IBM documents the low-five-bit
count for [RLWNM](https://www.ibm.com/docs/en/aix/7.2.0?topic=is-rlwnm-rlnm-rotate-left-word-then-mask-instruction)
and mask behavior for [RLWINM](https://www.ibm.com/docs/en/aix/7.1.0?topic=is-rlwinm-rlinm-rotate-left-word-immediate-then-mask-instruction).
These architecture references do not establish physical console timing.
