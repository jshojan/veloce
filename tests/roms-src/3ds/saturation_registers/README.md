# Original ARM11 saturation register corpus

Original MIT assembler, cartridge and oracle. No SDK, firmware, retail assets or
external emulator code. Only the original assembler from
`../reversal_registers/build.py` is reused. Build with `python3 build.py`.

## Behavior and bounded coverage

[ARM Architecture Reference Manual DDI 0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a),
A4.1.91–92 and A4.1.138–139 specify SSAT, SSAT16, USAT and USAT16. Scalar
signed widths are 1–32, unsigned widths 0–31. LSL uses encoded counts 0–31;
ASR uses counts 1–32, with encoded zero meaning 32. The shifted operand wraps
to 32 bits before its signed value is clamped. Dual forms clamp two signed
halfword inputs independently to signed widths 1–16 or unsigned widths 0–15.
Q becomes set if either lane clamps; an already set Q stays set. NZCV and GE
are preserved. This does not implement physical instruction timing or exceptions.

Both ARM11 contexts execute **12,582,912 snapshots / 88,080,384 actual register
words**. Each has 8,192 scalar groups: both families, every encoded width,
every LSL/ASR control and two NZCVQ surrounds (all clear/all set), each sweeping
all 256 sign-extended byte inputs. Another 64 dual groups cover both families,
every width and both flag surrounds, each sweeping all 65,536 low halfwords
with the complemented high halfword. This covers every value of each lane,
not every lane pair or every 32-bit scalar input. No Thumb saturation is claimed.

The original V3DSP001 container starts ARM11_0 at 0x20000000 and ARM11_1 at
0x20040000, with an ARM9 completion stub. Context1 waits on RAM 0x2003f000 until
context0 completes its output. This is an original debug profile, not native
firmware boot. The guest writes seven register words per case to the 3dsdebug
port: result, observed CPSR, unchanged source, requested encoding, input CPSR,
unchanged seed 0x12345678 and guard 0x89abcdef. Guest completion is not a verdict.

## Independent file grading

```sh
ctest --test-dir build -R '^(arm_v6_saturation_test|ctr_saturation_registers|ctr_saturation_checkpoint_test)$' --output-on-failure
```

The host oracle independently defines groups and encodings, uses signed byte
conversion, mathematical floor division, unbounded shifts and min/max clamping,
and imports only the ROM constructor. It verifies exact identity/CRC, seven-word
rows, declared domains, both block completions and strict trailer. No framebuffer
is graded. Emulator, core, decoder, generator, oracle, shared assembler, recipe
and archive hashes must stay fixed throughout capture and grading.

The **132,096-case** short cartridge executes eight selected values in every
group on both contexts. Wrong-result and wrong-Q controls reject all cases;
wrong-source rejects **71,184** cases. Seven separate field faults and seven
malformed/incomplete streams reject. The full archive reproduces from source: 463,652 bytes, SHA256
`90f2306ea28a82005987822ad0bd7b09d5528ec0e2251e2377eac68844941730`.

The component gate checks **64,101,056 observations**: all scalar controls with
selected full byte sweeps at four positions and two surrounds, full halfword
sweeps for dual forms and selected scalar boundaries, all ordinary conditions
and NZCVQ/GE states on boundary inputs, register fields/aliases and earlier CPU/
PC diagnostics. The prior decoder fails the result/Q gate. Diagnostic stops do
not claim hardware exception behavior.

The checkpoint gate checks actual register text and all **135,791,096 bytes** of
native state at boundaries 0/1/3, comparing two ordinary frames with two fast
netplay frames after restore. These are prefixes of the first scalar groups;
checkpoint coverage of every group is not claimed. DS/DSi/3DS decoder, machine
and network gates protect shared-code behavior.

Remaining media/system instructions, MMU/SCTLR/unaligned modes, complete
exceptions, physical timing, firmware/retail boot, graphics/audio and native
analog/touch/wireless devices remain pending. 3DS remains **30%** of the fixed
delivery checklist, not 30% hardware accuracy.

Verification for 0.4.0: full Release build, **19 distinct selected gates pass**,
and all **141 original archives** reproduce. The first network attempts were
blocked by sandbox socket restrictions; all three pass with local TCP enabled.
The full capture has zero mismatches and fixed source/binary fingerprints.
Completion renderer and whitespace checks pass. This increment changes neither
the native state format nor the emulator ABI and does not add a fresh private
Mario 64 capture.
