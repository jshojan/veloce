# Original ARM11 ARMv6 extension register corpus

Original MIT assembler/cartridge/oracle; no SDK, firmware, retail assets or
external emulator code. Reuses only the original assembler from
`../reversal_registers/build.py`. Build with `python3 build.py`.

## Hardware behavior and finite domains

[ARM Architecture Reference Manual DDI 0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a),
A3.7 and the ARM/Thumb SXT/UXT instruction entries describe ARMv6 extension.
The twelve ARM forms are SXTB/SXTH/SXTB16, UXTB/UXTH/UXTB16 and their accumulate
forms SXTAB/SXTAH/SXTAB16/UXTAB/UXTAH/UXTAB16. Rotations are 0/8/16/24. Dual-byte
forms extract byte0 and byte2 after rotation and add independently modulo 2^16;
other forms add modulo 2^32. Rn=15 selects no accumulation. Flags are unchanged.
Thumb has four low-register, non-rotating scalar extension forms. Earlier CPU
profiles and unpredictable PC operands stop diagnostically; precise exceptions
remain pending.

Both ARM11 contexts execute **13,631,488 snapshots / 95,420,416 actual captured
register words**. Each has 96 ARM groups and eight Thumb groups. Every group
sweeps all 65,536 selected values and two NZCVQ surrounds (all clear/all set).
Scalar stimuli contain a complete low halfword and its complemented high half;
dual-byte stimuli scatter the two independently varying bytes into byte0/byte2
with guard bytes 0x5a/0xa5. Source formation inversely rotates the word so all
selected values are exercised at each instruction rotation. Accumulator seeds
are 0x7fffffff and 0x80008000. This is exhaustive for these finite selected-value
sweeps, not all source/accumulator/flags combinations or the whole ARM ISA.

The original V3DSP001 debug container supplies ARM11 entry points in shared
FCRAM and an ARM9 completion stub. It is not firmware boot or a native 3DS image.
Context1 waits on an explicit RAM flag until context0's output completes, so
blocks are deterministic. Executed instructions write register words to the
3dsdebug hex port: result, observed CPSR, unchanged source, requested instruction,
input CPSR, unchanged accumulator seed and preserved guard. Thumb returns through
BX to ARM before MRS, so captured T is normalized; the component gate checks T.

## Independent file grading and controls

```sh
ctest --test-dir build -R '^(arm_extend_test|ctr_extend_registers|ctr_extend_checkpoint_test)$' --output-on-failure
```

The host oracle independently defines groups/encodings and uses byte slicing and
unbounded arithmetic. It imports only the ROM constructor. It checks exact file
identity/CRC, rows, block domains, actual completion and trailer. No guest verdict
or framebuffer is scored. Emulator/core/decoder/assembler/oracle/recipe/archive
hashes must remain fixed. All seven fields have one-bit faults; seven malformed
streams reject. The **1,664-case** short image executes wrong-result and
wrong-flags controls (all cases reject) and wrong-source control (**1,344
mismatches**). The archived full cartridge must reproduce from original source.

The component gate passes **10,081,152 observations**, including selected full
sixteen-bit domains, all defined register fields and aliases, all ordinary
conditions, all NZCVQ/GE combinations on boundary patterns, all Thumb allocations,
and ARM7/ARM9/reserved/PC diagnostics without architectural mutation. The prior
core fails the first extension result/flag gate.

The checkpoint gate captures actual continuation text at frame boundaries 0/1/3
in the first sweep, compares two-frame ordinary/fast execution, and compares all
**135,791,096 bytes** of native state. It proves those prefixes; it does not
claim checkpoint coverage of every instruction group. Existing DS/DSi/3DS
machine/decoder/network gates check shared-code regressions.

Physical instruction timing, ARMv6 MMU/SCTLR/unaligned modes, complete exceptions,
remaining DSP/media/system instructions, firmware/retail boot, graphics/audio,
analog/touch/wireless peripherals and full platform compatibility remain pending.
3DS stays **30%** of the fixed delivery checklist, not hardware accuracy.
