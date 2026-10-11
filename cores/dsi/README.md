# Experimental DSi core plugin

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo DSi | **30%** | 6/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo DSi delivered milestones: Native digital controller/keypad registers consume frontend input, Actual production-plugin TCP peers pass original guest oracle and whole-state comparison, Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Enhanced mapping/control, firmware/retail boot, graphics/audio and wireless multiplayer remain.
<!-- completion:end -->

`dsi_plugin` builds `dsi.so` (or the platform library equivalent), ABI v2,
version 0.1.0. It is a distinct loadable plugin for repository-owned `.vdsi`
programs. DS and DSi share the ARM/Thumb interpreter and bus implementation;
DS retains its own `.nds` loader, 4 MiB RAM and state identity. DSi now has
working dual-CPU integer execution and expanded-memory behavior. **Retail DSi
software and firmware do not boot. Overall platform completion is unknown.**

## Implemented profile

- Existing [ARM9/ARM7 integer subset](../nds/README.md), including all sixteen
  ARMv5TE signed DSP multiply forms and LDRD/STRD in the supplied U=0
  alignment profile, plus ordinary ARM LDM/STM across all four address modes,
  ARM9 BLX immediate, SWP/SWPB, CLZ and QADD/QSUB/QDADD/QDSUB
  from the shared decoder (original corpus graded on DS),
  separate CPU state,
  deterministic scheduling and synthetic `dsdebug` register transport.
- 16 MiB main RAM, 32 KiB legacy shared WRAM and 64 KiB ARM7 private WRAM.
- SCFG_EXT memory-size bits 14–15 at `04004008`, propagated from ARM9 to ARM7.
  Modes 0/1 mirror the first 4 MiB across `02000000–02ffffff`; `0c`/`0d`
  regions read zero and ignore stores. Mode 2 exposes all 16 MiB and mirrors
  it at `0c000000–0cffffff` and `0d000000–0dffffff` for both CPUs.
- Mode 3 retains the first chip and `0c` mirror; the `0d` open-bus/debug-chip
  region diagnoses as unsupported. Physical open-bus values are not scored.
- Byte/halfword/word control lanes, ARM7 read-only RAM-limit bits, and bit 31
  access locks. Each CPU's lock persists until reset; locked control reads
  return zero and writes are ignored in this profile. RAM mode is retained.
  Other SCFG feature bits reject before mutation because their circuits are
  absent. Initial SCFG values `80008000` enable only this supplied subset;
  they are not measured firmware reset values.
- Digital DS-style keypad input and a distinct DSi controller layout identity.
  Touch, lid transitions, microphone and cameras are pending.
- Atomic ROM/state validation, full RAM/CPU/control/input snapshots, reset,
  exact replay and `INetplayCapable` fast-state/hash support.

Instruction counters and the existing 2:1 CPU attempt scheduling ratio are
supplied conventions. DSi CPU frequency metadata is zero (unspecified).
No physical DSi clock, bus, cache or instruction timing has been verified.

## Original images and states

`.vdsi` is a repository test convention using a 512-byte header with unit code
2 at `12`, marker `VDSI0001` at `180`, and the same original segment descriptor
positions as the DS harness (`20` for ARM9, `30` for ARM7). Each descriptor is
four little-endian words: file offset, entry, RAM address and byte count.
Segments must be aligned, nonempty, in bounds, disjoint in file and memory,
and contain their entry. ARM9 loads anywhere within the 16 MiB main RAM;
ARM7 can load there or in its 64 KiB private WRAM. Images are limited to
16 MiB. This is not the native TWL loader or firmware boot procedure.

States use `VDSI0001` as their separate state magic, the existing 352-byte
CPU/identity prefix, all 16 MiB main RAM and legacy WRAM, then eight-byte
keypad, SCFG9 and SCFG7 scalars. Total size is **16,875,896 bytes**. ROM and
state use separate containers despite sharing that version marker. Invalid
scalars, inconsistent RAM-limit fields, truncation and cross-ROM/profile
states reject before mutation. DS legacy states are not accepted by DSi.
Both plugins can coexist without metadata/static symbol collisions.

## Verified evidence

The [original RAM corpus](../../tests/roms-src/dsi/ram_registers/README.md)
matches **387,072/387,072 fourteen-word observations**: three RAM-limit modes,
four 4 MiB banks, three store aliases, three read aliases, either writing CPU,
all aligned byte/halfword/word lanes and all 256 byte/complement data patterns.
An independent host byte oracle grades both CPUs' four bank words and every
captured stimulus field. These are settled mapping checks, not timing proof.

The local component gate matches **67,109,008 bus observations**. Every byte
of the supplied 16 MiB RAM is written once with an address-derived pattern and
read through all three aliases by the other CPU; boundary widths, configuration
lanes/modes/locks, atomic rejection, high-RAM code, DS/DSi image/state/plugin
isolation and exact replay are also checked. This covers one chosen value at
each byte address, not every address/value combination. The profile treats
all these bytes as RAM; physical chip-control locations remain unmodeled.
The component gate passes undefined-behavior sanitization. Existing DS CPU,
bus, state and plugin checks still pass.

The [original netplay probe](../../tests/roms-src/dsi/netplay_probe/README.md)
passes the production TCP rollback test: two independent peers match a
100-frame solo baseline byte-for-byte and eight guest register words match an
independent oracle. Host owns the one keypad; the joining peer is a viewer.
This does not implement DSi wireless link multiplayer. ROM mismatch, malformed
frames, prior-history reset and core-replacement disconnect checks pass.
Physical controller hardware and Internet/relay/Windows sessions remain
unverified; existing frontend controller identity/hotplug support is shared.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce nds_plugin dsi_plugin nds_machine_test dsi_machine_test netplay_plugin_peer -j4
ctest --test-dir build -R '^(dsi_|nds_machine_test|nds_netplay_plugin_test)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Local TCP tests require loopback socket permission. CI/CD remains disabled.

## Pending hardware

Firmware/boot ROM startup, native DSi-enhanced cartridge/DSiWare loading,
remaining CPU instructions and privileged/banked/interrupt state, TCM/caches,
physical timing and exceptions, new WRAM/MBK allocation, DMA/NDMA, full SCFG,
chip memory control, graphics/audio/DSP, touch/microphone/cameras, SD/NAND,
wireless, accessories and real hardware traces remain pending. No full
hardware percentage is claimed. Nintendo 3DS and Wii U now have separate partial integer plugins; Switch remains
missing; see [coverage and remaining work](../../docs/HARDWARE-COVERAGE.md).

RAM-limit/mirror and SCFG definitions follow [GBATEK's DSi control register
section](https://problemkaputt.de/gbatek.htm#dsicontrolregistersscfg), accessed
through its [Markdown mirror](https://github.com/mgba-emu/gbatek/blob/gh-pages/ds.md#dsicontrolregistersscfg).
The implementation's initial values, unsupported-circuit handling, diagnostic
open-bus behavior and scheduling are supplied profile choices.

## Shared ARMv6 decoder isolation

The shared interpreter now implements ARMv6 extension/accumulate instructions
for 3DS ARM11. DS/DSi ARM9 and ARM7 remain earlier CPU profiles and reject these
forms diagnostically before data access; no ARMv6 feature is enabled here.
The shared extension component gate checks profile isolation. Physical undefined
instruction exceptions and complete CPU behavior remain pending.

The shared decoder also implements ARMv6 scalar/dual-halfword saturation for
3DS ARM11. DS/DSi CPU profiles reject these encodings; their ARMv4/v5 instruction
support and platform completion percentages are unchanged.
