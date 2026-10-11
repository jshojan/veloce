# Original ARM11 high-word and unsigned wide multiply register corpus

Original MIT cartridge, independent host oracle and assembler. No SDK,
firmware, retail assets or external emulator code. Reuses only the original
assembler in `../reversal_registers/build.py`. Rebuild with `python3 build.py`.

## Behavior

[ARM Architecture Reference Manual DDI0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a),
A4.1.82–84 and A4.1.127, defines these ARMv6 instructions:

| Form | Arithmetic before extracting the result |
|---|---|
| SMMUL / SMMULR | Signed 32×32 product; R adds 0x80000000 before taking the high word |
| SMMLA / SMMLAR | Add the accumulator shifted left by 32 to that product; R rounds |
| SMMLS / SMMLSR | Subtract the product from the accumulator shifted left by 32; R rounds after subtraction |
| UMAAL | Unsigned 32×32 product plus both old destination words as **separate unsigned 32-bit values**, then split into two words |

High-word arithmetic is modulo 2^64 before extracting bits 63:32. For the add
encoding, accumulator field 15 selects SMMUL and does not read PC. The subtract
encoding requires an accumulator. UMAAL requires distinct destination registers.
All forms preserve flags, including NZCVQ/GE, and read sources/accumulators
before aliased writes. Earlier CPU profiles, reserved high-word operation bits,
PC operands and duplicate UMAAL destinations stop diagnostically. Timing,
multiplier early termination and precise hardware exceptions remain pending.

## Executed guest domain

Both ARM11 contexts produce **7,340,032 snapshots / 73,400,320 actual register
words**, 56 groups per context, each with 65,536 selected values:

- Seven forms: six high-word truncating/rounded forms and UMAAL.
- Four right words: 0x80000000, 0x7fffffff, 0xffffffff, 0x00010001.
- Two NZCVQ/GE surrounds: all clear/all set.
- Left words contain a complete low halfword and its complemented high halfword.
  Old low destination/accumulator seeds are 0xffffffff and 0x7fffffff; old high
  destination seeds are 1 and 0x80000000, correlated with the flag surround.

The correlated source halves and four right seeds are finite selected domains,
not all independent 32-bit operand/accumulator pairs. Right 0x80000000 supplies
exact half-way rounding cases. The component adds zero/max values, wider
surrounds and independent carry/rounding boundary anchors.

Each row contains **ten actual register words**: low/result register, high/
preserved register, CPSR, unchanged left, unchanged right, requested instruction,
input CPSR, unchanged low seed, unchanged high seed and guard 0x89abcdef.
The seeds are held in separate registers before the instruction executes. The
line prefix is emitted before MSR reseeds flags; all values are emitted after
the tested instruction. The following output helper preserves captured inputs.
No framebuffer or guest verdict is graded.

The original V3DSP001 debug container starts ARM11_0/1 at
0x20000000/0x20040000 and an ARM9 completion stub at 0x08000000. Context1 waits
at RAM 0x2003f000 for deterministic block order. This is not firmware/native
retail boot. ABI and native state format remain unchanged.

## Independent grading and executed controls

```sh
ctest --test-dir build -R '^(arm_wide_media_multiply_test|ctr_wide_media_multiply_registers|ctr_wide_media_multiply_checkpoint_test)$' --output-on-failure
```

The host oracle independently defines groups/encodings, converts signed byte
words, uses unbounded integer products and floor division, and handles UMAAL's
independent seeds before splitting the result. It imports only the ROM
constructor. Exact file identity/CRC, ten-word rows, both block domains,
completion and strict trailer are required. Emulator/core, decoder, oracle,
recipe, archive and original assembler/generator hashes must stay fixed.

The **896-case** short image executes eight selected values in every group on
both contexts. Wrong-result and wrong-Q reject all cases. Executed replacement
of UMAAL by UMLAL rejects all **128 UMAAL cases**. Flipped rounding rejects
**416** cases; wrong source rejects **744**. All ten one-bit field faults and
seven malformed/incomplete streams reject. The full archive reproduces:
**4,508 bytes**, SHA256
`1216541b8bd39795dd9820157a55fc5c87ab93116c3f0ab6155366baa7e7a34b`.

The component gate checks **31,559,680 observations**. It sweeps complete
selected halfwords at both positions with zero/one surrounds, eight right
boundary words and both flag surrounds for every form. High-word grading uses
signed quotient/floor arithmetic; UMAAL grading independently multiplies in
16-bit limbs and carries the separate seed additions. Boundary anchors check
positive/negative half-way rounding, subtraction before rounding and the maximum
UMAAL result (all four inputs 0xffffffff produces both words 0xffffffff).
Every ordinary condition and NZCVQ/GE state is tested on boundary sources.
All valid register fields/aliases, reserved high-word operations, earlier CPU/
PC diagnostics and UMAAL destination restrictions are checked. The prior decoder
fails the actual new high-word result gate.

Native checkpoints at frames 0/1/3 compare **5,499 actual register rows** and complete
**135,791,096-byte** states after two ordinary versus two fast netplay frames.
These are first SMMUL group prefixes, not checkpoints for every multiply form.
Older ARM decoder/components, the original DS generic multiply image and DS/
DSi/3DS machine/network gates protect shared-code regressions.

Remaining dual-halfword multiply/media/system instructions, complete CPU/MMU/
SCTLR/unaligned/exception modes, privilege/banked registers, physical timing,
firmware/native retail boot, graphics/audio and analog/touch/wireless devices
remain unfinished. 3DS remains **30%** of the fixed delivery checklist, not
hardware accuracy. This increment has no new private Mario 64 capture; CI/CD
remains disabled.

Verification: the full Release build and all **21 selected gates** pass, including
the **38,338,560-case** original DS multiply regression. All **144 original
archives** reproduce. The new native corpus has zero mismatches and unchanged
capture provenance hashes; completion rendering and whitespace checks pass.
