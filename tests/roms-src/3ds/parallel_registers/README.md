# Original ARM11 parallel arithmetic and SEL register corpus

Original MIT cartridge, assembler and independent oracle. No SDK, firmware,
retail assets or external emulator code. Reuses only the original assembler
from `../reversal_registers/build.py`. Rebuild with `python3 build.py`.

## Implemented behavior

The [ARM Architecture Reference Manual DDI0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a)
provides the ARMv6 encodings. The newer [Arm Instruction Set Reference Guide](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab),
UASX/USAX entries, clarifies unsigned subtraction GE as **no borrow**.
ASX/SAX were called ADDSUBX/SUBADDX in DDI0100I.

| Family | ADD16 | ASX | SAX | SUB16 | ADD8 | SUB8 |
|---|---|---|---|---|---|---|
| Signed wrapping | SADD16 | SASX | SSAX | SSUB16 | SADD8 | SSUB8 |
| Signed saturation | QADD16 | QASX | QSAX | QSUB16 | QADD8 | QSUB8 |
| Signed halving | SHADD16 | SHASX | SHSAX | SHSUB16 | SHADD8 | SHSUB8 |
| Unsigned wrapping | UADD16 | UASX | USAX | USUB16 | UADD8 | USUB8 |
| Unsigned saturation | UQADD16 | UQASX | UQSAX | UQSUB16 | UQADD8 | UQSUB8 |
| Unsigned halving | UHADD16 | UHASX | UHSAX | UHSUB16 | UHADD8 | UHSUB8 |

Arithmetic is independent within each byte/halfword. Exchange forms swap the
second operand's halfwords; ASX subtracts low/adds high, SAX adds low/subtracts
high. Signed wrapping sets GE from the unwrapped result's sign. Unsigned
wrapping sets GE for carry on addition or no borrow on subtraction. Halfword
GE bits update in pairs. Saturating and halving families preserve all flags,
including Q and GE. Halving discards bit zero of the wide result; negative odd
values round down. SEL chooses each byte from Rn when its GE bit is set, else
Rm, and preserves flags. All sources are read before aliased writes.

ARMv6 CPSR_s/CPSR_fs register/immediate writes expose GE, preserving reserved
bits; fs also writes NZCVQ. Full control/extension fields, banked registers,
privilege enforcement and physical exceptions remain pending. Reserved MSR source bits stop; attempts
to set T/J source bits stop in privileged modes and are ignored in User mode.
Earlier CPU
profiles, PC operands and reserved parallel allocations stop diagnostically.
No Thumb-2 SIMD instructions or physical timing are claimed.

## Actual guest files and finite domains

Both ARM11 contexts execute **19,922,944 snapshots / 179,306,496 actual register
words**, 152 groups per context, each with 65,536 stimuli:

- 24 byte-arithmetic groups: six families × two operations × two NZCVQ/GE
  surrounds (all clear/all set). Each sweeps every pair of byte operands.
- 96 halfword groups: six families × four operations × two right words
  (0x7fff8000, 0xffff0001) × both flag surrounds. Each sweeps a full low left
  halfword and its complemented high halfword.
- 32 SEL groups: all 16 GE masks × both NZCVQ surrounds, each sweeping every
  selected byte operand pair.

Byte/SEL sources are left=[x,x,255-x,255-x] and right=[y,255-y,y,255-y] in
little-endian byte order, where x/y enumerate all 256² pairs. These cover all
operand pairs at each byte position, not all independent combinations of four
lanes. Halfword tests use two fixed right words, not all 16-bit operand pairs.

Each row contains nine actual words: arithmetic result, CPSR, unchanged left,
unchanged right, requested instruction, input CPSR, result of a following SEL,
CPSR after SEL and guard 0x89abcdef. The follow-up SEL consumes the actual GE
flags without reseeding them. Guest completion is not a verdict; no framebuffer
is graded. The V3DSP001 original debug container starts ARM11_0/1 at
0x20000000/0x20040000 and an ARM9 stub at 0x08000000. Context1 waits at RAM
0x2003f000 for deterministic block ordering. This is not native firmware boot.

## Independent grading and negative controls

```sh
ctest --test-dir build -R '^(arm_parallel_test|ctr_parallel_registers|ctr_parallel_checkpoint_test)$' --output-on-failure
```

The host oracle independently defines encodings, groups, signed lane conversion,
unbounded arithmetic, floor division and byte selection. It imports only the
ROM constructor. It checks exact file identity/CRC, nine-word rows, both domains,
block completion and strict trailer. Decoder, emulator/core, oracle, generators,
recipe and archived ROM hashes must remain fixed during capture and grading.

The **2,432-case** short cartridge executes eight selected values per group in
both contexts. Executed wrong-result, wrong-Q, wrong-GE and wrong-SEL controls
reject every case; wrong-source rejects **2,312** cases. Wrong-GE actually writes
a flipped GE bit before the consumer. All nine field faults and seven malformed/
incomplete streams reject. The archive reproduces: **9,940 bytes**, SHA256
`0917fb6a837ddc4d638693d90aae92b6404123e11c55a432b597889b4371a3fc`.

The component gate checks **32,198,238 observations**: full byte-pair sweeps,
full selected halfword sweeps with eight right boundary words, all ordinary
conditions and NZCVQ/GE states on boundary data, all 15³ valid register fields
and aliases for 37 forms, all reserved family/op allocations, invalid fixed
fields and ARM7/ARM9/PC diagnostics. GE register writes sweep all 65,536 upper
halfwords with two old flag surrounds: allocated inputs are graded, reserved/
execution-state source inputs reject diagnostically. Immediate writes cover all
256 values and 16 rotations with the same defined-input/diagnostic distinction.
Conditional GE writes and User-mode T/J source handling are checked. The prior decoder fails the new result/GE gate.

Native checkpoint prefixes at frames 0/1/3 compare actual register text and all
**135,791,096 bytes** of state after two ordinary versus two fast netplay frames.
These are first-group prefixes, not checkpoint coverage of every SIMD family.
Existing saturation/extension and DS/DSi/3DS decoder/machine/network gates check
shared-code regressions. The ABI and native state format do not change.

Remaining CPU/media/system instructions, MMU/SCTLR/unaligned/exception modes,
physical timing, firmware/native retail boot, graphics/audio and native analog,
touch/wireless devices remain unfinished. 3DS remains **30%** of the fixed
milestone checklist, not hardware accuracy. This increment has no new private
Mario 64 capture.

Verification for 0.5.0: full Release build passes; all **142 original archives**
reproduce. All **22 selected gates** passed the first candidate; after the MSR
source diagnostic correction, all **nine affected register/checkpoint/machine/
network gates** pass on the final binary. The repeated full register capture
has zero mismatches and stable source/binary hashes. The native checkpoint gate
compares **5,657 actual register rows** across three continuation prefixes.
The earlier decoder fails the arithmetic gate; the draft without the MSR guard
fails the reserved-source rejection gate. Completion renderer and whitespace
checks pass. CI/CD remains disabled.
