# Original ARM11 packing and absolute-difference register corpus

Original MIT cartridge, independent host oracle and assembler. No SDK,
firmware, retail assets or external emulator code. Reuses only the original
assembler in `../reversal_registers/build.py`. Build with `python3 build.py`.

## Behavior

[ARM Architecture Reference Manual DDI0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a),
A4.1.43–44 and A4.1.136–137, defines the encodings and behavior. PKHBT retains
Rn's low halfword and takes the high halfword of Rm after LSL 0–31. PKHTB
retains Rn's high halfword and takes the low halfword after ASR 1–32. Encoded
ASR zero means 32; the assembler's omitted-shift spelling is a different alias.
USAD8 sums four unsigned byte absolute differences. USADA8 adds the accumulator
modulo 2^32. Accumulator field 15 selects USAD8 and does not read PC. All four
forms preserve flags, including NZCVQ and GE, and read sources before an aliased
destination write. Earlier CPU profiles and unpredictable PC sources/destinations
stop diagnostically. Physical timing and precise exceptions are not supplied.

## Executed guest domain

Both ARM11 contexts produce **18,874,368 snapshots / 150,994,944 actual register
words**, 144 groups per context, each with 65,536 selected values:

- **128 packing groups**: PKHBT/PKHTB × 32 encoded counts × two NZCVQ/GE
  surrounds (all clear/all set). Right sources contain a complete low halfword
  and its complemented high halfword. Left seeds are 0x12345678 and 0x89abcdef,
  correlated with the flag surround.
- **16 difference groups**: USAD8/USADA8 × four selected byte positions × two
  flag surrounds. Every 256² selected byte pair is exercised. Other bytes stay
  at guards: left 0x78563412, right 0x000180ff. The accumulator register holds
  0x7fffffff or 0xffffffff; non-accumulating USAD8 ignores it. The two seeds
  exercise sign-boundary crossing and unsigned wrap while flags remain unchanged.

These are complete finite selected-value sweeps, not all independent 32-bit
source pairs or accumulator/flag combinations. The component checks wider
packing surrounds, all possible difference sums and a selected near-wrap interval.

Each row contains **eight actual register words**: result, CPSR, unchanged left,
unchanged right, requested instruction, input CPSR, unchanged accumulator and
guard 0x89abcdef. The guest writes to the 3dsdebug hex port; no framebuffer or
guest pass verdict is scored. The original V3DSP001 container starts ARM11_0/1
at 0x20000000/0x20040000 and an ARM9 completion stub at 0x08000000. Context1
waits on RAM 0x2003f000 for deterministic block ordering. This is an original
debug execution profile, not firmware or native retail boot.

## Independent grading and controls

```sh
ctest --test-dir build -R '^(arm_pack_difference_test|ctr_pack_difference_registers|ctr_pack_difference_checkpoint_test)$' --output-on-failure
```

The host oracle independently defines encodings/groups, packs with unbounded
arithmetic and signed floor division, and computes byte differences from byte
arrays. It imports only the ROM constructor. It checks file identity/CRC, exact
row width/count, both block completions and strict trailer. Emulator, core,
decoder, oracle, builders, recipe and archive hashes must remain fixed.

The **2,304-case** short image executes eight selected values per group in both
contexts. Wrong-result and wrong-Q reject every case. Executed omitted accumulator rejects all **128 accumulating cases**, swapped packing form rejects
**2,048**, and wrong source rejects **1,984**. All eight field faults and seven
malformed/incomplete streams reject. The full original archive reproduces:
**9,764 bytes**, SHA256
`dd0481b74218b8b881735837f3700faf03daab75e7555959096fea95f471f75c`.

The component gate checks **45,984,747 observations**: all 64 packing controls
with complete selected halfwords at low/high positions and zero/one surrounds;
all selected byte pairs at every lane for both difference forms; all sums 0–1020
at every seed in 0xfffff000–0xffffffff and two flag surrounds, plus seven boundary
seeds; every ordinary condition/NZCVQ/GE combination on boundary sources; all
15³ packing register fields at every control, all valid difference fields with
all 16 accumulator fields, and ARM7/ARM9/PC diagnostics. Alias checks use the
actual pre-instruction operands. The prior decoder fails the packing result gate.

Three native checkpoint prefixes at frames 0/1/3 compare **6,117 actual register
rows** and complete **135,791,096-byte** states after two ordinary frames versus
two fast netplay frames after restore. These are first packing group prefixes,
not checkpoint coverage of every packing/difference group. Shared DS/DSi/3DS
machine/network and older ARM decoder/component gates protect regressions.
The native state format and emulator ABI do not change.

Remaining CPU/media/system instructions, MMU/SCTLR/unaligned/exception modes,
privilege/banked registers, physical timing, firmware/native retail boot,
graphics/audio and analog/touch/wireless devices remain unfinished. 3DS remains
**30%** of the fixed delivery checklist, not hardware accuracy. This increment
has no fresh private Mario 64 capture; CI/CD remains disabled.

Verification for 0.6.0: full Release build and all **19 selected gates pass**;
all **143 original archives** reproduce. The full guest capture has zero
mismatches and stable source/binary hashes. Three native checkpoint prefixes
compare **6,117 actual register rows** and complete states. Completion renderer
and whitespace checks pass. CI/CD remains disabled.
