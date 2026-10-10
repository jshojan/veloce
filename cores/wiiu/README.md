# Experimental Wii U core plugin

`wiiu_plugin` builds `wiiu.so` (or the platform library equivalent), ABI v2,
version 0.2.0. It executes repository-owned `.vwiiu` PowerPC integer programs
on three independent Espresso-style contexts and shared RAM. **Native Wii U
software and firmware do not boot. Full platform completion is unmeasured.**

## Implemented profile

- Shared [PowerPC integer interpreter](../ppc/README.md): wrapping arithmetic,
  carry/overflow and condition-register behavior, integer multiply/divide,
  logical/rotate/shift/unary operations, branches and byte/halfword/word
  displacement/indexed/update/reverse memory transfers. CPU states include
  all 32 GPRs, PC, CR, XER, LR, CTR, attempt counts and diagnostic stop state.
- Three independent CPU states share the same instruction implementation.
  GameCube/Wii retain their existing loader, memory maps and state formats.
  `ppc::execute(State&, Memory&)` accepts the bus; it owns instruction behavior,
  while each console owns memory, scheduling, output and completion.
- Physical-profile 32 MiB MEM1 at `00000000–01ffffff` and 2 GiB MEM2 at
  `10000000–8fffffff`, visible to all three contexts. The bus is big endian;
  direct byte/halfword/word accesses may cross allocated-page boundaries.
  Instructions retain their alignment restrictions. Unmapped accesses reject
  before changing a read destination or writing any byte.
- RAM is backed by lazily allocated 4 KiB pages. All mapped addresses work;
  untouched bytes read zero. Reset discards pages and reloads guest segments.
  These are supplied reset/mapping conventions, not firmware or chip timing.
- Distinct twelve-button frontend controller layout and serialized virtual
  digital-input latch. The latch is available to original programs only at
  synthetic `ffe0000c` (word, read-only). **Native GamePad/VPAD/KPAD, analog
  controls, touch, motion and wireless controller hardware are pending.**
- Synthetic `wiiudebug` transport: byte text at `ffe00000`, word hex at
  `ffe00008`, and per-context ungraded completion at `ffe00004`. These ports
  are emulator test conventions, not Wii U registers.
- Atomic original-image/state validation, reset, sparse/dense RAM snapshots,
  exact replay and plugin lifecycle. The debugger's 16-bit memory interface
  views the first 64 KiB of MEM1.

The supplied scheduler gives each active context one attempt per quantum,
100,000 quanta per frame, interleaved 1:1:1. Metadata supplies 60 fps and zero
CPU frequency (unspecified). It models no measured hardware clock, cache,
coherency or instruction/bus timing. The 640×480 framebuffer is blank and the
audio buffer empty; graphics/audio are not implemented.

## Original images and states

The 64-byte original header has magic `VWUP0001`, zero reserved bytes 8–15,
and three 16-byte descriptors at 16/32/48. Each descriptor has big-endian
file offset, RAM address, entry and byte count. Segments must be aligned,
nonempty, within one mapped RAM region, disjoint in file and physical memory,
and contain their entry. Maximum image size is 64 MiB. This is a repository
test container; RPX/RPL, disc images and native boot/loaders remain pending.

Canonical states use `VWUS0001`. The 992-byte prefix contains ROM identity/CRC,
frame, three completion bits, virtual input, all three CPU states and page
count. Each remaining record is a big-endian four-byte global page index plus
4,096 bytes. MEM1 occupies indices 0–8,191; MEM2 follows at 8,192–532,479.
Indices are strictly increasing, and all-zero pages are omitted. Missing pages
mean zero RAM. Thus every RAM byte is represented without allocating untouched
regions. A fully nonzero-page image is **2,183,168,992 bytes**; a small original
program needs only its used pages. Use a 64-bit host for dense states.

Restore validates identity, exact length, scalar widths, PC/XER masks,
completion/input, page count, index ordering/range and nonzero payloads before
committing replacement CPU/RAM/input state. Cross-ROM, truncated, malformed and
noncanonical states reject. DS/DSi/3DS/GC/Wii states are not compatible.

## Register-file evidence

The full original corpus matches **25,165,824/25,165,824 captures**, with zero
mismatches. Executed result/width/CR controls fail exactly 3,072/1,536/2,304
reduced rows; nine field mutations and seven invalid streams are rejected.
The component gate matches **28,311,552 register snapshots** and
**8,724,152,428 RAM bus observations**, covering the original fixed-field
domain plus all RT/RA fields/aliases on eight source/eight immediate patterns,
all supplied RAM bytes and selected boundary/endian/page widths. Full dense
2,183,168,992-byte state round trip and sparse replay pass, as do atomic
loaders/states, all 4,096 original digital latch values, a three-context guest
at MEM1/MEM2 endpoints and the actual plugin lifecycle. All 52 original ROMs
reproduce against their source/image ledger. The full component gate passes
undefined-behavior sanitization; the GC/Wii component regression passes.

