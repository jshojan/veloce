# Experimental Nintendo 64 core plugin

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo 64 | **35%** | 7/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo 64 delivered milestones: Guest sound produces audio samples, Native digital controller/keypad registers consume frontend input, Parse a native cartridge/executable format (boot scored separately), Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Super Mario 64 reaches 5,896,277 startup attempts, then faults on FPU CFC1; native boot/playability, RAM remapping/calibration, remaining CPU/FPU/devices, RSP/RDP/VI, RSP audio microcode, physical AI timing, analog and netplay remain.
<!-- completion:end -->

`n64_plugin` builds `n64.so`/`n64.dll`/`n64.dylib` and implements emulator ABI v2.
The frontend discovers it alongside the other core plugins. It executes original
MIPS III integer homebrew ROMs with 64-bit GPRs, big-endian memory, delay slots,
HI/LO, and basic CP0 exception context. An experimental NTSC CIC-6102 handoff accepts recognized native cartridges; **retail boot and gameplay remain unverified.**
Full-core completion is unknown; the available corpus measures defined results
on selected operands, not a percentage of N64 hardware implemented.

## Implemented execution

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
  instruction, alignment/address exception EPC/BD context. CP0 is partial;
  most register masks, privilege modes and precise bus/TLB
  exception classes still need implementation.
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
producers and reset/NMI/cache-error paths are unverified. The current `VN64S009` state stores Count, Compare, Cause, Status and pending SI/PI/SP/AI/MI state. Device levels are resampled on the next step.

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

`VN64S009` states store explicit big-endian scalars, all GPR/CP0 registers,
HI/LO, both PCs, pending delay state, instruction/frame counters, completion,
RAM and SP storage, PIF RAM, SI address/completion, MI mask, latched digital
input and debug yield state, plus 13 PI registers, PI IRQ and boot mode (120 bytes), six RI controls and a current-loaded flag (56 bytes), plus MI mode and pending DP (16 bytes) and 36 device-register scalars (288 bytes), plus all cache tags/valid/dirty flags and data (40,960 bytes) and nine SP interface scalars (72 bytes), plus 15 AI scalars, five host-clock/ring scalars and 32,768 bytes of pending PCM storage (32,928 bytes). Legacy `VN64S008` restores neutral AI/host audio; `VN64S007` restores reset SP controls; `VN64S006` restores invalid caches; `VN64S005` restores reset device registers; `VN64S004` states restore with neutral MI mode/DP; `VN64S003` states restore with neutral RI; `VN64S002` loads only for homebrew, with neutral PI/RI; `VN64S001` is rejected. ROM data is immutable and referenced by identity. Size,
version, ROM identity and basic field validation precede restoration. The
instruction counter counts CPU step attempts including interrupt entry,
**not actual VR4300 cycles**;
each frontend frame is a bounded 100,000-instruction scheduling quantum,
not an emulated VI field. The nominal metadata rate is NTSC/93.75 MHz only.

## Remaining hardware and modern features

PIF/IPL/CIC and retail boot, TLB/address modes, cache errors and pipeline timing,
full CP0, remaining interrupt producers and timing, LL/SC, traps,
FPU, RSP/vector instructions, RDP, VI, full AI/DAC physical behavior, full RDRAM initialization and remaining RI/PI behavior, full SI/MI/PIF,
analog and multiplayer controllers,
save media, video/audio output, expansion devices and physical hardware
validation remain. Unimplemented opcodes raise a reserved-instruction exception.
Unsupported memory access currently uses an address exception approximation.
Merge memory supports big-endian unmapped RAM/SP storage; TLB, reverse-endian
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
RI register configuration and reaches MI control setup; the local target now reaches the unsupported FPU control instruction CFC1.

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
RSP BREAK/VI/DP hardware producers and physical bus/interrupt timing remain incomplete.

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
The plugin **loads it and reaches 5,896,277 startup step attempts**, then faults on
the unsupported FPU control instruction CFC1 at PC `$80328730` (word `$4442F800`). EPC is `$80328730`, Cause is `$28`,
and the exception vector is `$80000180`. The production ABI probe runs 120
frontend scheduling quanta with uniform video and no audio. **Retail boot and
playability are unverified.** The 8 MiB image has SHA256
`17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91`.
It is not included in this repository.

```bash
cmake --build build --target retail_core_probe n64_boot_trace n64_plugin
python3 tests/n64_retail_probe.py /path/to/your/sm64.z64
```

The optional probe loads the production shared library, checks ABI, measures
video/audio and runs a bounded machine trace to the first CPU exception.
It writes hashes, EPC, Cause, fault address and all 32 GPRs to
`build/artifacts/retail/n64.json`; no commercial bytes are archived or copied.
The current verdict is `boot-blocked`. A successful probe process is not a
playability verdict. Next work includes FPU and native RAM mapping, full CPU/CP0/FPU,
RSP/RDP/VI/device scheduling, physical AI timing and save media.


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
are included in `VN64S009`. Replay and malformed-state rejection are tested.

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
