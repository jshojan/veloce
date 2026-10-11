# Experimental Nintendo 3DS core plugin

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo 3DS | **30%** | 6/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo 3DS delivered milestones: Native digital controller/keypad registers consume frontend input, Actual production-plugin TCP peers pass original guest oracle and whole-state comparison, Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: ARM11 ARMv6 dual-halfword multiply/exchange/accumulate, UMAAL/high-word multiply, packing/absolute differences, parallel arithmetic/SEL, extension/accumulate and saturation now have original independently graded register images, flags/alias/profile controls and native checkpoint continuations. The new dual multiply image checks 12,582,912 snapshots / 125,829,120 register words. Remaining CPU/MMU, firmware/native formats, graphics/audio, analog/touch and wireless multiplayer remain.
<!-- completion:end -->

`ctr_plugin` builds `3ds.so` (or the platform library equivalent), ABI v2,
version 0.8.0. The separately loadable core executes repository-owned `.v3ds`
integer programs on two ARM11 contexts and one ARM9 context. **Native 3DS
software and firmware do not boot. Complete hardware behavior remains unverified.**

## Implemented profile

- ARM11-only ARMv6 subsets: ARM/Thumb reversal, signed/unsigned extension and
  accumulation, scalar/halfword saturation, parallel arithmetic/GE/SEL, halfword
  packing, byte absolute differences, high-word/unsigned wide multiplication,
  and signed dual-halfword multiply/exchange/32-bit or 64-bit accumulation.
  Original register-output sources and finite domains are linked in the
  [front-page coverage table](../../README.md#3ds-arm11-instruction-coverage).
- The [DS integer interpreter](../nds/README.md), with ARMv6 ARM/Thumb REV,
  REV16 and REVSH enabled only for ARM11. Unsupported instructions diagnose
  and stop that context. The shared decoder also supports CLZ and QADD/QSUB/QDADD/QDSUB
  on ARM9/ARM11, plus ARM-state BLX immediate and SWP/SWPB in the supplied legacy alignment model;
  all sixteen ARMv5TE signed DSP multiply forms and LDRD/STRD in the
  supplied little-endian U=0 alignment profile are also supported, along with
  ordinary ARM LDM/STM in all four address modes. User-bank/SPSR forms remain
  unimplemented.
  CLZ/saturation/DSP corpora are graded on DS ARM9, and exchanges on both DS CPUs.
  DSP/doubleword component gates separately check ARM11 aliases and flags.
  Doubleword alignment/endianness configuration and physical aborts are pending.
  Remaining ARMv6 media/system instructions and platform-specific timing are pending.
- Shared 128 MiB FCRAM at `20000000–27ffffff`, shared 512 KiB AXI RAM at
  `1ff80000–1fffffff`, and 1 MiB ARM9 private RAM at `08000000–080fffff`.
  Bus byte/halfword/word access is little endian; unmapped reads/writes reject,
  with writes validated before mutation. ARM11 cannot access ARM9 private RAM.
- Read-only 16-bit HID_PAD at `10146000`, twelve active-low digital controls,
  byte lanes and a distinct 3DS frontend controller layout. All 4,096 input
  combinations pass component checks. Circle Pad, touch and HID IRQ are pending.
- Atomic image/state validation, reset, full RAM/CPU/input snapshots and exact
  replay. The debugger's 16-bit memory API views the first 64 KiB of FCRAM.
- Synthetic `3dsdebug` output: byte text at `1fe00000`, word hex at `1fe00008`,
  per-context ungraded completion at `1fe00004`. These are test conventions.

Each supplied frame permits 100,000 instruction attempts per active context,
interleaved 1:1:1. Metadata supplies 60 fps and an unspecified CPU frequency.
Instruction counts and this scheduler do not prove physical timing. The blank
400×480 framebuffer and empty audio buffer are placeholders. Existing legacy
aligned/rotated ARM word-memory behavior remains; ARMv6 SCTLR/MMU and unaligned
access modes are not implemented.

## Images and states

The original image format has a 64-byte header: `V3DSP001` at byte 0, zero
reserved bytes 8–15, then three 16-byte descriptors at 16/32/48 for ARM11_0,
ARM11_1 and ARM9. Each has four little-endian words: file offset, RAM address,
entry, byte count. Segments are aligned, nonempty, in bounds, disjoint in both
file and physical memory, and contain their entry. ARM11 loads FCRAM or AXI;
ARM9 can also load private RAM. Maximum image size is 16 MiB. This container
is not a native 3DS/3DSX/NCCH format or retail boot procedure.

States use `V3DS0001`, a 504-byte identity/CRC/frame/completion/input/CPU prefix
and all three RAM arrays, totaling **135,791,096 bytes**. Scalar widths, flag
masks, PC alignment, completion bits, ROM identity and exact length validate
before mutation. CPU model is fixed by the plugin. States are not compatible
with DS or DSi states.

## Verified evidence

The [original reversal ROM](../../tests/roms-src/3ds/reversal_registers/README.md)
passes **25,165,824/25,165,824 seven-word captures**: both ARM11 contexts,
ARM/Thumb REV/REV16/REVSH, every NZCVQ combination, and every 16-bit low value
with its complemented high half. This is a finite input domain, not all 32-bit
values. All seven captured fields are independently graded from the result
file; no screen or guest verdict is scored. Thumb CPSR is captured after BX
back to ARM, with T normalized to zero. Direct component checks verify T.

`ctr_machine_test` matches **36,123,648 reversal register snapshots** including
all defined register fields/aliases, ARM conditions, selected full-width
patterns, walking/complemented bits, GE and flag preservation. Unsupported
ARM9/DS/DSi forms and reserved/R15 forms reject without register mutation.
The same gate matches **541,065,300 RAM bus observations**, writing every byte
once with an address-derived pattern and reading through all permitted CPUs;
selected widths, boundaries, ownership and atomic rejection are checked.
This is one chosen value per address, not all address/value combinations.
Loader/state rejection, a three-context RAM-writing guest, dynamic plugin
lifecycle, digital input and exact replay pass. The full component gate also
passes undefined-behavior sanitization. Existing DS/DSi component regressions
pass. All 54 original ROMs reproduce against the source/image ledger. See [testing](../../TESTING.md).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce ctr_plugin ctr_machine_test -j4
ctest --test-dir build -R '^ctr_' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Full raw register output is about 1.48 GiB and stays in
`build/artifacts/owned/3ds_reversal/` with JSON grades and control artifacts.
CI/CD remains disabled; verification runs locally.

## Remaining hardware and modern features

The core now exposes `INetplayCapable`: one host-owned keypad, a joining
viewer, direct fast-state save/load and full canonical-state FNV hashing.
Fast serialization writes into preallocated buffers without temporary vectors;
validation completes before restoration mutates CPU/RAM/input. Fast states
retain the existing `V3DS0001` format and all 135,791,096 bytes. Null/short
buffers, guards, full canonical bytes/hash, far-end RAM and exact replay pass
component and undefined-behavior sanitizer checks.

The [original netplay probe](../../tests/roms-src/3ds/netplay_probe/README.md)
passes production IPv4 TCP rollback: both independent peers' 100-frame states
match the solo baseline byte-for-byte and twelve guest words match an
independent arithmetic oracle. The viewer corrects host predictions. Executed
wrong-input, field and invalid-stream controls fail; ROM mismatch, malformed
TCP frames, different prior histories and replacement/frame-change lifecycle
checks pass. This mirrors one 3DS instance; wireless/link multiplayer is pending.

Full snapshots cost about 129.5 MiB each. Window 8 retains nine full states once history fills,
about 1.14 GiB per peer, in addition to core and scratch buffers. Compression,
page sharing and sustained real-time performance are unverified. Use a smaller
rollback window on machines with limited memory. Existing NES/DS/DSi production
netplay regression tests pass. Internet/relay, Windows/cross-platform and
physical two-machine sessions remain unverified.
Frontend controller bindings and hotplug support are shared, but physical
3DS input, analog controls, touch are unverified.

Remaining CPU instructions, privileged/banked/exception state, firmware,
MMU/caches/TCM, SMP coherency, physical timing, DMA, graphics/GPU/VRAM, DSP/audio,
wireless, storage, services, native formats and New 3DS extensions remain
pending. Wii U and Switch now have separate partial integer plugins. No overall hardware
completion percentage is inferred from these finite test domains.

Instruction definitions follow the ARM-published
[ARM Architecture Reference Manual DDI0100I, ARM and Thumb REV sections](https://www.scs.stanford.edu/~zyedidia/docs/arm/armv6.pdf)
and [ARM11 MPCore TRM DDI0360F](https://documentation-service.arm.com/static/5e8e1e0388295d1e18d368b2).
Physical RAM and HID bit assignments follow
[GBATEK's 3DS reference](https://github.com/mgba-emu/gbatek/blob/gh-pages/3ds.md).
The loader, debug transport, reset values and scheduling are supplied conventions.

## ARMv6 extension and accumulation (0.3.0)

Both ARM11 contexts now implement all twelve ARM signed/unsigned byte/halfword/
dual-byte extension forms with optional accumulation, and all four Thumb scalar
extensions. ARM rotations 0/8/16/24, Rn=15 non-accumulating aliases, source/
destination/base aliases, modulo scalar/independent halfword addition and NZCVQ/
GE preservation are supported. ARM7/ARM9, reserved fields and unpredictable PC
operands stop diagnostically. This does not supply physical exception behavior.

The [original cartridge and independent oracle](../../tests/roms-src/3ds/extend_registers/README.md)
grade **13,631,488 snapshots / 95,420,416 actual register words** from both ARM11
contexts. Selected halfword or two independent byte domains are complete at each
rotation, with two flag surrounds and boundary accumulator seeds. The short
image has 1,664 cases; wrong result/flags reject all cases, and an executed wrong
source produces 1,344 rejected snapshots. All seven fields and seven malformed
streams are checked, with fixed binary/source/archive hashes.

The component gate passes **10,081,152 observations** over source domains,
register allocations/aliases, conditions, NZCVQ/GE, Thumb allocations and profile/
reserved/PC diagnostics. The previous core fails the new behavior. A separate
plugin gate compares actual text and complete 135,791,096-byte native states at
three checkpoints in the first sweep, with two-frame ordinary/fast continuations.
State format/ABI remain unchanged. Physical timing, MMU/SCTLR, full exceptions,
remaining media/DSP/system instructions and complete platform behavior remain
pending; 3DS stays **30%**.

Verification for 0.3.0: Release build and all **16 selected gates pass**
(191.61 s), including the full new register-file corpus, shared decoder/machine
regressions and DS/DSi/3DS TCP rollback. The checkpoint gate compares **6,180
actual continuation rows** and complete states at the documented prefixes.
All **140 original ROM archives reproduce**; completion rendering and whitespace
checks pass. This increment supplies no native firmware/retail boot evidence.

## ARM11 saturation register files (2026-10-11)

3DS 0.4.0 adds ARMv6 SSAT/USAT and SSAT16/USAT16, including every encoded
width, LSL/ASR control and sticky Q. NZCV/GE are unchanged; aliases read sources
before writes. Earlier CPU profiles and PC operands stop diagnostically.
The original dual-ARM11 image independently grades **12,582,912 snapshots /
88,080,384 actual register words**. Scalar inputs cover sign-extended bytes at
all controls; dual inputs sweep each halfword with complemented lane pairs.
This is bounded coverage. The component gate checks **64,101,056 observations**,
including wider selected inputs, conditions, all flag states, aliases and CPU
profile restrictions; the prior decoder fails it. Short guest controls reject
all 132,096 wrong-result/wrong-Q cases and 71,184 wrong-source cases. Seven
field faults and seven malformed streams reject. Three native checkpoints
compare actual continuation text and complete 135,791,096-byte states.
Full CPU/media/system support, timing/MMU/exceptions, firmware/native retail,
graphics/audio and analog/touch/wireless remain pending. No delivery milestone
is newly complete; 3DS remains **30%**. See [source and finite domains](../../tests/roms-src/3ds/saturation_registers/README.md).

Verification for 0.4.0: full Release build, **19 distinct selected gates pass**,
and all **141 original archives** reproduce. The first network attempts were
blocked by sandbox socket restrictions; all three pass with local TCP enabled.
The full capture has zero mismatches and fixed source/binary fingerprints.
Completion renderer and whitespace checks pass. This increment changes neither
the native state format nor the emulator ABI and does not add a fresh private
Mario 64 capture.

## ARM11 parallel arithmetic and GE selection (2026-10-11)

3DS 0.5.0 adds 36 ARMv6 wrapping/saturating/halving byte/halfword arithmetic
forms and SEL, plus CPSR_s/fs register/immediate writes for GE. Wrapping signed
GE reflects the unwrapped result's sign; unsigned GE reflects carry/no borrow.
Parallel saturation preserves Q; halving keeps the wide arithmetic bit before
discarding bit zero. SEL consumes actual GE and leaves flags unchanged.
The original dual-ARM11 image independently grades **19,922,944 snapshots /
179,306,496 actual register words**: complete byte pairs at each constrained
lane, full left halfwords at two right boundary words, and all 16 SEL masks.
This is bounded coverage, not every independent lane or halfword operand pair.
The component gate checks **32,198,238 observations**, including aliases,
conditions, flags, reserved encodings, profile isolation and GE writes.
Executed wrong-result/Q/GE/SEL controls reject all 2,432 short cases; wrong-source
rejects 2,312. Nine field faults and seven malformed streams reject. Native
checkpoint prefixes compare actual text and complete 135,791,096-byte states.
Full CPU/media/system/MMU/exception behavior, timing, firmware/retail boot,
graphics/audio and native devices remain pending. State format and ABI are
unchanged; 3DS remains **30%**. See [original source and domains](../../tests/roms-src/3ds/parallel_registers/README.md).

Verification for 0.5.0: full Release build passes; all **142 original archives**
reproduce. All **22 selected gates** passed the first candidate; after the MSR
source diagnostic correction, all **nine affected register/checkpoint/machine/
network gates** pass on the final binary. The repeated full register capture
has zero mismatches and stable source/binary hashes. The native checkpoint gate
compares **5,657 actual register rows** across three continuation prefixes.
The earlier decoder fails the arithmetic gate; the draft without the MSR guard
fails the reserved-source rejection gate. Completion renderer and whitespace
checks pass. CI/CD remains disabled.

## ARM11 packing and absolute differences (2026-10-11)

3DS 0.6.0 adds ARMv6 PKHBT/PKHTB and USAD8/USADA8. Packing supports all encoded
LSL/ASR counts, including encoded ASR #32. Difference sums use unsigned bytes
and wrap accumulator addition modulo 2^32; accumulator field 15 selects USAD8.
All forms preserve NZCVQ/GE and read sources before aliased destination writes.
The original dual-ARM11 image independently grades **18,874,368 snapshots /
150,994,944 actual register words**: complemented halfword packing inputs at
all shifts and selected byte pairs at each lane with guard bytes. These are
finite constrained domains. The component gate checks **45,984,747 observations**,
including all possible difference sums 0–1020 at a complete selected near-wrap
seed interval, all flags/conditions, aliases and CPU-profile/PC diagnostics.
Executed wrong-result/Q controls reject all 2,304 short cases; omitted accumulator,
swapped packing and wrong source reject 128/2,048/1,984 cases. Eight field faults
and seven malformed streams reject. Native checkpoint prefixes compare actual
text and complete 135,791,096-byte states. Full CPU/media/system/MMU/exception
behavior, timing, firmware/retail boot, graphics/audio and native devices remain
pending. ABI/state format are unchanged; 3DS remains **30%**. See
[original source and domains](../../tests/roms-src/3ds/pack_difference_registers/README.md).

Verification for 0.6.0: full Release build and all **19 selected gates pass**;
all **143 original archives** reproduce. The full guest capture has zero
mismatches and stable source/binary hashes. Three native checkpoint prefixes
compare **6,117 actual register rows** and complete states. Completion renderer
and whitespace checks pass. CI/CD remains disabled.

## ARM11 high-word and unsigned wide multiplication (2026-10-11)

3DS 0.7.0 adds ARMv6 UMAAL and SMMUL/SMMLA/SMMLS with their rounded forms.
UMAAL adds both old destination words as independent unsigned 32-bit seeds.
High-word forms multiply signed operands; rounded subtraction negates the
product before adding 0x80000000 and extracting the high word. All preserve
NZCVQ/GE and read sources before aliased writes; invalid profiles/PC operands,
reserved operations and duplicate UMAAL destinations stop diagnostically.
The original dual-ARM11 image independently grades **7,340,032 snapshots /
73,400,320 actual register words**, sweeping selected complemented halfwords,
four right boundary words and two flag/seed surrounds. These are finite domains.
The component gate checks **31,559,680 observations**, including quotient/floor
and independent 16-bit-limb arithmetic, ties, carry, flags/conditions and aliases.
Executed wrong-result/Q reject all 896 short cases; UMLAL replacement, wrong
rounding and wrong source reject 128/416/744 cases. Ten field faults and seven
malformed streams reject. Native checkpoint prefixes compare actual text and
complete 135,791,096-byte states. Full CPU/media/system/MMU/exception behavior,
timing, firmware/retail boot, graphics/audio and native devices remain pending.
ABI/state format are unchanged; 3DS remains **30%**. See
[original source and domains](../../tests/roms-src/3ds/wide_media_multiply_registers/README.md).

Verification: the full Release build and all **21 selected gates** pass, including
the **38,338,560-case** original DS multiply regression. All **144 original
archives** reproduce. The new native corpus has zero mismatches and unchanged
capture provenance hashes; completion rendering and whitespace checks pass.

## ARM11 dual-halfword multiplication (2026-10-11)

3DS 0.8.0 adds SMUAD/SMUSD, SMLAD/SMLSD and SMLALD/SMLSLD, each with
second-operand exchange. Signed halfword products are summed or subtracted;
32-bit final overflow sets sticky Q, while long accumulation preserves flags.
Sources and accumulators are read before aliases; earlier CPU profiles, PC,
reserved operations and duplicate long destinations stop diagnostically.
The original dual-ARM11 image independently grades **12,582,912 snapshots /
125,829,120 actual register words** across complete selected low halfwords,
a fixed high halfword, four right words and two correlated flag/seed surrounds.
The component gate passes **55,320,578 observations**, including independent
carry arithmetic, overflow cancellation, conditions, flags, aliases and invalid
encodings. Executed 1,536-row result/Q/subtract/source controls reject all;
removing accumulation rejects 1,024 rows and wrong exchange rejects 720.
Ten field faults and seven malformed streams reject. Native checkpoint prefixes
compare actual output and complete 135,791,096-byte states. This is finite
instruction coverage: full CPU/system/MMU/exception/privilege behavior, timing,
firmware/retail boot, graphics/audio and devices remain pending. ABI/state formats
are unchanged; 3DS remains **30%**. See
[original source and domains](../../tests/roms-src/3ds/dual_media_multiply_registers/README.md).

Verification: full Release build and all **22 selected gates** pass, including
the adjacent **7,340,032-case** high-word/UMAAL image. The new **12,582,912-case**
image has zero mismatches with unchanged capture provenance. Checkpoints compare
**5,450 actual rows** and complete states. All **145 original archives** reproduce;
completion rendering and whitespace checks pass. CI/CD remains disabled.