The [original immediate ROM](../../tests/roms-src/wiiu/immediate_registers/README.md)
executes ADDI/ADDIS on all three contexts, capturing nine guest register words.
Its finite domain is every 16-bit immediate, four source patterns, RA=0 and
RA=3, and all eight incoming XER.SO/OV/CA combinations with CR seeds. The host
oracle independently checks every result, preserved CR/XER, source, encoding,
seed, sentinel and context tag from the file. Register aliases and further
sources have separate component evidence. This does not exhaust all 32-bit
operands, every instruction or complete platform hardware.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce wiiu_plugin wiiu_machine_test ppc_machine_test -j4
ctest --test-dir build -R '^(wiiu_|ppc_machine_test)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Raw files and JSON reports remain in `build/artifacts/owned/wiiu_immediates/`.
The original stream is about 1.85 GiB. The component gate writes every RAM byte
once with an address-derived pattern and reads it through all three contexts;
this checks one chosen value per address, not all address/value combinations.
The dense state gate needs several GiB of host RAM. CI/CD remains disabled.

## Netplay and remaining hardware

The core now exposes `INetplayCapable` with one host-owned virtual digital
input latch and a joining viewer. `save_state_fast(nullptr, 0)` queries the
current canonical sparse size; direct writes need only that capacity. The
stable maximum remains 2,183,168,992 bytes, preserving every mapped RAM byte.
Fast saves and full canonical-state hashing avoid temporary state vectors;
loads retain atomic validation. Existing `VWUS0001` states remain compatible:
a 100-frame state matches the previous serializer byte-for-byte.

The [original netplay probe](../../tests/roms-src/wiiu/netplay_probe/README.md)
passes production IPv4 TCP rollback. Both independent peers match the solo
100-frame state byte-for-byte and twelve guest words match the independent
arithmetic oracle. The viewer corrects host predictions. The guest makes a
high MEM2 page nonzero/zero with input, exercising sparse state growth and
shrinkage. Executed input/field/stream controls and ROM/transport/lifecycle
rejections pass. This mirrors one instance; native GamePad/VPAD/KPAD,
wireless multiplayer and native software remain pending.

Rollback history now grows each slot to the current serialized requirement
and reuses its capacity, bounded by the core's fixed maximum. The frontend
also queries current size. This probe needs at most 21,492 bytes per snapshot,
with a 17,392-byte complete final state. Cleared/untouched pages represent
zero RAM, rather than a smaller memory map. High-water capacity remains in
each history slot; dense workloads can still need several GiB per snapshot.
Page sharing, delta states, sustained real-time performance, Internet,
physical two-machine and cross-platform sessions remain unverified.
All seven netplay gates pass across NES/DS/DSi/3DS/Wii U, including the raw
transport and variable-state rollback tests. Full Wii U component checks
(including dense fast bytes/hash, guards and replay) pass undefined-behavior
sanitization. Shared frontend input
bindings/hotplug are available, but native GamePad/Pro Controller/Wii Remote
protocols, ZL/ZR/analog/touch/motion and physical device behavior are unverified.

Remaining CPU instructions, floating point/paired singles, privileged/MSR,
banked/exception/interrupt state, cache/MMU/BAT/RAMPID/coherency, physical timing,
MEM0, Starbuck/IOSU/Cafe services, Latte/GX2 GPU, audio/DSP, storage, native
formats/firmware, wireless and accessories remain pending. Nintendo Switch
still lacks a core plugin. No full hardware completion percentage is claimed.

Base instruction definitions follow [IBM ADDI](https://www.ibm.com/docs/en/ssw_aix_71/assembler/idalangref_cal_inst.html),
[IBM ADDIS](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-addis-cau-add-immediate-shifted-instruction)
and the [NXP MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf).
Physical RAM capacities/ranges and the three-context model follow the original
research recorded in [WiiUBrew memory map](https://wiiubrew.org/wiki/Memory_Map)
and [hardware notes](https://wiiubrew.org/wiki/Hardware).
[Cemu's MMU implementation](https://github.com/cemu-project/Cemu/blob/main/src/Cafe/HW/MMU/MMU.cpp)
was checked as an effective-address reference; its title map is not implemented
here. The supplied flat physical profile does not implement address translation,
firmware reset, measured timing or all Espresso extensions.
