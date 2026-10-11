# Original ARM11 dual-halfword multiply register corpus

Original MIT source, assembler and cartridge; no SDK, firmware, proprietary ROM
or external emulator implementation is included. Reuses only the original
assembler in `../reversal_registers/build.py`. Rebuild with `python3 build.py`.

## Defined behavior

[ARM Architecture Reference Manual DDI0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a),
A4.1.75, 78, 80, 81, 85 and 89 defines these ARMv6 instructions:

| Forms (each has an X exchange variant) | Result |
|---|---|
| SMUAD | Sum of two signed 16×16 products, modulo 2^32; overflow sets sticky Q |
| SMUSD | Bottom product minus top product, modulo 2^32; flags preserved |
| SMLAD / SMLSD | Sum/difference plus signed 32-bit accumulator; final signed overflow sets sticky Q |
| SMLALD / SMLSLD | Sum/difference plus 64-bit destination pair, modulo 2^64; flags preserved |

X exchanges the two halfwords of the second operand before multiplication.
Accumulator field 15 selects SMUAD/SMUSD and does not read PC. All instructions
preserve NZCV/GE; 32-bit forms never clear Q. Long destinations must be distinct.
Inputs and accumulators are read before aliased writes. Earlier profiles, PC
operands, reserved bit 7 and duplicate long destinations stop diagnostically.
Physical undefined exceptions, early termination and instruction timing remain
pending. ARMv4/v5 profiles for DS/DSi do not gain these instructions.

## Actual guest domain

The original `.v3ds` image emits **12,582,912 snapshots / 125,829,120 actual
register words**, from both ARM11 contexts:

- Twelve forms: six instructions × normal/exchanged second operand.
- Four right words: 0x80008000, 0x7fff8000, 0xffff0001, 0x00010001.
- Two NZCVQ/GE surrounds: all clear/all set.
- Complete low-halfword values 0–65535, with left high halfword fixed at 0x8000.
- Accumulator/low seeds 0xffffffff and 0x7fffffff; high seeds 1 and 0x80000000,
  correlated with flag surround. Long forms interpret these as one 64-bit pair.

Each context has 96 groups × 65,536 selected values. The domain includes the
SMUAD +2^31 overflow and SMLAD cancellation with seed -1, exchange effects on
asymmetric right words, positive/negative products and long carry/borrow.
These fixed surrounds and correlated seeds are **not all independent operand
or accumulator pairs**. Wider component surrounds and flag states supplement
this finite guest corpus.

Rows have ten actual words: low/result register, high/preserved register,
CPSR, unchanged left, unchanged right, requested instruction, input CPSR,
original low seed, original high seed, guard 0x89abcdef. Separate registers hold
the original seeds before execution. Text prefix output precedes MSR reseeding;
MRS captures immediately after the tested instruction. The output helper
preserves the captured words. No framebuffer, visual result or guest verdict
is graded.

V3DSP001 loads ARM11_0/1 at 0x20000000/0x20040000, with an ARM9 completion stub
at 0x08000000. Context1 waits on original shared RAM 0x2003f000 for block order.
This synthetic container does not establish native firmware/retail boot or
physical multicore/cache timing. ABI and state formats remain unchanged.

## Independent oracle and executed controls

```sh
ctest --test-dir build -R '^(arm_dual_media_multiply_test|ctr_dual_media_multiply_registers|ctr_dual_media_multiply_checkpoint_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

The oracle imports only the constructor. It independently enumerates encodings
and domains, converts signed byte values, indexes the exchanged halfwords and
uses unbounded integer arithmetic before modulo splitting. Exact core/ROM CRC,
all ten fields, two ordered domains, completion, ungraded trailer and EOF are
required. Emulator/core, decoder, oracle, recipe, archive and assembler/generator
hashes must remain fixed through capture and grading.

The **1,536-case** short image executes eight values in every group/context.
Wrong-result and wrong-Q mutations reject all 1,536 rows; removing accumulation
rejects all **1,024 accumulating rows**; flipped exchange rejects **720** rows;
flipped subtraction and wrong source reject all **1,536** rows each. All ten
single-field faults and seven malformed/missing/false-completion streams reject.
The archive reproduces: **6,756 bytes**, SHA256
`703ad6e676d0531df90314287e1c4015247c9552a307b6f13f1eb21bccbe773f`.

The component gate passes **55,320,578 observations**: complete selected
halfwords at both positions with zero/one surrounds, eight right boundary words,
two flag surrounds and all forms; boundary operands/seeds with every ordinary
condition and NZCVQ/GE state; all valid register fields/aliases and exhaustive
invalid profile/PC/reserved/destination encodings. Its long oracle carries the
signed delta through a low-word addition separately from the core's 64-bit
accumulator equation. Literal overflow/cancellation and negative borrow anchors
are tested. The prior decoder fails the actual new result gate.

Checkpoint prefixes at frames 0/1/3 compare **5,450 actual register rows** and complete
**135,791,096-byte** native states after two ordinary versus fast netplay frames.
These are first SMUAD group prefixes, not checkpoints for every form. Whole
CPU/media/system/MMU/exception/privilege behavior, physical timing, firmware/
retail boot, graphics/audio and analog/touch/wireless devices remain unfinished.
3DS remains **30%** of the fixed delivery checklist. No new private Mario capture
is included; CI/CD remains disabled.

Verification: full Release build and all **22 selected gates** pass, including
the adjacent **7,340,032-case** high-word/UMAAL image. The new **12,582,912-case**
image has zero mismatches with unchanged capture provenance. Checkpoints compare
**5,450 actual rows** and complete states. All **145 original archives** reproduce;
completion rendering and whitespace checks pass. CI/CD remains disabled.
