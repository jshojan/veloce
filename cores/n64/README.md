# Experimental Nintendo 64 core plugin

Current N64 evidence: [FPU conversions and current Mario blocker](#fpu-conversion-and-fixed-rounding-family). Earlier increment reports are historical.

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo 64 | **40%** | 8/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo 64 delivered milestones: Guest sound produces audio samples, Native digital controller/keypad registers consume frontend input, Guest graphics produce rendered video, Parse a native cartridge/executable format (boot scored separately), Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Super Mario 64 reaches 6,212,339 startup attempts, then encounters unsupported DIV.S; native boot/playability, remaining CPU/FPU/devices, RSP/RDP, VI filters and physical timing, RSP audio microcode, analog and netplay remain.
<!-- completion:end -->

`n64_plugin` builds `n64.so`/`n64.dll`/`n64.dylib` and implements emulator ABI v2.
The frontend discovers it alongside the other core plugins. It executes original
MIPS III integer homebrew ROMs with 64-bit GPRs, big-endian memory, delay slots,
HI/LO, and basic CP0 exception context. An experimental NTSC CIC-6102 handoff accepts recognized native cartridges; **retail boot and gameplay remain unverified.**
Full-core completion is unknown; the available corpus measures defined results
on selected operands, not a percentage of N64 hardware implemented.

## Implemented execution

- Functional VI scanline/field and IRQ/MI controls, plus unfiltered RGBA16/RGBA32
  guest-RAM framebuffer output with fixed-point scale/offset. Filters and RSP/RDP remain pending.
- AI stereo guest PCM, FIFO/IRQ and deterministic 48 kHz host output; physical
  timing, DAC/filter behavior and RSP audio microcode remain pending.

- 32 paired-page TLB entries, seven page sizes, ASID/global matching, valid/dirty
  permissions, TLBR/TLBWI/TLBWR/TLBP, masked CP0 fields and DMFC0/DMTC0.
  Translated instruction/data/cache access, 32/64-bit segments and privilege checks.

- FPU MFC1/MTC1/DMFC1/DMTC1 and LWC1/LDC1/SWC1/SDC1 data transfers in both FR modes; CFC1/CTC1 control transfers and BC1F/BC1T/BC1FL/BC1TL branches;
  CU1/FPE exception context and persistent FCR31. CVT.S/D/W/L and ROUND/TRUNC/CEIL/FLOOR W/L conversion families are implemented; arithmetic and comparison remain pending.

- Word and doubleword add/subtract, trapping and nontrapping forms; logic,
  signed/unsigned comparisons, immediate arithmetic and logic.
- Word and doubleword shifts, immediate/variable forms (including +32 forms).
- Word and doubleword signed/unsigned multiplication/division, MFHI/MFLO/MTHI/MTLO.
- J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ, REGIMM sign branches and link forms,
  plus likely variants with an annulled untaken delay slot.
- Aligned byte/halfword/word/doubleword loads/stores, sign/zero extension.
- Big-endian LWL/LWR/LDL/LDR merge loads and SWL/SWR/SDL/SDR merge stores.
  Masked stores preserve untouched bytes without a bus read-modify-write read.
- Basic MFC0/MTC0, ERET from EXL/ERL, integer overflow, SYSCALL/BREAK, reserved
  instruction, alignment/address and TLB/bus exception EPC/BD context.
  Full CP0, cache-error/shutdown/reset/NMI paths and pipeline hazards remain incomplete.
- 8 MiB RDRAM, KSEG0/KSEG1 aliases, read-only cartridge mapping, and 8 KiB SP
  memory, SP PC/status/semaphore controls and settled bidirectional SP DMA.
  RSP scalar/vector execution and DMA scheduling remain pending.

Implementation is original C++ and grounded in the
[NEC VR4300 user manual](https://hack64.net/docs/VR43XX.pdf), especially the
instruction descriptions and exception EPC/BD behavior. No SDK, Nintendo IPL,
PIF ROM, copyrighted boot logo, or commercial ROM is included.

## Digital controller subset

One standard controller supports all 14 digital buttons, identify/reset commands
(`00`/`FF`, identifier `05 00`, accessory absent `02`), and read command `01`.
Ports 2–4 and the cartridge channel report no response; accessories are unsupported.
The analog bytes are zero. Aligned 64-byte SI DMA transfers between RDRAM and
PIF RAM complete immediately. SI status bit 12 and MI pending bit 1 indicate
completion; writing SI status clears both. MI mask clear/set commands and mask
reads drive CPU IP2; interrupt delivery is implemented as described below. Direct
PIF RAM accesses are available. Invalid DMA bounds/alignment fail without mutation.
Busy/error timing, overlapping DMA, PIF boot/other commands, accessory protocols,
analog input, port routing and physical hardware validation remain unverified.

Register definitions follow the archived
[Nintendo RCP header](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm);
controller identification follows [libdragon Joybus definitions](https://github.com/DragonMinded/libdragon/blob/trunk/include/joybus.h).
This is settled functional behavior, not a measured model of serial timing.

The original [controller ROM](../../tests/roms-src/n64/controller_serial/README.md)
matched **16,384/16,384 digital cases**, with 131,072 actual guest register values
covering every combination of 14 buttons, identification, absent-port responses,
DMA completion/mask/pending registers and acknowledgement. A mutated read command
failed all 16 negative cases; seven damaged result files were rejected. Unit tests
also cover all 4,032 nonzero-transmit length combinations, bounded malformed
packets, identify/reset, channel skips, MI masks, DMA failure atomicity and
pending-transfer state restoration.

Keyboard defaults: A=Z, B=X, Z=C, Start=Enter, D-pad=arrows, L=Q, R=W,
C-Up=I, C-Down=K, C-Left=J, C-Right=L. The input configuration panel exposes
these controls; custom keyboard/gamepad/axis bindings use the existing persistence
and hotplug system. Physical pads and rendered UI behavior remain unverified.

## Interrupt subset

CPU interrupt gates now honor all eight Status mask bits, IE, EXL and ERL.
Software Cause.IP0/IP1 are writable; SI, PI, SP and AI drive live IP2 after MI masking.
The approximate Count clock latches IP7 when an increment equals Compare;
writing Compare acknowledges that latch. Cause writes preserve hardware/timer
requests and exception context. Interrupt entry suppresses the current
instruction, records EPC/BD, sets EXL and uses the BEV-selected general vector.
ERET selects ErrorEPC and clears ERL when error level is active; otherwise it
selects EPC and clears EXL. MTC0 writes to EPC/ErrorEPC sign-extend their word.
Pending device levels can re-enter after ERET until acknowledged.

The original [interrupt ROM](../../tests/roms-src/n64/interrupts/README.md)
matched **65,536/65,536 cases** (524,288 guest values) across software/SI/timer
requests, MI masks, all 256 CPU masks and all IE/EXL/ERL states. Separate component
tests cover all eight logical requests, both BD contexts and both BEV vectors:
**2,097,152 gates**. IP3–IP6 are injected only for those CPU tests; their hardware
producers remain absent. Compare/wrap/latch, protected Cause fields, ERET priority,
SI re-entry and pending SI/timer save-state continuation also have unit evidence.

Count still advances once per two CPU step attempts, including interrupt entry.
This is an instruction-clock approximation. Exact PClock/pipeline timing,
CP0 hazards, Count-write equality timing, privilege checks, other interrupt
producers and reset/NMI/cache-error paths are unverified. The current `VN64S013` state stores Count, Compare, Cause, Status and pending SI/PI/SP/AI/VI/MI state. Device levels are resampled on the next step.

## Explicit homebrew entry profile

Images marked `V64HB001` at normalized header offset `$34` use direct homebrew entry. They also
need the standard `$80371240` big-endian cartridge prefix, an aligned entry in
KSEG0/KSEG1 RAM at header offset `$08`, and a payload starting at ROM offset
`$1000`. The loader zeroes RAM, copies that payload to physical RAM `$400`, and
enters the header's address with zero GPRs/HI/LO and interrupts disabled in
kernel mode with Config.K0=2 (uncached KSEG0). This is a repository-defined test entry profile, **not PIF/IPL/CIC
boot emulation**. The entry must point within the copied payload. The loader
rejects unsupported native variants and malformed/truncated headers without replacing an
already loaded machine. Maximum homebrew image size is bounded by the RAM payload size. Native images are bounded at 64 MiB.

`.z64` big-endian, `.v64` byte-pair swapped and `.n64` four-byte reversed images
are normalized before execution. Test file CRC32 identifies the entire raw
input file. State identity additionally binds the normalized full image.

Emulator-only observation registers live at uncached `$BFF00000`: `SB`
forwards one character to `ITestSink`; `SW` to `$BFF00004` ends with an unknown
verdict. `SW` to `$BFF0000C` yields the current frontend quantum so the
controller corpus can synchronize one observation with each host input event.
These addresses are debug instrumentation, not Nintendo hardware.
The frontend owns register files. Ordinary 16-bit RAM-watch API accesses map
the first 64 KiB of RAM; a wider inspection API remains needed.

## Evidence and reproduction

The original [integer ROM](../../tests/roms-src/n64/integer_registers/README.md)
records 28,160 observations for 20 operations. Its independent Python oracle
matches all **28,127 defined results**; 33 divide-by-zero/overflow observations
are retained but unscored. All low-byte shift amounts are exercised on 16
patterns per word width, and arithmetic/HI/LO operations use 16×16 patterns.
This is not exhaustive over all 32-bit/64-bit values or the full ISA.

The original [doubleword multiply/divide ROM](../../tests/roms-src/n64/muldiv64/README.md)
adds **262,623/262,623 defined LO/HI pairs** for DMULT/DMULTU/DDIV/DDIVU,
with 263,168 total observations and 545 undefined inputs unscored. It executes
every signed/unsigned byte pair as 64-bit inputs, plus 16×16 full-width
boundaries. Products use portable 32-bit limbs in the core and independent
Python 128-bit arithmetic in the oracle. Signed division uses unsigned
magnitudes so min/-1 and divisor zero cannot invoke undefined host arithmetic.
The full 64-bit operand domain and multiply/divide stalls/hazards remain pending.

The original [merge-memory ROM](../../tests/roms-src/n64/merge_memory/README.md)
adds **3,158,016/3,158,016 observations** for eight merge instructions, every
word/doubleword lane, all pairs of repeated-byte values and 16×16 full-width
boundaries. Its independent byte-array oracle checks loaded GPRs and stored
memory, including sign extension and an adjacent word-store sentinel. This is
not exhaustive over independently varying multi-byte operands.

`n64_machine_test` checks taken/untaken/likely branch paths, delay execution,
JAL/JALR link/alias cases, $zero, word/doubleword overflow and EPC/BD, nested
exceptions, ERET, SYSCALL/BREAK/RI, alignment faults, big-endian load/store
extension, aliases, ROM protection, doubleword multiply/divide references and
full-width HI/LO state continuation, paired merge accesses in both orders,
unaligned cross-block results, guard bytes, base/target aliasing, $zero, negative
offsets, protected/masked stores and partial-merge state continuation,
pending-delay state restore, and actual
shared-plugin state continuation equality. It also checks all three image byte
orders, invalid state/load rejection, and unload. The ROM runner separately
executes all three archive formats through actual frontend discovery.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce n64_plugin n64_machine_test n64_pi_test n64_ri_test n64_mi_test n64_rdram_test n64_cache_test n64_sp_test n64_ai_test n64_audio_capture -j
ctest --test-dir build -R '^n64_' --output-on-failure
```

`VN64S013` states store explicit big-endian scalars, all GPR/CP0 registers,
HI/LO, both PCs, pending delay state, instruction/frame counters, completion,
RAM and SP storage, PIF RAM, SI address/completion, MI mask, latched digital
input and debug yield state, plus 13 PI registers, PI IRQ and boot mode (120 bytes), six RI controls and a current-loaded flag (56 bytes), plus MI mode and pending DP (16 bytes) and 36 device-register scalars (288 bytes), plus all cache tags/valid/dirty flags and data (40,960 bytes) and nine SP interface scalars (72 bytes), plus 15 AI scalars, five host-clock/ring scalars and 32,768 bytes of pending PCM storage (32,928 bytes). FCR31 is stored as an 8-byte validated scalar, followed by 32 TLB entries as four 8-byte scalars each (1,024 bytes). VI adds 20 validated 8-byte scalars (160 bytes), including registers, current line, crystal phase, field count, leap pattern and interrupt latch. FPR data appends 32 raw 64-bit scalars (256 bytes); legacy `VN64S012` restores zero FPR storage. `VN64S011` restores reset VI; `VN64S010` restores neutral TLB entries; `VN64S009` restores neutral FCR31; `VN64S008` restores neutral AI/host audio; `VN64S007` restores reset SP controls; `VN64S006` restores invalid caches; `VN64S005` restores reset device registers; `VN64S004` states restore with neutral MI mode/DP; `VN64S003` states restore with neutral RI; `VN64S002` loads only for homebrew, with neutral PI/RI; `VN64S001` is rejected. ROM data is immutable and referenced by identity. Size,
version, ROM identity and basic field validation precede restoration. The
instruction counter counts CPU step attempts including interrupt entry,
**not actual VR4300 cycles**;
each frontend frame is a bounded 100,000-instruction scheduling quantum,
not an emulated VI field. The nominal metadata rate is NTSC/93.75 MHz only.

## Remaining hardware and modern features

PIF/IPL/CIC and retail boot, TLB multiple-hit/shutdown behavior and remaining address edge cases, cache errors and pipeline timing,
full CP0, remaining interrupt producers and timing, LL/SC, traps,
FPU arithmetic/comparison, RSP/vector instructions, RDP, VI filters/diagnostic line buffer/physical timing, full AI/DAC physical behavior, full RDRAM initialization and remaining RI/PI behavior, full SI/MI/PIF,
analog and multiplayer controllers,
save media, retail graphics and RSP audio microcode, expansion devices and physical hardware
validation remain. Unimplemented opcodes raise a reserved-instruction exception.
Unsupported aligned physical accesses raise IBE/DBE; precise physical board/open-bus responses remain unverified.
Merge memory supports translated big-endian RAM/SP storage; reverse-endian
modes, MMIO partial writes and precise transaction timing remain pending.
There is no N64 netplay capability yet. Video is a blank buffer, audio is empty,
and digital input is routed to the first SI/PIF controller. Windows/macOS builds and rendered GUI usage
remain unverified. The external four-platform suite matrix does not yet include
N64; its original ROM and component gates run locally with CTest; CI/CD is disabled.

## Native cartridge handoff and settled PI DMA

Recognized unmarked cartridges must have an aligned KSEG0 RAM entry, NTSC country
`E` or `J`, and IPL3 CRC32 `90bb6cb5` over normalized bytes `$40..$FFF`.
Other native boot variants reject. The original `V64IP001` marker opts a test
image into the same handoff. Reset copies only IPL3 into SP DMEM `$40`, leaves
RDRAM zero, and enters `$A4000040`. It supplies Status `34000000`, registers
s3=0, s4=1, s5=0, s6=63, s7=1, SP=`A4001FF0`, RA=`A4001550`, and header-derived
PI domain-1 timing values, with Config.K0=3 selecting cached KSEG0. This is an explicit HLE handoff; PIF ROM execution,
CIC authentication, full reset state and successful native boot remain pending.
The setup follows independently authored [flashcart boot code](https://github.com/Polprzewodnikowy/N64FlashcartMenu/blob/main/src/boot/boot.c)
and [PIF handoff code](https://github.com/mikeryan/n64dev/blob/master/src/boot/pif.S).
No firmware bytes are bundled.

Aligned 32-bit PI registers support cartridge-ROM reads into 8-byte-aligned
RDRAM with even lengths and 2-byte-aligned cartridge sources. Transfers settle
immediately: addresses advance, WR_LEN reads 127, status bit 3/MI bit 4 latch,
and masked PI drives CPU IP2. Status write bit 1 acknowledges PI without
clearing SI. Domain latency/pulse/page/release fields store 8/8/4/2 bits;
they do not model transfer delay. Unsupported transfers reject before mutation.
Peripheral/save writes, odd/misaligned transfer quirks, busy/error timing and
hardware measurement remain pending. The supported alignment follows
[libdragon DMA documentation](https://github.com/DragonMinded/libdragon/blob/trunk/include/dma.h);
register/post-transfer behavior is grounded in the authored
[PI register model](https://github.com/ares-emulator/ares/blob/master/ares/n64/pi/io.cpp)
and [DMA model](https://github.com/ares-emulator/ares/blob/master/ares/n64/pi/dma.cpp).

The [original PI corpus](../../tests/roms-src/n64/pi_dma/README.md) matches
524,288 cases / 8,388,608 guest values across every even length 2–65,536,
four cartridge offsets, two RDRAM addresses and both MI mask states.
The [original handoff corpus](../../tests/roms-src/n64/ipl3_handoff/README.md)
matches all twelve startup captures. Executed wrong-length/seed programs,
all captured-field mutations and seven malformed streams per corpus are rejected.
`n64_pi_test` separately covers 1,048,576 RDRAM destinations, 32,768 even lengths,
65,536 address masks, every byte of an 8 MiB DMA, all SI/PI/MI combinations,
actual IRQ/ACK/ERET continuation, atomic rejection, legacy/native states,
64 MiB formats and the loaded plugin. All twenty-four N64 CTests pass; the component
also passes undefined-behavior sanitization.

## RI configuration controls

Aligned 32-bit RI accesses at `$A4700000..$A470001C` implement mode (4 bits),
configuration (7), selection (two 3-bit fields at bits 0 and 4), refresh (19)
and latency (4). Any current-load write records the current configuration and
marks the internal latch loaded; later configuration writes leave that latch
unchanged. Error indication is settled zero, with no bus-error producer yet;
write-error accepts an acknowledgement. Write-only reads and read-only writes
reject, as do unsupported widths and alignment. Fields follow the archived
[Nintendo RCP header](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm).

The [original RI corpus](../../tests/roms-src/n64/ri_registers/README.md) matches
524,288 cases / 3,145,728 actual guest values: every refresh value and all
mode/configuration/selection combinations, with correlated latency values.
An executed selection mutation fails all 256 reduced rows; six field mutants
and seven malformed streams are rejected. `n64_ri_test` separately exercises
current-load latching, 65,536 error-clear inputs, MMIO/state rejection atomicity,
exact continuation, legacy states, reset and the loaded plugin.

These are functional controls. Analog current calibration, mode-dependent RAM
access, refresh scheduling, bus contention/error production, native RAM remapping
and physical timing remain pending. Native cartridge startup now proceeds past
RI register configuration and reaches MI control setup; the local target now reaches unsupported DIV.S (see the retail probe above).

## MI mode and interrupt controls

Aligned 32-bit mode accesses replace the seven-bit repeat count and apply
clear/set commands for repeat, EBus and upper controls. Set wins when both
commands are supplied. Bit 11 acknowledges DP independently of SI/PI.
Revision reads `02020102`; writes to revision or pending indication are ignored.
All six interrupt masks support clear/set commands. SI/PI and a pending DP latch
drive masked CPU IP2; DP has no production RDP interrupt producer yet.
[libdragon MI definitions](https://github.com/DragonMinded/libdragon/blob/trunk/include/mi.h)
and the authored [MI register model](https://github.com/ares-emulator/ares/blob/master/ares/n64/mi/io.cpp)
ground the functional controls.

The [original MI corpus](../../tests/roms-src/n64/mi_controls/README.md) matches
393,216 rows / 3,145,728 guest captures, with real SI/PI pending requests:
all mode command words across eight flag states and all mask command words
across all 64 masks. The component covers all 16,777,216 prior-mode/command
transitions, injected DP gates/acknowledgment and CPU IRQ/ERET, exact state replay
and legacy conversion. Executed count mutation, eight field mutants and seven
invalid streams reject. The component passes undefined-behavior sanitization.

Repeat/upper device-register effects are implemented in the RDRAM profile.
EBus, ninth-bit RAM, repeat RAM writes and native RAM remapping remain pending.
RSP BREAK/DP hardware producers and physical bus/interrupt timing remain incomplete.

## RDRAM device register topology

Four 2 MiB devices support selected and broadcast register accesses, disabled
chain discovery, encoded device IDs, enable/mode readback, write-delay masks,
refresh/interval/address storage and row aliases. RI selection/current-load gates
the device bus. Aligned word accesses cover `$03F00000..$03FFFFFF`; inactive,
absent, reserved and broadcast reads settle at zero. Writes to type/manufacturer
are ignored. MI upper controls uncached odd-word register reads, and a repeated
uncached register write consumes repeat mode once. Non-unit initial write delay
requires at least sixteen repeated bytes and swaps the submitted halfwords.
The authored [RDRAM model](https://github.com/ares-emulator/ares/blob/master/ares/n64/rdram/rdram.cpp)
and [MI bus model](https://github.com/ares-emulator/ares/blob/master/ares/n64/mi/bus.hpp)
ground the functional behavior; implementation is original C++.

The [original RDRAM corpus](../../tests/roms-src/n64/rdram_devices/README.md)
matches 14,336 cases / 86,016 guest values with selected-device isolation,
all delay fields and repeated-byte storage operands. Executed mutation, six
field mutations and seven invalid streams reject. `n64_rdram_test` additionally
covers 262,144 ID writes, 2,113,536 delay/repeat cases, 131,072 selector/register
aliases, mode patterns, discovery, broadcast, MI controls, exact state replay,
atomic rejection and legacy/native state conversion. The component passes
undefined-behavior sanitization.

Native RAM remapping remains pending; ordinary RAM and PI/SI DMA still use the
existing direct 8 MiB storage profile. Analog current calibration/degradation,
refresh scheduling, ordinary-RAM repeat writes, EBus ninth-bit memory, physical
timing and device cache-burst transactions remain incomplete. Stored current/mode
readback is functional and does not establish physical calibration or boot.

## Functional primary caches

CPU instruction fetches and data loads/stores now use separate direct-mapped
caches for KSEG0 when Config.K0 differs from 2. KSEG1 and the homebrew reset
configuration bypass them. Instruction/data cache sizes are 16/8 KiB with
32/16-byte lines. Dirty data evicts to the address stored in its tag; uncached
CPU and PI/SI DMA writes do not snoop either cache. Masked merge stores update
enabled cached bytes without changing neighboring bytes.

Thirteen defined CACHE sub-opcodes support index invalidate/writeback,
index load/store tag, create dirty exclusive, hit invalidate/writeback and
explicit instruction fill. TagLo reflects valid D lines as state 11 even when
clean. CACHE checks CU0 outside kernel/exception levels; unsupported encodings
raise RI, and unsupported addresses/bus failures use the existing address-fault
approximation. Full TLB/privilege, parity/error, Status/cache diagnostic behavior,
cycle timing, hazards, bus ordering and extended address modes remain pending.
Details follow the [VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapters
5/11 and instruction descriptions; code is original C++.

The [original cache ROM](../../tests/roms-src/n64/cache_coherence/README.md)
matches 2,097,152 cases / 12,582,912 guest values across every D index/byte lane
and initial byte. Executed writeback mutation, six field mutants and seven
invalid streams reject. `n64_cache_test` separately checks all 1,048,576 tag
values, 1,048,576 instruction staleness/refill cases, index/offset selection,
create dirty exclusive, index versus hit operations, widths/merged masks,
real PI DMA staleness, executed stale instructions, CACHE opcode/privilege
checks, dirty/stale state replay, rejected cache fields and loaded-plugin states.
The component passes undefined-behavior sanitization.

Native startup now executes past cache initialization into the RAM bootstrap.
Native RAM chip remapping/calibration and remaining devices still prevent a
verified boot. The bounded trace now allows 12,000,000 attempts, matching the
120-frame production probe's scheduling budget; neither proves gameplay.

## Local retail regression target

Super Mario 64 (USA), supplied locally by the user, was probed on 2026-10-10.
The plugin **loads it and reaches 6,212,339 startup step attempts**, then
encounters unsupported `DIV.S` at EPC `$803259A0`, actual instruction-cache
and backing word `$460A3483`, Cause `$10000028` (RI code 10). There were 22
earlier nonterminal exception entries. The production ABI probe runs 120
frontend scheduling quanta with uniform video and zero audio pairs.
**Retail boot and playability are unverified.**
The 8 MiB image has SHA256
`17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91`.
It is not included in this repository.

```bash
cmake --build build --target retail_core_probe n64_boot_trace n64_plugin
python3 tests/n64_retail_probe.py /path/to/your/sm64.z64
```

The optional probe loads the production shared library, checks ABI, measures
video/audio and runs a bounded machine trace through interrupts and recoverable TLB/FPU exceptions to the first terminal exception.
It writes hashes, EPC, Cause, fault address and all 32 GPRs to
`build/artifacts/retail/n64.json`; no commercial bytes are archived or copied.
The current verdict is `boot-blocked`. A successful probe process is not a
playability verdict. Next work includes FPU and native RAM mapping, full CPU/CP0/FPU,
RSP/RDP, VI filtering and device pipeline scheduling, physical AI timing and save media.


## SP controls and settled DMA

The CPU interface now maps SP controls at `$A4040000`–`$A404001C` and
PC at `$A4080000`. HALT resets set; the PC is a word-aligned 12-bit value.
Status commands control HALT, single step, interrupt on break, eight signals,
BROKE clearing and the SP interrupt. Simultaneous set/clear preserves a bit.
Semaphore reads atomically return its previous value and acquire it; any write
releases it. SP interrupts connect through MI mask bit 0 to CPU IP2.

DMA copies in either direction between RDRAM and one selected 4 KiB SP bank.
Addresses align down to eight bytes, encoded lengths round up to eight bytes,
and row count/stride are supported. Bank overflow wraps within DMEM or IMEM.
Transfers settle synchronously and do not snoop CPU caches. Unsupported RAM
addresses and non-eight-byte strides reject before changing memory or controls.
The interface retains staged addresses separately from last-transfer readback;
terminal lengths use `skip | 0xFF8`. Exact DMA address/length readback and
unaligned-stride behavior still need direct physical reference measurements.
Busy/full queues, DMA clocks, RSP instruction execution, BREAK production,
IO-full behavior and running-PC writes are incomplete. Clearing HALT changes the
control bit; it does not execute microcode yet.

The [control ROM](../../tests/roms-src/n64/sp_controls/README.md) matches
**196,608 cases / 1,572,864 guest values**: every 4,096 writable status/interrupt
seed, twelve command pairs and all four commands per pair. Captures include
status, MI interrupt levels, semaphore, PC and CPU Cause. The
[DMA ROM](../../tests/roms-src/n64/sp_dma/README.md) matches **65,536 cases /
393,216 guest values**, both directions/banks, four start boundaries and every
12-bit raw length. An executed mutation, every captured field mutation and
seven malformed streams reject in each corpus. The component checks every
transferred byte and surrounding bank/RAM guards, all 256 counts × 512 aligned
strides through both directions, cache staleness, interrupt delivery/ERET,
state replay/rejection, reset and loaded-plugin execution. Undefined-behavior
sanitization passes. These domains do not establish full RSP accuracy.

Behavior is grounded in the original [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf),
CPU/register/DMA chapters, and the author's hardware observations in
[nemu64-test SP registers](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/registers.rs)
and [DMA tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/sp_memory/dma.rs).
The C++ implementation and cartridge programs here are original; these references
were read for behavior and have not been run or copied into the corpus.


## AI playback and plugin audio

The AI module maps six word registers at `$A4500000`–`$A4500014`. It maintains
two DMA descriptors and consumes signed big-endian 16-bit stereo from RDRAM at
playback time. Length writes align down to eight bytes and use 18 bits; address
writes use 24 bits and eight-byte alignment. A full FIFO ignores further length
writes. Write-only registers read back current length. Register reads expose
FIFO busy/full, enable and fixed status bits; COUNT/WC/BC readback is unmodeled
and unscored. Status writes acknowledge AI's MI source (bit 2).

Activation of each FIFO descriptor raises the AI interrupt, including promotion
of a queued buffer; draining the final buffer alone does not raise a new one.
The documented delayed-carry bug adds 8 KiB to the following descriptor when a
buffer ends on an 8 KiB boundary. Transfers outside implemented RAM reject before
queue mutation, including this carry. The PCM ROM avoids those boundaries; a
separate component checks the carry with distinct RAM values. DMA observes
backing RAM rather than dirty CPU cache lines.

DACRATE selects the crystal period `value + 1`. BITRATE zero freezes the DAC
clock and suppresses new host samples; DMA disable freezes consumption. Exact
serial-bit distortion for short periods, analog interpolation/filtering, dynamic
clock-register phase rules and physical FIFO/IRQ latency remain unverified.
The current profile uses NTSC's 48,681,818 Hz crystal against nominal 93,750,000
CPU clocks, with one clock per interpreter attempt. That CPU/device relationship
is an approximation; the 100,000-attempt frontend quantum is not a VI frame.
PAL/MPAL and hardware timing need separate implementation and evidence.

The core converts PCM to normalized floats and 48 kHz using deterministic sample
holding. Buffered `get_audio()` and the existing streaming callback return
identical samples and leave identical whole states after consumption. Pending
host output is bounded to 8,192 stereo pairs; a stalled consumer drops the oldest
host samples without stopping guest DMA, with a stateful drop counter. FIFO,
partial word position, raw sample latches, clock phases and pending host output
are included in `VN64S011`. Replay and malformed-state rejection are tested.

The [FIFO ROM](../../tests/roms-src/n64/ai_fifo/README.md) matches 2,048 cases /
22,528 guest register values. The [PCM ROM](../../tests/roms-src/n64/ai_pcm/README.md)
matches nine FIFO/IRQ captures and the production ABI emits **66,169 stereo
pairs covering all 65,536 signed sample values in each channel**. Independent
host grading rejects an executed sample mutation and malformed streams. Components
check all 262,144 length inputs, 2,097,152 aligned address fields, 262,144
DACRATE/BITRATE combinations, all 16,384 dividers, FIFO promotion, carry,
RAM/cache visibility, pause, IRQ/ERET, state replay and both plugin audio paths.
These are declared functional domains; they do not establish complete AI hardware
accuracy or RSP audio microcode. The component passes undefined-behavior sanitization.

References: the [AI hardware documentation](https://n64brew.dev/wiki/Audio_Interface?action=raw),
[libdragon register definitions](https://libdragon.dev/ref/group__lowlevel),
and the [authors' audio driver](https://github.com/DragonMinded/libdragon/blob/trunk/src/audio.c).
Implementation and ROM code are original. No external driver code or firmware
is included.

## FPU control registers and branches

CFC1/CTC1 implement FCR0 and FCR31 with CU1 unusable precedence (CE=1),
reserved-bit masking and write-before-trap semantics. Enabled IEEE causes or
unimplemented-operation cause trigger CPU FPE. FCR0 is read-only, implementation
0x0B/revision zero. BC1F, BC1T, BC1FL and BC1TL use the condition bit with signed
offsets and ordinary/annulled delay slots. FCR31 persists in `VN64S011`; S009 and
older states initialize it to zero. Reset-to-zero is a functional convention.

The [original ROM](../../tests/roms-src/n64/fpu_controls/README.md) matches
8,192 cases / 65,536 actual guest values, including a guest exception handler.
The component matches 1,048,576 control combinations, 524,288 branch offsets,
1,024 CU1 cases, transfers, exception contexts and state rejection/replay.
The profile follows NEC's original VR4300 manual, sections 7.2 and the six
control/branch instruction entries. Arithmetic, FPR data transfers, comparisons,
conversions, physical pipeline hazards and NaN/denormal behavior remain pending.
Unsupported/undefined COP1 encodings raise RI as an emulator limitation.
No CPU-complete milestone was awarded for this increment; N64 was **35%** before the VI graphics increment below.

The private Mario 64 probe now passes CFC1 and reaches unimplemented VI Current register at
EPC `803275F4`, word `8D8D0010`, Cause `1C` hex, after 5,897,464 attempts.
Boot/playability, retail graphics and retail sound remain unverified.

## TLB and translated memory

The CPU has 32 paired-page entries, the seven defined 4 KiB–16 MiB page sizes,
20-bit physical frame fields, 40-bit virtual address regions, ASID/global
matching and valid/dirty permissions. TLBR/TLBWI/TLBWR/TLBP and the implemented
CP0 Index/EntryHi/EntryLo/PageMask/Wired/Random/Context/XContext fields are
connected to instruction fetches, loads, stores, merge accesses and CACHE.
DMFC0/DMTC0 preserve implemented 64-bit fields and raise RI in 32-bit
user/supervisor mode; 32-bit kernel transfers remain defined. CP0/CACHE usability is checked
after a legal instruction fetch. User/supervisor/kernel segments, KX/SX/UX,
compatibility spaces, xkphys CCA and ERL bypass select translation/cache use.

Refill, invalid and modification faults update BadVAddr, EntryHi, Context and
XContext, preserve outer EPC/BD when nested, and select the general, refill or
extended refill vector with BEV. Alignment errors precede translation. Rejected
board transactions produce IBE/DBE and leave BadVAddr unchanged. Caches index
by virtual address and tag by physical address; mapped aliases remain
noncoherent with DMA/uncached writes until explicit maintenance.

The [original TLB ROM](../../tests/roms-src/n64/tlb_translation/README.md) matches
114,688 cases / 1,261,568 actual guest values. Component checks match 3,670,016
ASID/global/half combinations, 2,097,152 physical frames, 79,188 offsets,
229,376 independent permission/CCA cases and 4,096 mask inputs, plus CPU faults, control instructions, cache aliases and
state replay/rejection. `VN64S011` adds 1,024 bytes for the TLB; S010 converts
with neutral entries while retaining FCR31/audio and other implemented state.

Reset tags and undefined-mask/multiple-hit outcomes are functional conventions.
Multiple-hit shutdown/TS, physical Random/CP0/cache/TLB timing, pipeline hazards,
remaining address edge cases and physical board response validation remain
pending. This TLB increment left N64 at **35%**; the subsequent VI increment below raises delivery completion to 40%. The CPU remains incomplete.

The private Mario 64 trace now passes TLBWI and reaches a read of VI Current at
`A4400010`: EPC `803275F4`, decoded word `8D8D0010`, Cause `1C` (DBE), after
5,897,464 attempts. The trace records actual CPU exception entry and decoded
instructions; merely setting EXL in software is no longer labeled a fault.
Interrupt/refill/permission/FPE entries may be handled by guest code and do not
stop the diagnostic trace. Boot, gameplay and retail video/audio remain unverified.

## Video Interface and first framebuffer

The N64 core now exposes functional VI registers, scanline/field progression,
five-field H_LEAP selection, CURRENT acknowledgement and VI → MI → CPU interrupts.
The NTSC crystal advances at 48,681,818 / 93,750,000 per nominal CPU attempt;
physical CPI, DMA arbitration and sub-line sync timing remain unverified.
Odd V_TOTAL preserves field parity; even V_TOTAL alternates parity. Interrupt
comparison includes the documented odd-field early-match and V_INTR=0 behavior.
TYPE 0 holds CURRENT at zero; reserved TYPE 1 uses an emulator blank convention.

The production ABI derives an **unfiltered RGBA16/RGBA32 framebuffer** from RAM,
ORIGIN, WIDTH, visible spans and fixed-point scale/offset fields. Dirty CPU data
cache remains invisible until writeback. Host observation leaves guest state
unchanged. Host alpha is opaque because source low bits represent coverage.
Inactive/invalid visible spans return a 320×240 black framebuffer; unavailable
source pixels are black. Active dimensions follow the programmed visible spans.
Y offset's upper two stored bits do not affect rendering. Rendering is a snapshot,
not accumulated physical scanout. Antialiasing/divot/dither/gamma, coverage filters,
TEST_MODE staged data, pixel advance, extreme-scale errata, mid-frame changes,
RSP/RDP rendering and physical timing remain pending.

The [register ROM](../../tests/roms-src/n64/vi_registers/README.md) matches **65,536
rows / 983,040 guest values**. The [framebuffer ROM](../../tests/roms-src/n64/vi_framebuffer/README.md)
matches **24 guest values** and **131,072 actual production ABI pixels** across
two formats. Executed mutations and strict file/channel/framing controls reject
incorrect results. The component checks all **16,777,216 ORIGIN values**, all
**1,048,576 vertical-total/interrupt-target sequences** over multiple fields,
**4,096 horizontal periods**, all **65,536 RGBA16 words**, register bits, leap
selection, fractional scaling, memory/cache bounds, IRQ routing and save replay.
These are declared finite domains, not their full independent product or physical
hardware certification. No physical-console run has validated these cartridges.

`VN64S012` appends 160 validated VI state bytes. Legacy S011 restores reset VI;
earlier conversions retain their documented neutral state. Phase, field, leap
pattern and IRQ continuation replay exactly; malformed states reject atomically.
N64 is now **40% (8/20 delivery milestones)**: guest framebuffer output earns the
graphics milestone. This is not a hardware-accuracy or retail-compatibility score.

The private Super Mario 64 USA trace now reaches **6,212,303 attempts**, then
unsupported `LDC1`: EPC `80325978`, actual instruction-cache word `D7400130`,
Cause `10000028` (RI code 10). The backing word `44844000` differs because the
cache is noncoherent; the diagnostic records both. The trace passed 22 earlier
nonterminal exception entries. The 120-quantum production probe still observes
uniform video and zero audio pairs. Boot and playability remain unverified;
commercial ROM bytes are never committed. Earlier increment reports below/above
retain their historical blocker and percentage at the time of that increment.

Rules follow the [VI register reference](https://n64brew.dev/wiki/Video_Interface),
[original Nintendo VI manual](https://jrra.zone/n64/doc/pro-man/pro30/30-01.htm),
and [hardware timing reference](https://github.com/meauxdal/N64-Refresh-Rate-Reference/blob/main/N64_Timing_Reference.md).

Verification: **32/32 N64 CTests passed** in 668.56 seconds, including all original register ROMs, production PCM/video oracles and eleven component gates. The full build and VI undefined-behavior sanitizer passed; all **68 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU register and memory data transfers

The core implements all eight FPU data-transfer instructions:
MFC1/MTC1/DMFC1/DMTC1 and LWC1/LDC1/SWC1/SDC1. FR=0 pairs low words of even
registers with high words addressed by odd register numbers; FR=1 exposes 32
independent 64-bit registers. MFC1 sign-extends its word, MTC1 ignores high GPR
bits, and data movement preserves payload bits without interpreting NaNs or
changing FCR31. Doubleword moves are valid in 32-bit kernel mode and 64-bit
user/supervisor mode. Memory transfers use the CPU's existing translation/cache
path and enforce aligned 4-/8-byte accesses. CU1 unusable takes priority over
operand decoding or data access; failed loads leave FPR state untouched and
faulted stores preserve backing memory. EPC/BD, nested and BEV context are tested.

The [original register ROM](../../tests/roms-src/n64/fpu_data/README.md) matches
**131,072 cases / 1,179,648 actual 64-bit values**. Its independent host oracle
rejects an executed capture mutant, nine field mutants and seven malformed-stream
controls. The component checks **4,194,304 word-register/payload cases** and
**3,145,728 defined doubleword-register/payload cases**, **262,144 signed-offset
cases**, all GPR indices/zero, all 64 data-bit positions in every FR=1 register,
reserved transfer fields, privilege/CU1, alignment/TLB/bus exceptions and state
continuation. These are declared finite domains, not full independent products or
a complete FPU/hardware corpus. No physical-console run has validated this ROM.

`VN64S013` appends 32 raw 64-bit FPRs (256 bytes). S012 and earlier versions
restore zero FPR storage. Existing cache/VI/AI/TLB state and legacy conversions
retain their documented behavior. Undefined FR=1 upper word-write bits are
preserved; undefined odd FR=0 doubleword forms raise RI as deterministic emulator
conventions. Raw storage is retained across FR view changes, with physical
mode-transition behavior ungraded. Transfer pipeline hazards, physical timing,
reverse-endian mode and FPR arithmetic/conversion/comparison remain pending.
Rules follow the [NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf),
sections 7.2, 17.3 and the corresponding instruction entries.

Super Mario 64 USA now passes LDC1 and reaches **6,212,336 startup attempts**,
then unsupported `CVT.S.W` at EPC `80325984`, actual instruction-cache and backing
word `468042A0`, Cause `10000028` (RI code 10). It passed 22 earlier nonterminal
exception entries. The 120-quantum production probe still observes uniform video
and zero audio pairs; boot/playability remain unverified. Commercial ROM bytes
are never committed. Earlier increment records retain their historical blockers.
N64 remains **40% (8/20 delivery milestones)**; the whole CPU milestone is pending.

Verification: **34/34 N64 CTests passed** in 437.02 seconds, including twenty original register-ROM gates, two production audio/video oracles and twelve component gates. The full build and final FPU-data undefined-behavior sanitizer passed; all **69 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## FPU conversion and fixed rounding family

The core now implements CVT.S/D/W/L and ROUND/TRUNC/CEIL/FLOOR W/L using integer
bits, without host floating-point state. It supports nearest-even, toward-zero,
positive and negative directed rounding; fixed-mode instructions preserve FCSR
rounding controls. FCSR causes replace previous causes, sticky flags accumulate
only on nontrapping results, and enabled exceptions/software assist suppress
writes while preserving sources and old flags. FR views, long-target privilege,
CU1 precedence, delay/nested/BEV exceptions and S013 state replay are tested.

The [integer ROM](../../tests/roms-src/n64/fpu_integer_conversions/README.md) matches
**524,288 rows / 2,621,440 captured words**. The
[edge ROM](../../tests/roms-src/n64/fpu_conversion_edges/README.md) matches
**289,536 rows / 1,447,680 captured words**, including every S/D exponent field
on four fraction patterns and both signs, all conversion functions, selected full
enable/FS/rounding products, integer bit boundaries and reserved source formats.
Independent exact-rational host grading checks actual CPU/FPR/FCSR captures;
undefined upper result bits are recorded and ungraded. Executed capture mutants,
five field mutations per ROM and strict malformed-stream controls reject incorrect
results. The component checks **4,194,304 exact integer/index cases**, **229,376
exception-control combinations**, **98,304 register-pair/FR/format/function cases**,
fixed modes, CU1/privilege, cause replacement, exception context and state replay.

Rules follow the [NEC manual](https://hack64.net/docs/VR43XX.pdf), chapters 7–8,
conversion instruction entries and Appendix D, with ambiguities resolved by the
hardware-test author's [conversion cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs)
and [raw constants](https://github.com/lemmy-64/n64-systemtest/blob/main/src/cop1.rs).
Subnormal inputs require assist. Floating precision conversion preserves infinity;
NaNs with top payload bit 1 raise invalid and return a canonical legacy value,
while top-bit-0 NaNs require assist. Integer conversions reject both NaN types,
infinity and out-of-range results with assist. Long inputs support [-2^55, 2^55);
long outputs require magnitude below 2^53. Tiny results use FS/enable/rounding
rules, including rounding to the minimum normal and signed zero.

No physical console has validated these original cartridges. Their selected finite
domains do not exhaust the full independent operand product or whole FPU.
Undefined odd FR=0 arithmetic operands retain the RI convention; nonzero unary
`ft` encodings use an assist convention with physical behavior ungraded. Pipeline
hazards, precise CE timing, reverse-endian mode and FPU arithmetic/comparison
remain pending. State stays `VN64S013`; conversions add no serialized fields.
N64 remains **40% (8/20 delivery milestones)**; the whole CPU milestone is pending.

Super Mario 64 USA now passes CVT.S.W and reaches **6,212,339 startup attempts**,
then unsupported `DIV.S` at EPC `803259A0`, actual instruction-cache/backing word
`460A3483`, Cause `10000028` (RI code 10). It passed 22 earlier nonterminal exception
entries. The 120-quantum production probe still sees uniform video and zero audio
pairs; boot/playability remain unverified. Commercial ROM bytes are never committed.
Earlier increment reports retain their historical blockers and counts.

Verification: **37/37 N64 CTests passed** in 475.97 seconds, including twenty-two original register-ROM gates, two production audio/video oracles and thirteen component gates. The full build and final conversion undefined-behavior sanitizer passed; all **71 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.
