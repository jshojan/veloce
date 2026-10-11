# Experimental Nintendo 3DS core plugin

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo 3DS | **30%** | 6/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo 3DS delivered milestones: Native digital controller/keypad registers consume frontend input, Actual production-plugin TCP peers pass original guest oracle and whole-state comparison, Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Remaining CPU/MMU, firmware/native formats, graphics/audio, analog/touch and wireless multiplayer remain.
<!-- completion:end -->

`ctr_plugin` builds `3ds.so` (or the platform library equivalent), ABI v2,
version 0.2.0. The separately loadable core executes repository-owned `.v3ds`
integer programs on two ARM11 contexts and one ARM9 context. **Native 3DS
software and firmware do not boot. Full platform completion is unmeasured.**

## Implemented profile

- The [DS integer interpreter](../nds/README.md), with ARMv6 ARM/Thumb REV,
  REV16 and REVSH enabled only for ARM11. Unsupported instructions diagnose
  and stop that context. The shared decoder also supports CLZ and QADD/QSUB/QDADD/QDSUB
  on ARM9/ARM11, plus ARM-state BLX immediate and SWP/SWPB in the supplied legacy alignment model;
  all sixteen ARMv5TE signed DSP multiply forms are also supported.
  CLZ/saturation/DSP corpora are graded on DS ARM9, and exchanges on both DS CPUs.
  The DSP component gate independently checks ARM11 register aliases and flags.
  Remaining ARMv6 DSP extensions and platform-specific timing are pending.
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
pending. Wii U now has a separate partial integer plugin; Switch still lacks a core. No overall hardware
completion percentage is inferred from these finite test domains.

Instruction definitions follow the ARM-published
[ARM Architecture Reference Manual DDI0100I, ARM and Thumb REV sections](https://www.scs.stanford.edu/~zyedidia/docs/arm/armv6.pdf)
and [ARM11 MPCore TRM DDI0360F](https://documentation-service.arm.com/static/5e8e1e0388295d1e18d368b2).
Physical RAM and HID bit assignments follow
[GBATEK's 3DS reference](https://github.com/mgba-emu/gbatek/blob/gh-pages/3ds.md).
The loader, debug transport, reset values and scheduling are supplied conventions.
