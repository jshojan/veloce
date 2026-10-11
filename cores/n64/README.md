# Experimental Nintendo 64 core plugin

Current N64 evidence: [Rejected color history and depth component foundation](#rejected-color-history-and-depth-component-foundation). Earlier increment reports are historical.

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo 64 | **45%** | 9/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo 64 delivered milestones: Guest sound produces audio samples, Native digital controller/keypad registers consume frontend input, Guest graphics produce rendered video, Parse a native cartridge/executable format (boot scored separately), Actual production-plugin TCP peers pass original guest oracle and whole-state comparison, Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Super Mario 64 passes SET_Z_IMAGE and real color/depth-buffer clears, then reaches the 200-million-attempt trace bound without a terminal CPU fault. RSP reciprocal/square-root/move now pass; the longer probe reports no unsupported RSP instruction and a real BREAK. RDP TMEM block/tile/palette transfers now pass; functional one-cycle RGBA32 shade/primitive/environment and affine/perspective textured/color-converted pixels with one-cycle blending/AA/alpha-coverage decisions pass original framebuffer/register oracles; Mario’s textured/depth-tested triangle profile remains blocked; video stays uniform. Native boot/playability, full CPU/CP0/devices, FPU physical timing, rare RSP operations and pipeline/interlock timing, complete RDP/audio, hidden coverage/depth bits, analog/accessories/presence detection, local multiplayer frontend routing and physical timing remain. Production two-player TCP rollback is verified for the implemented digital profile; four virtual PIF ports are supported.
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
  CU1/FPE exception context and persistent FCR31. CVT.S/D/W/L and ROUND/TRUNC/CEIL/FLOOR W/L conversion families and ADD/SUB/MUL/DIV/SQRT/ABS/MOV/NEG S/D are implemented; all sixteen S/D comparisons are implemented; physical pipeline behavior remains pending.

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
  RSP scalar instructions execute through shared SP controls and memory; DP control access and a bounded fill/sync command profile work. Vector transfers and multiply/accumulate work; remaining
  arithmetic, complete RDP and physical DMA/pipeline timing remain pending.

Implementation is original C++ and grounded in the
[NEC VR4300 user manual](https://hack64.net/docs/VR43XX.pdf), especially the
instruction descriptions and exception EPC/BD behavior. No SDK, Nintendo IPL,
PIF ROM, copyrighted boot logo, or commercial ROM is included.

## Digital controller subset

One standard controller supports all 14 digital buttons, identify/reset commands
(`00`/`FF`, identifier `05 00`, accessory absent `02`), and read command `01`.
Ports 1–4 now respond as virtual digital pads; the cartridge channel reports no response. Accessories are unsupported.
The analog bytes are zero. Aligned 64-byte SI DMA transfers between RDRAM and
PIF RAM complete immediately. SI status bit 12 and MI pending bit 1 indicate
completion; writing SI status clears both. MI mask clear/set commands and mask
reads drive CPU IP2; interrupt delivery is implemented as described below. Direct
PIF RAM accesses are available. Invalid DMA bounds/alignment fail without mutation.
Busy/error timing, overlapping DMA, PIF boot/other commands, accessory protocols,
analog input, connection detection and physical hardware validation remain unverified.

Register definitions follow the archived
[Nintendo RCP header](https://ultra64.ca/files/documentation/online-manuals/man/header/rcp.htm);
controller identification follows [libdragon Joybus definitions](https://github.com/DragonMinded/libdragon/blob/trunk/include/joybus.h).
This is settled functional behavior, not a measured model of serial timing.

The original [controller ROM](../../tests/roms-src/n64/controller_serial/README.md)
matched **16,384/16,384 digital cases**, with 131,072 actual guest register values
covering every combination of 14 buttons, identification, a neutral second pad,
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
producers and reset/NMI/cache-error paths are unverified. The current `VN64S019` state stores Count, Compare, Cause, Status and pending SI/PI/SP/AI/VI/MI state. Device levels are resampled on the next step.

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

`VN64S019` states store explicit big-endian scalars, all GPR/CP0 registers,
HI/LO, both PCs, pending delay state, instruction/frame counters, completion,
RAM and SP storage, PIF RAM, SI address/completion, MI mask, latched digital
input and debug yield state, plus 13 PI registers, PI IRQ and boot mode (120 bytes), six RI controls and a current-loaded flag (56 bytes), plus MI mode and pending DP (16 bytes) and 36 device-register scalars (288 bytes), plus all cache tags/valid/dirty flags and data (40,960 bytes) and nine SP interface scalars (72 bytes), plus 15 AI scalars, five host-clock/ring scalars and 32,768 bytes of pending PCM storage (32,928 bytes). FCR31 is stored as an 8-byte validated scalar, followed by 32 TLB entries as four 8-byte scalars each (1,024 bytes). VI adds 20 validated 8-byte scalars (160 bytes), including registers, current line, crystal phase, field count, leap pattern and interrupt latch. FPR data appends 32 raw 64-bit scalars (256 bytes). S019 appends a validated depth-image address (8 bytes); S018 and earlier restore its neutral value. S018 appends three combiner/color state scalars (24 bytes); S017 and earlier restore neutral combiner state. S017 appends eight validated raw 48-bit accumulator lanes in 64-bit slots (64 bytes); S016 and earlier restore neutral accumulators. S016 appends 64 packed vector doublewords and three flags (536 bytes); S015 and earlier restore neutral vector storage/flags. S015 appends 19 DP scalars (152 bytes); S014 and earlier restore reset DP control/counters/pipeline while preserving previously stored IRQ state. S014 appends scalar RSP execution and its clock phase (304 bytes); legacy S013 and earlier restore neutral scalar registers/phase and resume sequentially at SP PC. Legacy `VN64S012` restores zero FPR storage. `VN64S011` restores reset VI; `VN64S010` restores neutral TLB entries; `VN64S009` restores neutral FCR31; `VN64S008` restores neutral AI/host audio; `VN64S007` restores reset SP controls; `VN64S006` restores invalid caches; `VN64S005` restores reset device registers; `VN64S004` states restore with neutral MI mode/DP; `VN64S003` states restore with neutral RI; `VN64S002` loads only for homebrew, with neutral PI/RI; `VN64S001` is rejected. ROM data is immutable and referenced by identity. Size,
version, ROM identity and basic field validation precede restoration. The
instruction counter counts CPU step attempts including interrupt entry,
**not actual VR4300 cycles**;
each frontend frame is a bounded 100,000-instruction scheduling quantum,
not an emulated VI field. The nominal metadata rate is NTSC/93.75 MHz only.

## Remaining hardware and modern features

PIF/IPL/CIC and retail boot, TLB multiple-hit/shutdown behavior and remaining address edge cases, cache errors and pipeline timing,
full CP0, remaining interrupt producers and timing, LL/SC, traps,
FPU physical pipeline behavior, remaining RSP vector arithmetic/reciprocal state, complete RDP, VI filters/diagnostic line buffer/physical timing, full AI/DAC physical behavior, full RDRAM initialization and remaining RI/PI behavior, full SI/MI/PIF,
analog and multiplayer controllers,
save media, retail graphics and RSP audio microcode, expansion devices and physical hardware
validation remain. Unimplemented opcodes raise a reserved-instruction exception.
Unsupported aligned physical accesses raise IBE/DBE; precise physical board/open-bus responses remain unverified.
Merge memory supports translated big-endian RAM/SP storage; reverse-endian
modes, MMIO partial writes and precise transaction timing remain pending.
There is no N64 netplay capability yet. VI renders unfiltered guest RAM and AI
produces PCM; RSP/RDP game graphics and RSP audio microcode remain incomplete.
Digital input is routed to the first SI/PIF controller. Windows/macOS builds and rendered GUI usage
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
RI register configuration and reaches MI control setup; the local target now passes DIV.S and reaches the bounded trace limit (see the retail probe below).

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
The plugin loads the image and passes the former DIV.S, DPC_STATUS, LQV and
VMULF blockers. The default 120-quantum probe reaches 12,000,000 CPU attempts
without a terminal fault, with 52 nonterminal exception entries. The RSP retires
6,026 instructions and reaches a real BREAK (SP PC `$144`, status `$243`).
Production output remains uniform video and 480 audio sample pairs.

The longer 2,000-quantum probe reaches its 200,000,000-attempt bound without
a terminal CPU fault, with 534 nonterminal exception entries. SET_Z_IMAGE and
the actual color/depth clears now pass. The later graphics list blocks at
SET_TILE (`F510000007000000`, CURRENT `227128`, reason 1); the RSP blocks at
VCH (`4AFDE8E5`, SP PC `5F4`, status `C1`) after 215,369 instructions.
Production output stays uniform video with 93,572 audio sample pairs. Samples
do not establish audible game sound. **Retail boot and playability remain
unverified.**

The 8 MiB image has SHA256
`17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91`.
It is not included in this repository.

```bash
cmake --build build --target retail_core_probe n64_boot_trace n64_plugin
python3 tests/n64_retail_probe.py /path/to/your/sm64.z64
python3 tests/n64_retail_probe.py /path/to/your/sm64.z64 --frames 2000 --output build/artifacts/retail/n64-long.json
```

`--frames` selects 1–10,000 frontend scheduling quanta, not completed game
frames. The separate trace receives 100,000 nominal CPU attempts per quantum
and stops at its bound or the first terminal exception. The default stays 120.
The production ABI probe measures output separately. Reports include executable
and ROM hashes, requested budgets, CPU fault/fetch context, RSP state and DP
block diagnostics; no commercial bytes are archived or copied. A successful
probe process is not a playability verdict. Full CPU/CP0/FPU, native devices,
remaining RSP/RDP, VI filtering, pipeline scheduling, physical AI and save media
remain unfinished.


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

## FPU arithmetic family

The core implements ADD/SUB/MUL/DIV/SQRT/ABS/MOV/NEG in single and double
precision using portable integer significands, guard/sticky rounding, long
division and a restoring integer square root. It uses no native floating-point
state or compiler-specific 128-bit type. All four rounding modes, legacy NaNs,
signed zeros, infinity, invalid, division-by-zero, overflow and FS/enable-dependent
tiny results are handled. Assist takes precedence over invalid operands. MOV
copies bits without interpreting NaNs/subnormals or changing existing FCSR causes.
Traps update causes, preserve sticky flags and suppress destination writes.

The [pair ROM](../../tests/roms-src/n64/fpu_arithmetic_pairs/README.md) matches
**131,072 rows / 786,432 captured words**. The
[edge ROM](../../tests/roms-src/n64/fpu_arithmetic_edges/README.md) matches
**215,152 rows / 1,290,912 captured words**. Both stream six actual destination,
FCSR, handler, CPU exception and unchanged-source observations per row. An
independent exact-rational oracle grades arithmetic; square roots use integer
square comparisons, without native FP. Each ROM rejects an executed capture
mutant, six individual field mutations and seven malformed-stream controls.

The component passes **540,672 register/FR/format/function cases** and **327,680
rounding/enable/sticky/FS/condition combinations**, including aliased operands,
MOV preservation of enabled old causes, minimum-normal rounding rescue, CU1
precedence, reserved formats,
delay/nested/BEV exceptions and exact pending-arithmetic state replay. A separate
30,000-case seeded raw-bit differential check matches the rational oracle.
State stays `VN64S013`; this family adds no serialized fields.

Rules follow the [NEC manual](https://hack64.net/docs/VR43XX.pdf), chapters 7–8
and arithmetic instruction entries, supplemented by the hardware-test author's
[raw-bit arithmetic cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs).
These original cartridges have not been validated on a physical console. Their
finite domains do not exhaust all independent operand combinations. Undefined
odd FR=0 operands use the documented RI convention; nonzero unary ft fields use
an assist convention whose physical behavior is ungraded. FPU comparisons,
pipeline/interlock/CPI behavior, precise CE timing and reverse endian remain pending.
N64 stays **40% (8/20 delivery milestones)**; the whole CPU milestone is incomplete.

Super Mario 64 USA passes the former DIV.S blocker and reaches **12,000,000
startup attempts**, the bounded trace limit, without a terminal CPU fault.
It passed 51 nonterminal exception entries and ends at PC `80246DDC`, Status
`FF01`. The 120-quantum production probe observes uniform video and 480 audio
sample pairs; audible game sound, boot and playability remain unverified.
Commercial ROM bytes are never committed. Earlier increment reports retain
their historical counts and blockers.

Verification: **40/40 N64 CTests passed** in 246.85 seconds, including twenty-four original register-ROM gates, two production audio/video oracles and fourteen component gates. The full build and final arithmetic undefined-behavior sanitizer passed; all **73 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and Actions disabled.

## RSP scalar execution

The RSP now executes 32-bit scalar arithmetic, logic, comparisons, immediate and
variable shifts, conditional branches/link/jumps with delay slots, byte/halfword/
word DMEM loads/stores and shared MFC0/MTC0 SP fields. IMEM PC/targets wrap at
4 KiB; unaligned data accesses wrap each byte within DMEM. ADD/SUB/ADDI wrap
without CPU overflow traps. BREAK sets HALT/BROKE and the configured SP interrupt;
MI delivers that real interrupt to the CPU. Semaphore and settled DMA use the
same SP interface as CPU accesses. Single-step NOPs settle HALT.

RSP instructions advance at two per three nominal CPU interpreter attempts;
this is a functional scheduling approximation, not physical CPI, dual issue,
interlocks, bus timing or debugger pipeline behavior. Missing vector/DP operations
halt with an explicit blocked instruction and retained PC, without manufacturing
BROKE, a task-completion IRQ or a CPU hardware exception. Illegal/undefined
encodings and branches within delay slots are not physically graded.

The [ALU cartridge](../../tests/roms-src/n64/rsp_scalar_alu/README.md) matches
**704,512 rows / 4,931,584 actual captured words**, including all repeated-byte
pairs for ten functions and all fixed/variable shifts on 256 patterns. The
[immediate cartridge](../../tests/roms-src/n64/rsp_scalar_immediates/README.md) matches
**1,572,864 rows / 11,010,048 words**: every immediate for eight forms on three
operand patterns. Original RSP code writes actual GPRs to DMEM, and original CPU
code streams those values plus zero, SP PC/status and MI pending. A separate
mathematical host oracle grades every word. Each suite rejects an executed capture
mutant, seven field mutations and seven malformed-stream controls.

The component passes **4,194,304 immediate cases**, **327,680 register triples**,
**9,437,184 byte-address/pattern/load-store cases**, **262,144 branch/BREAK cases**,
all low-byte shift counts and all PC slots for J/JAL/JALR. It covers alias/zero
semantics, SP control/semaphore/real DMA bridges, CPU IRQ acknowledgment, explicit
unsupported-operation diagnostics, reset and state atomicity/replay.
The earlier SP-control cartridge now waits for single-step NOPs to settle and
halts before PC write/read to avoid execution races; all 196,608 rows still grade.

State `VN64S014` appends 304 bytes: 32 scalar GPRs, next PC, retired count, delay
state, blocked flag/word and the fractional clock phase. S013 and older states
preserve implemented fields while starting neutral scalar registers/phase and
sequential execution from the restored SP PC. Existing memory/debug prefix
positions stay stable. RSP state rejection is atomic, including invalid zero GPR,
word widths, alignment, flags and phase. Pending branch/BREAK replays exactly.

Rules follow the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
and the hardware-test author's [scalar cases](https://github.com/lemmy-64/n64-systemtest/tree/main/src/tests/rsp).
Published raw cases clarify SLTIU unsigned comparison, LWU as a word load,
unaligned DMEM wrapping, unconditional REGIMM links and BREAK in delay slots.
These original cartridges have not been validated on a physical console.
Selected domains do not exhaust all independent 32-bit operands or the whole RSP.

The local Mario trace reaches 12,000,000 CPU attempts with 51 nonterminal
exception entries and no terminal CPU fault. It retires **52 RSP scalar
instructions**, then blocks at **SP PC `090`, `MFC0 DPC_STATUS`, word `40055800`**.
Status `41` records HALT plus interrupt-on-break, without BROKE. The production
probe still observes uniform video and 480 audio sample pairs, which do not prove
audible game sound. Boot/gameplay remain unverified; commercial bytes are excluded.
N64 remains **40% (8/20 delivery milestones)**. Vector/RDP/game rendering, RSP
audio microcode, physical timing, FPU comparisons, analog and netplay remain pending.

Verification: **43/43 N64 CTests passed** in 325.15 seconds, including twenty-six original register-ROM gates, two production audio/video oracles and fifteen component gates. The corrected RSP ROM/component gates also passed in 78.34 seconds. The full build and final scalar undefined-behavior sanitizer passed; all **75 original ROMs** reproduce against the committed image/generator ledger. Completion renderer and whitespace checks pass. CI/CD remains removed and GitHub Actions disabled.

## Display-processor interface and fill engine

CPU MMIO `$04100000..1C` and RSP COP0 slots 8–15 now use one shared DP
module. START/END mask to aligned 24-bit pointers; START holds one pending
value and ignores repeats until END activates it. END updates CURRENT even
while frozen. The functional profile includes queued ranges, extensions,
XBUS DMEM wrapping, status controls and 24-bit nominal counters. It selects
zero for physically undefined reset pointers/counters; reset status is `$A8`.
Read-only writes and simultaneous clear/set pairs have deterministic conventions
and receive no physical-hardware credit.

The bounded RDP engine fetches big-endian 64-bit commands from DRAM or DMEM,
implements NOP, settled sync barriers, SetColorImage/SetScissor/SetOtherMode/
SetFillColor, integer fill-cycle RGBA16/32 rectangles, and actual FullSync.
Scissor ends are exclusive; fill rectangle ends are inclusive. RGBA16 alternates
the two halves of the 32-bit fill word by memory address. Bounds are validated
before any draw mutates RAM. FullSync raises MI DP bit 5 and settles the pipeline;
range-end alone never creates an IRQ. MI_INIT acknowledges the real level and
the existing mask/CPU IP2 path delivers it. CPU caches remain stale across RDP
writes until invalidated; the existing VI displays actual filled guest RAM.

The [register cartridge](../../tests/roms-src/n64/dp_registers/README.md)
matches **262,144 rows / 2,621,440 captured words**, using an original RSP
program to observe both control views. The [fill cartridge](../../tests/roms-src/n64/dp_fill_rectangles/README.md)
matches **524,288 rows / 6,815,744 captured words**, covering every 16-bit
color code in both pixel sizes, both command sources and whole/center scissor.
Independent integer oracles grade every captured pixel/guard/control word;
executed capture mutants fail all 32 reduced cases, each field mutation fails
individually and seven damaged stream controls return ERROR in each suite.
The component additionally checks all **16,777,216 START encodings**, **262,144
fill/scissor/guard cases**, shared CPU/RSP register banks, command flags/counters,
freeze/flush stalls, queued ranges, DMEM wrap, unsupported/bounds diagnostics,
cache/VI integration and CPU IRQ acknowledgment.

State `VN64S015` appends 19 validated big-endian scalars (152 bytes): DP latches,
active endpoint, status/counters, pipeline, command diagnostics and fill state.
Replay checks preserve pending commands, diagnostics and IRQs; invalid fields
are rejected atomically. S014 and earlier start reset DP controls/counters while
preserving all previously implemented fields, including the stored MI DP level.

Rules follow SGI's [DP register guide](https://hcs64.com/files/RSPCOP0.pdf),
[RDP command summary](https://ultra64.ca/files/documentation/silicon-graphics/SGI_RDP_Command_Summary.pdf),
and the hardware-test author's [current DP latch cases](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rdp/mod.rs).
These original cartridges have not been validated on a physical console.
Counters/issue use nominal RCP clocks, with no DMA FIFO/prefetch or physical
raster timing. Hidden coverage memory, TMEM/textures, triangles, blending, depth,
other cycle modes, fractional rectangles and interlace scissor remain unsupported.
Unknown commands or unsupported draws retain CURRENT and report the actual word
and a reason; they do not fabricate task completion or an interrupt.

The local Super Mario 64 (USA) probe on 2026-10-10 now passes the former
`MFC0 DPC_STATUS` blocker and retires **308 RSP scalar instructions**. It then
blocks at **SP PC `424`, `LQV`, word `C8411800`**. The CPU reaches 12,000,000
attempts with 51 nonterminal exception entries and no terminal CPU fault.
The production probe still observes uniform video and 480 audio sample pairs
across 120 scheduling quanta. Native boot, audible game sound and playability
remain unverified. The private image remains local. N64 stays **40% = 8/20**
delivery milestones; partial DP progress does not complete a whole hardware milestone.

Verification: **46/46 N64 gates passed** across the existing 43-gate regression
(322.32 seconds) and the three new DP gates (79.44 seconds). The final component
including VI scanout also passed. The full build and undefined-behavior sanitizer
passed; all **77 original ROMs** reproduce against the committed image/generator
ledger. Completion renderer and whitespace checks pass. CI/CD remains removed
and GitHub Actions disabled.

## RSP vector storage and memory

RSP scalar execution now owns 32 raw 128-bit vector registers and VCO/VCC/VCE
flags through a separate vector storage/transfer module. It implements all twelve
encoded load/store forms: byte/short/long/double, quad/rest, packed/fractional,
strided, whole and transpose. LWV follows the hardware-observed no-op behavior.
Loads truncate at the register end; stores wrap within the source register.
All memory bytes wrap within DMEM without touching IMEM. Seven-bit offsets are
sign-extended and scaled per opcode. Packed and transpose mappings follow the
hardware-test author's cases rather than treating every instruction as memcpy.

MTC2 discards the upper scalar halfword and truncates the last byte at element 15.
MFC2 reads two bytes with register wrap and sign-extends the halfword. CTC2/CFC2
share VCO/VCC/VCE storage, including all 32 selector aliases; VCE is eight bits,
so its readback never becomes negative. Memory transfers preserve all scalar
registers and flags. Unsupported arithmetic retains SP PC and the actual word,
sets HALT without BROKE, and never fabricates task completion or an interrupt.

Three original programs record actual guest observations, with no guest verdict:

| Original cartridge | Cases | Captured 64-bit words | Declared domain |
|---|---:|---:|---|
| [Vector loads](../../tests/roms-src/n64/rsp_vector_loads/README.md) | 786,432 | 19,660,800 | Twelve forms × 16 byte elements × 16 alignments × 256 byte-pattern seeds |
| [Vector stores](../../tests/roms-src/n64/rsp_vector_stores/README.md) | 786,432 | 25,952,256 | Same domain, including every byte of surrounding wrapped memory |
| [Scalar/vector transfers](../../tests/roms-src/n64/rsp_vector_transfers/README.md) | 1,048,576 | 10,485,760 | Every halfword payload × every byte element, with scalar zero and flag masks/signs |

For memory cartridges, seed parity selects `$900` or `$FF0`; VT is seed modulo
32. Results include the affected eight-register group, neighboring register
guards and actual scalar/SP/MI state. Each memory case uses three ordered REG
chunks within the existing debug line cap. Independent destination-oriented
integer byte mappings grade every field. Executed mutants must fail all reduced
cases; every field has a mutation check and seven damaged stream forms must
return ERROR in each suite. Artifacts are under `build/artifacts/owned/n64_rsp_vector_*/`.

The component additionally checks **3,145,728 memory/address/element cases**
covering every DMEM byte on two patterns, **3,145,728 signed-offset/register
cases** covering all seven-bit offsets and vector registers, **1,048,576
payload/element move pairs**, **2,097,152 flag alias cases**, and every GPR/vector
encoding at every byte element. Whole register, scalar, flag, DMEM and IMEM
guards are checked. State `VN64S016` appends 64 packed vector doublewords and
three validated flags (**536 bytes**). Pending-operation replay is exact;
invalid flags are rejected atomically; S015 and earlier restore neutral vector
storage while preserving previously implemented scalar/DP state.

Rules follow the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf),
and the hardware-test author's [loads](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_loads.rs),
[stores](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_stores.rs),
[moves](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_mfc2_mtc2.rs)
and [flag aliases](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_cfc2_ctc2.rs).
The original cartridges have not been validated on a physical console.
LFV element 1's first packed low bit follows the author's published mapping;
the published sequential-byte test cannot distinguish competing mappings there.
That bit on non-linear inputs receives no physical accuracy credit. Independent
full-width byte combinations, load interlocks, dual issue, physical DMA/pipeline
timing, vector arithmetic, accumulator/reciprocal state and complete graphics/
audio remain pending. These finite profiles do not establish full RSP accuracy.

The local Super Mario 64 (USA) probe on 2026-10-10 now passes the former LQV
blocker, retires **683 RSP instructions**, and blocks on **VMULF at SP PC `E5C`,
word `4BDFDEC0`**. The CPU reaches 12,000,000 attempts with 51 nonterminal
exception entries and no terminal CPU fault. Production output remains uniform
video and 480 audio sample pairs across 120 scheduling quanta. Native boot,
audible game sound and playability remain unverified. The private image remains
local. N64 stays **40% = 8/20 delivery milestones**.

Verification: **50 distinct N64 gates pass** after two harness corrections.
The initial 46-gate regression (394.74 seconds) found stale audio-test state
offsets; the corrected audio gate passed in 20.42 seconds. Vector loads passed
in 386.02 seconds, scalar/vector transfers in 114.31 seconds and the final
component in 3.08 seconds. The initial store run correctly rejected an incomplete
stream at the old execution cap; the full 786,432-case store corpus passed in
408.66 seconds with a larger budget, without reducing the domain. The final
build and undefined-behavior sanitizer passed; all **80 original ROMs** reproduce
from the committed image/generator ledger. Completion renderer and whitespace
checks pass. CI/CD remains removed and GitHub Actions disabled.

## RSP vector multiply and accumulate

The RSP vector module now implements all sixteen multiply-family encodings:
VMULF/U, VMULQ, VMUDL/M/N/H, VMACF/U/Q, VMADL/M/N/H and VRNDP/N.
Eight raw 48-bit accumulator lanes wrap modulo 2^48. Destination selection uses
signed or unsigned saturation, raw low/middle slices and quantized masks as
required by each instruction. VMUDM/VMADM multiply unsigned VS by signed VT;
VMUDN/VMADN multiply signed VS by unsigned VT. Source lanes are captured before
any destination writes, including aliases. All element selectors work, and
VRNDP/N use the VS register index parity for the addend position. VMACQ preserves
the low accumulator slice. These instructions preserve VCO/VCC/VCE.

VSAR reads accumulator high/middle/low with elements 8/9/10, writes zero for the
other elements and leaves the accumulator unchanged. Scalar/vector moves and
memory transfers preserve the accumulator. Other vector operations, including
VADD, still block explicitly at their PC without a fabricated BREAK or IRQ.

Two new original MIT programs emit actual guest register observations with no
guest verdict. Their independent host oracle uses unbounded integer arithmetic,
a literal lane-selection table and separate accumulator slices:

| Original cartridge | Cases | Captured 64-bit words | Declared domain |
|---|---:|---:|---|
| [Multiply pairs](../../tests/roms-src/n64/rsp_multiply_pairs/README.md) | 917,504 | 16,515,072 | Seven load-multiply functions × 16 selectors × 256 repeated-byte VS values × 32 groups of eight repeated-byte VT values |
| [Accumulate edges](../../tests/roms-src/n64/rsp_accumulate_edges/README.md) | 294,912 | 7,372,800 | Nine accumulate/round functions × 8,192 groups of eight raw halfword values × four partner/48-bit accumulator patterns; selectors distributed through the groups |

Each record captures result, all accumulator slices, source registers, flags
and SP PC/status/MI. The edge ROM additionally captures the accumulator before
the tested operation and scalar zero. Its six-instruction accumulator setup
uses VMUDH, four VMADH and VMADM and is independently graded. Ordered REG chunks
contain at most twelve words. Executed capture mutants must fail every reduced
case, all fields have individual mutation checks and seven incomplete/malformed
stream forms must return ERROR. Artifacts are under
`build/artifacts/owned/n64_rsp_multiply_pairs/` and `n64_rsp_accumulate_edges/`.

The component checks **16,777,216 raw-halfword/function/selector cases**,
**8,388,608 register/function/selector cases**, **524,288 VSAR encodings** and
all declared accumulator carry/overflow/sign/quantization boundaries. Whole
vector banks, flags, GPRs, DMEM and IMEM are guarded. Numeric published hardware
observations anchor thirteen multiply/accumulate instructions and even/odd VS
rounding. They caught and corrected the mixed-product signedness during review.

State `VN64S017` appends eight validated raw accumulator doublewords (**64
bytes**) after the S016 vector bank/flags. Pending multiply replay is exact;
nonzero upper 16 bits in any accumulator lane are rejected atomically. S016 and
earlier restore neutral accumulators while preserving previously implemented
fields. Plugin 0.18.0 continues ABI v2.

Sources are the SGI [RSP programmer guide](https://bukosek.si/hardware/collection/sgi-o2/n64-rsp-programmers-guide.pdf)
and the hardware-test author's [multiply](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmulf.rs),
[mixed product](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmudm.rs),
[accumulate](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmadn.rs),
[round](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vrndp.rs),
[quantization](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmacq.rs)
and [VSAR](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vsar.rs) tests.
These original ROMs have not run on a physical console. Their finite operand
patterns are not the exhaustive full-width pair corpus. Physical pipeline,
load interlocks/dual issue, remaining arithmetic/flags/reciprocal state, complete
RDP and RSP audio remain pending. The earlier LFV caveat remains ungraded.

Mario now passes VMULF and reaches a real RSP BREAK in the short probe. A longer
trace retires **145,423 RSP instructions** without an unsupported RSP operation,
then encounters unsupported CPU **C.LT.S** (`4608503C`, EPC `80248420`). The DP
also records unsupported **SET_COMBINE** (`FCFFFFFFFFFE793C`). Video remains
uniform; production output is 480 sample pairs at 120 quanta and 96,646 at
2,000 quanta. Native boot, audible game sound and playability stay unverified.
The private image stays local. N64 remains **40% = 8/20 delivery milestones**.

Verification: **53 distinct N64 gates pass** after one harness correction.
The 51-gate regression ran in 895.90 seconds: fifty passed and one still expected
VMULF to be unsupported. Its corrected VADD control passed in 0.42 seconds.
The two new original-ROM suites passed their full 917,504/294,912-case domains,
executed mutants, all 18/25 field mutations and seven stream controls each.
Final vector/multiply components, the full build and undefined-behavior
sanitizer pass. All **82 original ROMs** reproduce from the committed
image/generator ledger; completion renderer, probe budget validation and
whitespace checks pass. CI/CD remains removed and GitHub Actions disabled.

## FPU single and double comparisons

The CPU now executes all sixteen C.cond predicates in single and double format:
F/UN/EQ/UEQ/OLT/ULT/OLE/ULE and SF/NGLE/SEQ/NGL/LT/NGE/LE/NGT.
Integer raw encodings determine less/equal/greater/unordered without host floating
point. Signed zeros compare equal; infinities order normally. **Denormal operands
are compared directly**, with no arithmetic-style unimplemented assist and no
flush-to-zero approximation. The fraction MSB of a VR4300 NaN is set for a
signaling NaN, opposite to the newer IEEE convention. Quiet predicates signal
Invalid only for signaling NaNs; signaling predicates signal Invalid for any NaN.

Comparison replaces FCSR cause bits, commits the condition and sticky Invalid
flag only when the new cause is disabled, and leaves every FPR/GPR untouched.
An enabled Invalid trap preserves the old condition and flags. FPE now clears
stale CP0 Cause.CE, matching the published primary hardware observation. CU1 is
checked before numeric/encoding handling. Odd FR=0 sources retain the documented
RI convention; reserved formats and nonzero unused encoding bits use the E-assist
convention. Those undefined/reserved conventions are not physical accuracy claims.

Two original MIT cartridges capture actual guest register output with no guest
verdict. The independent host oracle uses exact rational values and separate
NaN classes and predicate truth masks:

| Original cartridge | Cases | Captured 64-bit words | Declared domain |
|---|---:|---:|---|
| [Comparison pairs](../../tests/roms-src/n64/fpu_comparison_pairs/README.md) | 262,144 | 1,835,008 | S/D × all sixteen predicates × all -32..31 integer pairs × both old condition bits |
| [Comparison edges](../../tests/roms-src/n64/fpu_comparison_edges/README.md) | 348,384 | 2,438,688 | Every exponent with cycling fraction patterns; special/sign pairs, full RM/enable/FS/condition products on ten profiles and reserved formats |

Each captures an untouched destination guard, actual FCSR, FPE indicator/code,
both source registers and branch-marker results for BC1F/T/FL/TL, including
actual delay-slot execution or annulment. Original code handles exceptions and
continues to the strict terminator. Executed mutants must fail every reduced
case; all seven fields have mutation controls and seven damaged/incomplete
stream forms must return ERROR. Artifacts:
`build/artifacts/owned/n64_fpu_comparison_pairs/` and `n64_fpu_comparison_edges/`.

The component checks **76,546,048 raw-fraction/exponent predicate cases**,
including every S/D exponent pair, **65,536 source/FR/predicate encodings** with
whole FPR/GPR guards, and **5,242,880 FCSR products**. It covers CU1 precedence,
reserved fields/formats, enabled/disabled exceptions, stale CE clearing,
BD/BEV/nested context and exact pending-comparison state replay. State remains
`VN64S017`; no new persistent fields are needed. Plugin 0.19.0 keeps ABI v2.

Rules follow the NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapters
7/8 and C.cond, and the hardware-test author's primary
[comparison tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/cop1/compares.rs)
and [raw NaN constants](https://github.com/thelemmy/nemu64-test/blob/main/src/cop1.rs).
The author's host-language quiet/signaling names are opposite to the VR4300
classification; raw bits and observed results determine this implementation.
These original ROMs have not run on a physical console. Finite operand patterns
do not exhaust every full-width pair. FPU physical timing/interlocks, complete
CP0/CPU and devices, remaining RSP/RDP and playable retail titles remain pending.

The local Mario probe now passes C.LT.S and reaches its **200,000,000-attempt
CPU trace limit without a terminal fault**. It retires **728,648 RSP
instructions** without an unsupported RSP operation. The RDP still blocks on
**SET_COMBINE** (`FCFFFFFFFFFE793C`, CURRENT `227018`, reason 1). Across 2,000
production scheduling quanta video stays uniform and 96,646 audio sample pairs
are observed. Native boot, audible game sound and playability remain unverified.
The private ROM stays local. N64 remains **40% = 8/20 delivery milestones**.

Verification: **56/56 N64 gates passed in one clean run** (700.37 seconds),
including both complete comparison corpora and their executed mutants, all seven
field controls and seven damaged stream controls each. The final expanded
fraction-pattern component and undefined-behavior sanitizer pass. The full build
passes; all **84 original ROMs** reproduce against the committed image/generator
ledger. Completion renderer and whitespace checks pass. Current executable hashes
match both recorded native probes. CI/CD remains removed and Actions disabled.


## RDP combiner command state

SET_COMBINE (0x3C), SET_PRIM_COLOR (0x3A) and SET_ENV_COLOR (0x3B)
now update persistent RDP command state. Both RGB/alpha selector banks are
independently decoded. Primitive RGBA, minimum LOD and fractional LOD, and
all environment RGBA bits are retained. These commands advance CURRENT without
asserting DP IRQ. FullSync remains the actual IRQ producer. Source layouts are
[SGI RDP Command Summary, tables 19, 21 and 22](https://hcs64.com/files/RDP_COMMANDS.pdf).
Discarding unused color/primitive bits is a deterministic convention; physical
reserved-bit behavior is ungraded.

This increment stores and decodes attributes. **Color-combiner arithmetic,
blending, texture sampling, triangle rasterization and depth operations remain
pending.** Existing bounded fill-cycle rectangles continue using FILL_COLOR.
Unsupported draws retain CURRENT and a real diagnostic, without fabricated
pixels or completion interrupts.

The original `rdp_combiner_commands` ROM grades **131,072 cases / 917,504
actual 64-bit words** over all repeated 16-bit payloads and both RDRAM and
wrapping DMEM lists. CURRENT, STATUS, MI IRQ and a RAM guard are captured before
and after a separately extended FullSync. This proves command progression and
IRQ behavior; internal attributes have no hardware readback in this ROM.
The component independently checks **43,264 selector pairs** and **524,288
command streams**, every reserved input bit, LOD/color retention, unsupported
draw failure, frozen pending-list replay and atomic invalid-state rejection.
An executed capture mutant, all seven field mutations and seven malformed
stream controls must fail. The component also passes undefined-behavior
sanitization. See the original corpus README for the exact finite scope.

State **VN64S018** appends three validated 64-bit attribute slots (**24 bytes**).
S017 and all previously accepted older versions restore neutral attributes and
preserve their earlier fields. Plugin **0.20.0** retains ABI v2. Earlier state
component offsets move by the appended 24 bytes.

The current private Super Mario 64 (USA) probe passes the former SET_COMBINE
blocker. At 2,000 scheduling quanta the CPU reaches **200,000,000 attempts**,
with 444 nonterminal exception entries and no terminal fault; the RSP retires
**728,648 instructions**, without an unsupported operation, and reaches real
BREAK (SP PC 0x144, status 0x243). The DP next blocks at **SET_Z_IMAGE**,
word **FE00000000000400**, CURRENT **0x227090**, reason 1. Production output
remains uniform video and **96,646 audio sample pairs**. Sample counts do not
prove audible game sound. Boot/playability remain unverified; no commercial
bytes are committed. The default 120-quantum probe remains bounded at
12,000,000 CPU attempts, 6,026 RSP instructions and 480 audio sample pairs.

Delivery completion stays **40% (8/20 milestones)**. This command-state
increment does not complete the CPU, RDP or native graphics milestone. The
front-page and core percentages remain synchronized with the fixed ledger.

Verification: **58 distinct N64 gates have passing results**. The full batch
passed 56 gates; its AI assertion used an old state-tail offset, and the new
ROM builder used OR instead of addition for one input address. Both were
corrected and their targeted gates pass, including the complete original-ROM
domain and negative controls. All **85 original ROM archives** reproduce;
completion synchronization and whitespace checks pass. CI/CD remains disabled.


## RDP fill pipeline and depth clears

The fill-cycle engine now accepts bypassed texture, combiner, blender and dither
mode state, including the native clearing path, and writes actual 8-, 16- and
32-bit framebuffer bytes. Fill pattern selection follows the absolute RAM
address. SET_Z_IMAGE stores its 26-bit address; clearing depth redirects the
color image to depth RAM, as documented by Nintendo, rather than performing
depth compares. The original corpus clears separate color and depth buffers.

Fractional rectangle/scissor bounds, field-even/field-odd selection and odd row
strides are supported. Width is a memory stride: X spans can extend beyond it
and overlap the next row. The detailed span model includes the truncated high
X scissor pixel, while Y clipping tests quarter scanlines before writing a row.
This corrects the older inset-scissor oracle, whose high X endpoint was excluded.
The original archived integer fill cartridge is unchanged; its oracle now
matches the expanded span model.

Command fields and depth clearing follow the [SGI RDP command summary](https://hcs64.com/files/RDP_COMMANDS.pdf)
and [Nintendo fill-cycle documentation](https://ultra64.ca/files/documentation/online-manuals/man/pro-man/pro12/12-01.html).
The quarter-span and hazard details are inferred from the RDP author's primary
[edge-walker implementation](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/rasterizer.c).
Nintendo gives a broad four-pixel scissor warning; these gates characterize the
author's detailed span model, **not physical subpixel/scissor accuracy**.
Image reads, depth compares and stepped depth-update modes are explicit blockers.
Hardware crash timing and partial crash writes are ungraded; the functional
engine rejects these profiles before any write. Four-bit fills are blocked.

The new original `rdp_fill_pipeline` corpus enumerates **294,912 input patterns**
and grades **589,824 rows / 6,488,064 actual 64-bit words**. All 12,288 safe low
16-bit modes, CI8/RGBA16/RGBA32, RDRAM/wrapping DMEM and four geometry profiles
are covered. Two ordered 11-word rows per pattern contain color/depth pixels,
three guards and actual CURRENT/STATUS/MI state. An independently executed
capture mutant, 22 field mutations and seven malformed streams must fail.
The two rows fit the result channel's 255-byte line limit.

The component passes **1,572,864 fractional/scissor/field/stride cases**,
**196,608 mode cases**, **196,608 raw fill-code cases**, all **1,024 widths**
and all **67,108,864 depth-image address values**, plus guards, selected-row
bounds, actual depth clears, FullSync IRQ, frozen-list replay, rejected state
fields and legacy conversion. Address enumeration proves command retention;
it is not a depth-comparison or physical-register readback test. The component
also passes undefined-behavior sanitization.

State **VN64S019** appends a validated 8-byte depth-image scalar. S018 and
previously accepted older states restore a neutral depth pointer and preserve
previous fields. Plugin **0.21.0** retains ABI v2. Earlier component state-tail
offsets move by eight bytes. Hidden coverage/depth bits, exact bus/DMA/raster
timing, texture loads/sampling, triangles, blending, combiner arithmetic and
actual depth comparisons remain missing or ungraded. Reserved image formats
and input bits receive no accuracy credit.

The private Mario probe now passes SET_Z_IMAGE and the real buffer clears.
At **2,000 scheduling quanta**, the CPU reaches **200,000,000 attempts** with
**534 nonterminal exception entries** and no terminal fault. The later graphics
list blocks at **SET_TILE**, word **F510000007000000**, CURRENT **227128**,
reason 1. The RSP blocks at **VCH**, word **4AFDE8E5**, SP PC **5F4**, status
**C1**, after **215,369 instructions**. Production video remains uniform with
**93,572 audio sample pairs**; sample counts do not prove audible game sound.
The default 120-quantum probe remains at 12,000,000 CPU attempts, 6,026 RSP
instructions and 480 sample pairs. **Retail boot/playability remain unverified.**
No commercial bytes are committed.

Delivery completion remains **40% (8/20 milestones)**. The complete graphics
milestone and the overall Nintendo-platform goal remain unfinished. Root and
core README percentages are synchronized from the fixed completion ledger.
CI/CD remains disabled.

Verification: the full build and **60/60 N64 gates pass** in one regression
run (**676.76 seconds**). All **86 original ROM archives** reproduce. The new
component passes undefined-behavior sanitization, both private retail reports
match current executable hashes, and completion/whitespace checks pass.

## RSP vector comparison and clipping

The RSP now executes **VLT, VEQ, VNE, VGE, VCL, VCH, VCR and VMRG**.
All 16 element encodings select VT lanes; inputs are captured before destination
writes so overlapping registers work. VCO, VCC and VCE follow each instruction's
rules. The destination also replaces accumulator LOW16 while retaining its
upper 32 bits. VCH's zero/-1 boundaries feed VCL and VMRG. Remaining arithmetic,
logic, reciprocal operations and physical pipeline/interlock timing are pending.
Unsupported instructions still halt with an explicit diagnostic and no fake
BREAK or interrupt.

The functional rules follow the hardware-test author's primary
[nemu64-test comparison tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_arithmetic.rs).
The new component independently checks **134,217,728 lane cases**: every 16-bit
VS value, eight boundary/relationship VT partners, all 32 incoming lane-flag
combinations and all eight functions. Another **81,920 actual vector cases**
cover all selectors and register positions, five alias patterns, nonuniform
flags, untouched registers, accumulator upper slices and scalar/DMEM guards.
Clipping chains, real dispatch/BREAK IRQ, pending-state replay and unsupported
VADD behavior are checked. These finite domains do not prove all full-width
operand pairs or physical hardware timing.

| Original ROM | Cases | Actual 64-bit captures | Domain |
|---|---:|---:|---|
| [Comparison pairs](../../tests/roms-src/n64/rsp_comparison_pairs/README.md) | 262,144 | 4,718,592 | 256 repeated-byte VS values × 32 groups of eight repeated-byte VT lanes × four incoming flag profiles × eight functions |
| [Comparison edges](../../tests/roms-src/n64/rsp_comparison_edges/README.md) | 262,144 | 4,718,592 | 8,192 groups of consecutive VS halfwords × boundary VT lanes × four incoming flag profiles × eight functions |

Selectors rotate with input/function/profile; the original corpora do not cover
every selector/flag for every operand. Each actual RSP sequence seeds the
accumulator with VMUDH, executes the comparison, and captures destination,
all accumulator slices, both sources, VCO/VCC/VCE and SP PC/status/MI. The guest
computes no verdict. Independent whole-integer grading checks all 18 fields.
An executed capture mutant, eighteen individual field mutations and seven
malformed streams are required to fail. These original cartridges have not run
on a physical console. Artifacts are under
`build/artifacts/owned/n64_rsp_comparison_pairs/` and `n64_rsp_comparison_edges/`.

Plugin **0.22.0**, ABI v2, retains **VN64S019**: its existing vector flags and
48-bit accumulator contain all comparison state, so no state-layout change is
needed.

The private Super Mario 64 probe now passes **VCH** and advances twelve more
RSP instructions. At 2,000 scheduling quanta, the CPU reaches **200,000,000
attempts**, with **534 nonterminal exception entries** and no terminal fault.
The next RSP blocker is **VADD**, word **4B1FED50**, SP PC **624**, status **C1**,
after **215,381 instructions**. RDP **SET_TILE** remains blocked at word
**F510000007000000**, CURRENT **227128**, reason 1. Video remains uniform;
**93,572 audio sample pairs** do not prove audible game sound. The default
120-quantum probe retains its 12,000,000-attempt bound and 6,026 RSP instructions.
Scheduling quanta are not game frames. **Native boot/playability are unverified.**
The commercial ROM remains local and is not archived.

Delivery completion stays **40% (8/20 milestones)**. The full graphics milestone
and the overall Nintendo-platform goal remain unfinished. README percentages
remain synchronized from the fixed ledger. CI/CD remains disabled.

Verification: the full build and **63 distinct N64 gates pass**. The two new
original corpus gates pass together in **80.43 seconds**; the remaining **61/61
regression gates** pass in **739.91 seconds**, including the comparison component.
All **88 original ROM archives** reproduce. The comparison component passes
undefined-behavior sanitization. Both private retail reports match current
executable hashes, and completion/whitespace checks pass.

## RSP vector integer arithmetic and logic

The RSP now executes **VADD, VSUB, VABS, VADDC, VSUBC, VAND, VNAND, VOR,
VNOR, VXOR and VNXOR**. All element encodings and source/destination overlaps
use captured input lanes. Arithmetic obeys signed saturation, the sign-selected
VABS special case, unsigned carry/borrow and nonzero flags. VADD/VSUB consume
VCO-low and clear VCO; VADDC/VSUBC produce carry/borrow flags. VABS and Boolean
operations retain all comparison flags. VCC/VCE are preserved throughout these
operations. Accumulator LOW16 receives the unclamped arithmetic residue, which
can differ from the saturated destination; the upper 32 bits are retained.

Rules follow the hardware-test author's primary
[nemu64-test arithmetic/Boolean tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vector_arithmetic.rs)
and [opcode encodings](https://github.com/thelemmy/nemu64-test/blob/main/src/rsp/rsp_assembler.rs).
The component independently grades **184,549,376 lane cases**: every 16-bit
VS value, eight boundary/relationship VT partners, all 32 incoming lane-flag
combinations and all eleven functions. It also checks **112,640 actual vector
cases** across all selectors, 32 register positions, five alias patterns and
nonuniform flags. Whole-integer residues and per-bit Boolean truth tables are
independent of the production lane helper. Literal carry/borrow producer-consumer
chains, VABS -32768 saturation, untouched registers/scalar/DMEM, actual dispatch,
BREAK IRQ and exact pending-instruction save replay are covered.

| Original ROM | Cases | Actual 64-bit captures | Domain |
|---|---:|---:|---|
| [Integer pairs](../../tests/roms-src/n64/rsp_integer_pairs/README.md) | 2,883,584 | 51,904,512 | Every repeated-byte operand pair × all 32 incoming lane flags × eleven functions; identity selection |
| [Integer edges](../../tests/roms-src/n64/rsp_integer_edges/README.md) | 2,883,584 | 51,904,512 | Every VS halfword × all 32 incoming lane flags × eleven functions; rotating selectors and selected boundary VT lanes |

Each real RSP sequence seeds the accumulator with VMUDH, executes the operation,
and records destination, every accumulator slice, both sources, VCO/VCC/VCE,
SP PC/status and MI. The guest computes no verdict. Independent whole-integer
and per-bit grading checks all eighteen fields. The flag loop runs in guest
code, keeping each original archive below 1 MiB. An executed capture mutant,
eighteen field mutations and seven malformed streams must fail. The edge
corpus collectively covers selectors; it does not enumerate every full-width
operand pair, selector and VT partner. These originals have not run on a physical
console. Their artifacts are under `build/artifacts/owned/n64_rsp_integer_pairs/`
and `n64_rsp_integer_edges/`.

Plugin **0.23.0**, ABI v2, retains **VN64S019**. Existing vector flags and
48-bit accumulators contain all integer-ALU state. Unsupported reciprocal,
square-root, move and rare vector operations still block with an explicit
word/PC diagnostic, without a fake BREAK/interrupt. Physical pipeline timing
and interlocks remain ungraded. Existing unsupported-instruction controls now
use the genuinely unsupported VRCP encoding instead of implemented VADD.

The private Mario probe now passes **VADD** and advances another twelve RSP
instructions to **VRCPH**, word **4B7D5EF2**, SP PC **0**, status **C1**, after
**215,393 instructions**. At 2,000 scheduling quanta the CPU reaches **200,000,000
attempts**, with **534 nonterminal exception entries** and no terminal fault.
RDP **SET_TILE** remains blocked at **F510000007000000**, CURRENT **227128**,
reason 1. Video remains uniform and **93,572 audio sample pairs** do not establish
audible game sound. Scheduling quanta are not game frames. **Native boot and
playability remain unverified**; commercial ROM bytes remain local.

N64 stays **40% = 8/20 fixed delivery milestones**. The complete graphics
milestone and the full Nintendo-platform goal remain unfinished. Front-page
and core README percentages are synchronized. CI/CD remains disabled.

Verification: the full build and **66 distinct N64 gates pass**. Both new
original corpora pass in **759.33 seconds**, grading **5,767,168 cases /
103,809,024 actual 64-bit words** with zero mismatches and rejecting the executed
capture mutants, eighteen field mutations and seven malformed streams per suite.
The remaining **64/64 N64 regression gates** pass in **1,242.24 seconds**,
including the new integer-ALU component. All **90 original ROM archives**
reproduce. The component passes undefined-behavior sanitization, both private
retail reports match current executable hashes, and completion/whitespace
checks pass.

## RSP reciprocal, square-root and move

The RSP now executes **VRCP, VRCPL, VRCPH, VMOV, VRSQ, VRSQL and VRSQH**,
plus the **VNOP/VNULL** no-ops. Reciprocal and square-root share a high input,
a 32-bit output and an explicit pending flag. High instructions return the
previous output's high half and prime the input; low instructions consume it
once, even if its high half is zero. Ordinary reciprocal/square-root operations
clear the pending flag. VMOV and no-ops leave divider state unchanged.

Only the destination lane selected by the low three VS-field bits is written.
Reciprocal/square-root source selection uses raw `e & 7`; VMOV uses the ordinary
element map at the destination lane. All seven replace accumulator LOW16 with
selected VT inputs and retain upper slices and VCO/VCC/VCE. The result can differ
from accumulator LOW16. Inputs are captured before aliases are written. No-ops
retain the entire vector/accumulator/flag/divider state.

The approximation and shared-latch rules follow the hardware-test author's
primary [reciprocal/square-root/move tests](https://github.com/thelemmy/nemu64-test/blob/main/src/tests/rsp/op_vmov_vrcp.rs),
including published E834 high/low expectations. Production tables are independently
derived with rational quantization and a binary integer-inequality search;
there is no host floating point or copied emulator implementation. The independent
component uses exponent/residue algebra and a restoring integer-square-root
algorithm. It checks all **1,024 table entries**, **393,216 value cases**
(including every signed halfword and exponent/mantissa/sign boundaries),
**2,097,152 cross-family high/low chains**, and **917,504 actual vector cases**
across register positions, every VS encoding, selectors, aliases and flag profiles.
Zero-high pending state, one-time consumption, published numeric expectations,
no-op guards, exact save replay and atomic rejection are checked. These domains
do not enumerate every full-width input or physical interlock.

The original [divider/move ROM](../../tests/roms-src/n64/rsp_divider/README.md)
enumerates **5,898,240 cases / 123,863,040 actual 64-bit captures**. Every halfword
is swept over nine instructions and ten profiles: unprimed/primed low inputs,
and every high half combined with eight boundary low halves. Source VT values
are broadcast; selector/destination encodings rotate. Twenty-one fields contain
vectors/accumulators/sources, flags, SP/MI, the full output high half and a replay
with low/high probes exposing pending behavior. The guest computes no verdict.
Independent rational/Python integer-square-root/latch grading checks each field.
An executed capture mutant, 21 field mutations and seven malformed streams must
fail. This original has not run on a physical console. Artifacts:
`build/artifacts/owned/n64_rsp_divider/`.

Plugin **0.24.0**, ABI v2, introduces **VN64S020**. An appended eight-byte word
stores input[63:48], output[47:16] and pending[0]; bits[15:1] must be zero.
S019 and previously accepted older states restore neutral divider state and
preserve previous fields. Existing EOF-relative component state checks move
eight bytes. Reserved-bit rejection is atomic; a pending zero-high VRCPL resumes
exactly after restore. Rare vector operations such as VSUT still block explicitly
without a fake BREAK or interrupt. Physical RSP timing/interlocks and full
RDP/audio/hardware compatibility remain unfinished or ungraded.

The private Mario probe now passes **VRCPH** and reports **no unsupported RSP
instruction** at the 2,000-quantum bound. It executes **3,497,074 RSP instructions**;
the last task reaches a real **BREAK**, SP PC **144**, status **243**. The CPU
reaches **200,000,000 attempts**, with **620 nonterminal exception entries** and
no terminal fault. RDP **SET_TILE** remains blocked at **F510000007000000**,
CURRENT **227128**, reason 1; END advances to **227C30**. Video is still uniform;
**93,571 audio sample pairs** do not prove audible game sound. Quanta are not
game frames. **Native boot/playability remain unverified.** Commercial bytes
stay local. This trace does not establish the full RSP hardware scope.

N64 remains **40% = 8/20 fixed delivery milestones**. The complete graphics
milestone and full Nintendo-platform goal remain unfinished. Front-page and
core README percentages remain synchronized. CI/CD stays disabled.

Verification: the full build and **68 distinct N64 gates pass**. The new original
suite passes in **1,657.36 seconds**, independently grading **5,898,240 cases /
123,863,040 actual 64-bit words** with zero mismatches and rejecting all executed,
21-field and seven malformed-stream controls. **24/24 component gates** pass in
**50.48 seconds**. The remaining ROM batch passes 42 of 43 in **1,285.47 seconds**;
the video capture helper needed its EOF-relative VI offset moved eight bytes for
S020, and the corrected gate passes in **4.78 seconds**, completing all 43.
All **91 original ROM archives** reproduce. Undefined-behavior sanitization,
current executable hashes for both private retail reports, completion synchronization
and whitespace checks pass.

## RDP tile and texture-image state

The RDP executes **SET_TILE (0x35), SET_TILE_SIZE (0x32) and
SET_TEXTURE_IMAGE (0x3D)** and retains all documented fields. Eight independent
tile descriptors contain format, texel size, line/TMEM word addresses, palette,
S/T clamp/mirror/mask/shift; separate bounds retain raw unsigned 10.2 SL/TL/SH/TH.
The texture image retains format/size, width-minus-one and its 26-bit byte address.
Descriptor updates preserve bounds and other tiles; size updates preserve descriptors.
The image width accessor adds one. This state is available to subsequent texture
operations. There is no fabricated TMEM transfer or textured output.

Layout follows [SGI RDP Command Summary, tables 3, 6 and 9](https://hcs64.com/files/RDP_COMMANDS.pdf).
Reserved command bits are discarded as an explicit deterministic convention;
physical reserved-bit behavior is ungraded. Command-state progression from
RDRAM and XBUS DMEM uses the shared DP interface, including wrap/freeze/END
extension. Only real FullSync asserts DP IRQ. LOAD_BLOCK, LOAD_TILE, LOAD_TLUT,
texture sampling/filtering/coordinates, rasterization and exact timing remain pending.

The [original tile command ROM](../../tests/roms-src/n64/rdp_tile_commands/README.md)
provides **131,072 cases / 917,504 actual register/RAM words** covering every
16-bit repeated payload through both sources, with all eight tile indices rotating.
It captures CURRENT/STATUS/MI before and after FullSync and an untouched RAM guard.
It computes no verdict. The independent host grader checks the exact finite domain
and rejects an executed capture mutant, seven field mutations and seven malformed
streams. **There is no hardware readback of internal tile/image state in this ROM.**
The component independently grades **10,107,008 descriptor-field pairs**, all encoded
field values at all tile indices, **1,048,576 source/tile state streams**, every
input bit, 131,072 isolated coordinate-field cases, raw bounds and image fields, reset and real unsupported-load blocking.
This is neither the full payload/address corpus nor physical-console verification.

Plugin **0.25.0**, ABI v2, saves **VN64S021**. The appended **136 bytes / 17 words**
store descriptor/bounds per tile then texture image; reserved bits must be zero.
Restore validates every word before updating the machine. S020 and previously
accepted older formats restore neutral texture state and preserve previous fields;
the S020 conversion specifically preserves a pending zero-high divider input and
nonzero output. Frozen pending texture commands replay exactly. Existing EOF-relative
state checks and the ABI video capture move 136 bytes.

The private Mario probe passes the prior **SET_TILE** blocker. At **2,000 scheduling
quanta / 200,000,000 CPU attempts**, the next block is **LOAD_BLOCK**, word
**F3000000073FF100**, CURRENT **227158**, reason 1, END **227C30**. There are **620
nonterminal exception entries**, no terminal fault, **3,497,074 RSP instructions**,
no unsupported RSP at this bound, and a real BREAK (SP PC **0x144**, status **0x243**).
Video remains uniform. **93,571 audio sample pairs** do not prove game sound.
Quanta are not game frames. **Native boot/playability remain unverified.** Commercial
ROM bytes remain local. Repeatable reports are in `build/artifacts/retail/n64-tile.json`
and the default `build/artifacts/retail/n64.json`.

N64 remains **40% = 8/20 fixed delivery milestones**; the complete graphics
milestone and full Nintendo-platform goal remain unfinished. Front-page and core
README percentages stay synchronized. CI/CD remains disabled.

Verification: the full build and **70 distinct N64 gates pass**. The full batch
passes 69 of 70 in **1,803.04 seconds**. The AI component's symbolic EOF offset
needed the same 136-byte shift as other state checks; with both rejection and
legacy-conversion offsets corrected, that gate passes in **17.65 seconds**.
Production code is unchanged by this correction. The new original tile suite
passes in **10.57 seconds**, grading **131,072 cases / 917,504 actual words** with
zero mismatches and rejecting all 16 executed capture mutants, seven field
mutations and seven malformed streams. The divider regression also passes in
**1,599.30 seconds**. All **92 original ROM archives** reproduce; the final
texture component passes undefined-behavior sanitization without compiler warnings.
Both private retail reports match current executable hashes. Completion synchronization,
new evidence links and whitespace checks pass.

## RDP texture-memory transfers

The RDP now executes **LOAD_BLOCK (0x33), LOAD_TILE (0x34) and LOAD_TLUT (0x30)**
into real **4 KiB TMEM**, stored as canonical big-endian bytes. Block transfers
use integer S/T and 1.11 DxT; tile/palette commands use raw 10.2 coordinates.
Loads update the selected tile's bounds while retaining descriptors and neighbors.
Full eight-byte groups include padding beyond a partial final span. DxT controls
odd-row longword swapping and line strides. TMEM addresses wrap; RGBA32 and YUV
split data between banks. Even-address palette fetches replicate a halfword;
odd addresses fetch a full word. Source size controls source/coordinate advance.
Loads preserve signed 16-bit coordinate wrapping and fractional edge behavior. Source reads
use the 24-bit RDP memory interface and zero for absent backing. These rules are
implemented with integer addressing, with no host floating point.

Field and transfer descriptions come from the [SGI RDP Command Summary](https://hcs64.com/files/RDP_COMMANDS.pdf).
Bank/partial-transfer rules were checked against the reference author's
[texture loads](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/tex.c),
[TMEM indexing](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/tmem.c)
and memory-interface source. An optional comparison against commit
`9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc` passes **168,192 full-TMEM
comparisons** across explicit load, coordinate and multirow partner domains.
The [repeatable reference harness](../../tests/reference/n64-tmem/README.md) records the exact domains.
This is software-reference evidence, **not physical-console verification**.
The independent component uses inverse bank-address algebra and byte scatter,
plus literal normal/odd rows, RGBA32 halves, palette alignment, fractional
edges and signed coordinate wrapping. No third-party implementation is vendored.

The component grades **665,884 byte cases**: every TMEM word, all source alignments,
source sizes 8/16/32, all encoded descriptor formats/sizes, all 4,096 DxT values,
counts 1..4,095, coordinate encodings, neighboring stride/base partners,
65,536 halfword-value patterns, and fractional/multirow tile and palette profiles.
These are explicit sweeps and partner products, **not every Cartesian product**
or all source addresses. The entire 4 KiB result is compared, including untouched
bytes. Load progression from RDRAM/XBUS, DMEM command wrapping, END extension,
nominal transfer-group counters, real FullSync IRQ and no RAM writes are checked.
Four-bit source images and multirow palette loads are blocked before writes with
reason 3. Physical crash side effects and timing remain ungraded; this atomic
blocking is a diagnostic convention. Byte sampling, palette lookup, filtering,
coordinate processing for draws, triangles, blending/depth and VI filtering remain
pending. TMEM presence does not establish rendered texture correctness.

The [original transfer-command ROM](../../tests/roms-src/n64/rdp_tmem_commands/README.md)
passes **393,216 cases / 2,752,512 actual 64-bit register/RAM captures** in
**58.73 seconds**, with zero mismatches and all 48 executed capture mutants,
seven field mutations and seven malformed streams rejected. It checks consumption,
FullSync IRQ and an untouched RAM guard for all three loads, through both sources.
**Internal TMEM bytes have no CPU readback in this ROM** and are verified separately
by the component. Counts are four texels/entries with zero origins; its geometry
is deliberately stated rather than described as the full hardware corpus.

Plugin **0.26.0**, ABI v2, saves **VN64S022**. The appended **4,096 raw bytes**
store all TMEM contents; all byte values are legal. S021 and previously accepted
older states restore zero TMEM and preserve preceding fields. Pending frozen loads
replay exactly; S021 conversion preserves descriptors, bounds, image and a pending
zero-high divider with nonzero output. Invalid state lengths reject atomically.
Reset clears TMEM. Existing EOF-relative state/capture checks move 4,096 bytes,
including the AI test's symbolic offset. TMEM_BUSY counts settled transfer groups
in this profile; it is not a measurement of physical pipeline clocks.

The private Mario probe passes **LOAD_BLOCK**. At **2,000 scheduling quanta /
200,000,000 CPU attempts**, it next blocks on **shaded/textured/depth-tested
triangle opcode 0x0F**, word **CF000FFA0FFAF006**, CURRENT **227160**, reason 1,
END **227C30**. TMEM's nominal counter is **0x100 groups**. There are **620
nonterminal exception entries**, no terminal fault, **3,497,074 RSP instructions**,
no unsupported RSP at this bound and a real BREAK (SP PC **0x144**, status **0x243**).
Video remains uniform. **93,571 audio sample pairs** do not prove game sound.
Quanta are not game frames. **Native boot/playability remain unverified.** Commercial
bytes remain local. Reports: `build/artifacts/retail/n64-tmem.json` and default
`build/artifacts/retail/n64.json`.

N64 remains **40% = 8/20 fixed delivery milestones**. Full graphics and the full
Nintendo-platform goal remain unfinished. Front-page and core README percentages
stay synchronized. CI/CD remains disabled. **72 distinct N64 gates pass**: the 70 existing regressions passed in
**2,149.11 seconds**, and both TMEM gates passed after the reference-driven
corrections. The full build and undefined-behavior sanitizer pass; all 93
original archives reproduce. This verifies the stated implemented profiles,
not the full native hardware or playable retail games.

## RDP scalar combiner arithmetic

The [internal arithmetic kernel](COMBINER-MATH.md) now implements special nine-bit
A/B/D extension, signed C multiplication, the 128 rounding bias, 17-bit RGB
residue, nine-bit alpha and the RDP clamp. **262,144 A/B pairs**, **268,173,312
reduced difference/C/D cases** and **512 clamp encodings** pass the independent
integer component. The exact finite reduction and limits are documented.
The reduced RGB/alpha cases and clamp values also pass the actual routines in
pinned Angrylion RDP Plus; the repeatable optional harness is linked there.
Undefined-behavior sanitizer and existing command-state/replay checks pass.

This is a kernel for the pending draw pipeline. Guest triangles remain blocked;
source routing, one-/two-cycle integration, keying, coverage/alpha coupling,
dithering, texture sampling, blending/depth and rasterization remain unfinished.
The original command ROM grades command progression, not this arithmetic.
Physical-console validation and hardware timing remain unverified. Native Mario
boot/playability remain unverified. ABI v2/plugin 0.26.0 and S022 are unchanged;
N64 remains **40% = 8/20 fixed delivery milestones**. CI/CD remains disabled.

**73 distinct N64 gates pass cumulatively**: the 72 verified S022/TMEM gates
plus the scalar arithmetic gate. The production N64 plugin built from this
branch is byte-identical to the TMEM build, confirming no change to existing
guest execution or state layout. This kernel is not yet wired to guest draws.

## Four digital ports and production netplay

Plugin **0.27.0**, emulator ABI v2, now exposes `INetplayCapable` for four
virtual digital PIF ports. The production default network plugin owns two
participants: host input reaches port 0, client input reaches port 1, and ports
2/3 remain neutral. The N-player core API routes up to four virtual masks;
this does not supply a four-peer network transport or local multi-pad frontend.
Normal local execution still supplies port 0. All four pads are permanently
present in this profile, with zero stick bytes and no accessories.

The [original guest](../../tests/roms-src/n64/controller_netplay/README.md)
uses actual SI DMA and reads four replies, completion status and acknowledged
status. **16,384 rows / 98,304 words** pass a separate bit-assignment oracle;
all four inputs receive different permutations of the digital domain. The
executed wrong-capture control fails all 16 rows; six field mutations and seven
malformed streams reject. **65,536 isolated component cases** cover every
port and digital mask; sanitizer checks pass. The existing single-input original
ROM is unchanged; its oracle now expects a neutral second pad. Absent-channel
component coverage uses channel 4, outside the four controller ports.

The production TCP test passes **100 scheduling quanta** in two independent
processes. Host, client and baseline match **600 actual guest words** and the
complete state. Both peers perform rollback with real AI replay callbacks.
The wrong-capture guest is rejected with exactly 100 mismatched words; six field
mutations, four malformed result streams, four fragmented invalid network
packets and ROM mismatch are rejected. Different pre-session histories reload
fresh; core replacement and external execution disconnect. Reports and raw
readbacks are in `build/artifacts/netplay-n64/`. Internet, two-machine and
Windows networking are unverified. Quanta are not measured VI game frames.

State **VN64S023** appends three big-endian eight-byte digital masks to S022,
for **8,478,744 bytes**. Reserved upper halves reject atomically. Older accepted
states restore the additional ports to neutral and preserve earlier fields.
Fast save queries the exact bound, rejects undersized buffers without writes,
and hashes the complete serialized state. Netplay drains output PCM each
quantum with or without a callback, keeping muted and streamed states equal;
normal local polling retains its existing behavior. Actual guest-generated AI
samples exercise this determinism check.

The current private **Super Mario 64 (USA)** probe still blocks on triangle
**CF000FFA0FFAF006**, CURRENT **227160**, after **200,000,000 CPU attempts**.
Video remains uniform; native boot and playability are unverified. The current
production core SHA-256 is
`56dd577178d40bae0b7b47125381f55b27d942db73bba49c752fc1eea42818f5`;
trace SHA-256 is
`b6fc9dd72f1c2390043903d91c3fbdab75ae449122632a1d3de6bdc5c388a650`.
Reports: `build/artifacts/retail/n64-controllers.json` and default `n64.json`.
Commercial ROM bytes remain local.

N64 now has **45% = 9/20 fixed delivery milestones**. This credits production
netplay for the implemented profile. Analog/accessories/presence detection,
local multiplayer frontend routing, complete graphics/audio/CPU/devices,
physical timing and retail playability remain unfinished. CI/CD remains disabled.
**77 distinct N64 gates pass cumulatively**. The broad regression run passed
71/73 in **1,906.17 seconds**; two legacy fixture checks still used old EOF
offsets. Updating those offsets for the 24-byte S023 extension preserves their
original assertions; both pass in a **22.32-second targeted rerun**. The four
new controller/netplay gates pass. All six production core network gates pass
in **57.15 seconds**. The full build, sanitizer, completion renderer, changed
local documentation links and whitespace checks pass. All 94 original archives
reproduce. Production binaries were unchanged by the fixture corrections.


## RDP complete command packets

Plugin **0.28.0**, emulator ABI v2, fetches complete native command packets,
including all eight triangle forms (4–22 64-bit words) and two-word texture
rectangles. CURRENT advances with each fetched word. Partial packets survive
an END boundary and continue on extension; coefficient words cannot become
commands. A complete unimplemented draw retains its packet and reports a
blocker without drawing or asserting FullSync. Freeze/flush and nominal issue
counters retain their functional-profile conventions; physical FIFO depth,
prefetch/raster timing and reserved electrical behavior remain ungraded.

The [original cartridge](../../tests/roms-src/n64/rdp_packets/README.md) passes **200,704 cases / 1,404,928 actual words**:
all repeated-byte payloads, every opcode-byte alias, RDRAM/wrapping XBUS and
every partial split in all triangle/rectangle packets. Actual CURRENT/STATUS/MI
pending before and after END extension and a RAM guard are independently graded.
Every coefficient is headed by FullSync's byte; none escapes as an interrupt.
The executed mutant fails all **72** short cases; seven field and seven stream
controls reject. Artifacts: `build/artifacts/owned/n64_rdp_packets/`.

The component verifies **1,441,792 coefficient words**, **648 partial header
states**, and **10,240 RDRAM/XBUS streams / 110,592 word-boundary captures**.
Every eight-byte DMEM alignment, retained data, freeze/flush, fetch faults and
state rejection/replay are checked. The old DP implementation failed the
incomplete-packet check by blocking on its first word. Component UBSAN passes
with clean compilation. A production ABI gate checkpoints a real partial guest
packet and restores through `INetplayCapable`, reproducing actual output and
complete states, with exact capacity and malformed-state guards.

State **VN64S024** appends **200 bytes** to S023 (start/count/needed and 22
canonical words), totaling **8,478,944 bytes**. Unused words must be zero;
invalid counts/lengths/addresses reject before any machine mutation. Accepted
older formats receive a neutral packet and preserve their represented prefix.
Legacy checks now account for the appended bytes. Existing four digital ports
and production TCP capabilities remain present; timing and analog are pending.

The private **Super Mario 64 (USA)** probe was repeated for **2,000 scheduling
quanta / 200,000,000 CPU attempts**. It has no terminal CPU fault and uniform
video. Packet **CF000FFA0FFAF006** is now retained completely: **22/22 words**,
start **0x227160**, fetched CURRENT **0x227210**, blocker reason 1. Triangle
shading/texturing/depth and native boot/playability remain unimplemented or
unverified. The reader does not fabricate successful graphics. Reports are
local at `build/artifacts/retail/n64-packets.json`, including all 22 retained
packet words for renderer diagnosis; no commercial bytes are
archived. Core SHA-256: **5b97a566a9030d0beb1b94bf1226725dd14c81dc5b93933c5f07759d9da07df4**;
trace SHA-256: **29498832f3274f0a64b72e984b01a80ee89efdc67717e7c7d52a65bfb5324939**.

All **102 original archives** reproduce. **80 distinct N64 gates pass
cumulatively**: the 79-gate batch finished in **2,598.12 seconds**, with 78
passes and one sandbox socket-creation failure; that TCP netplay gate passes
with local networking allowed (**2.66 seconds**, unchanged binaries). The new
production packet-state ABI gate separately passes (**0.15 seconds**). Final
component UBSAN, completion rendering and whitespace checks pass. N64 remains
**45% = 9/20 delivery milestones**, with complete native CPU/devices/RSP/RDP/audio, boot/retail play,
analog/accessories/local frontend routing, physical timing and the exhaustive
native/physical corpus still pending. CI/CD remains disabled; the full Nintendo
implementation goal remains active.

## RDP fill-cycle triangle spans

Plugin **0.29.0**, emulator ABI v2, now executes fill-cycle triangle packets.
The decoder separates all eight edge/shade/texture/depth layouts. Edge walking
uses signed 28-bit X and 30-bit slopes, quantized quarter-scanline increments,
mid-edge replacement, sticky fractional clipping and field selection. Bit 55
set places the major edge on the left in the verified raster path. Fill-cycle
writes use the fill word; shade/texture/Z payloads remain attributes rather than
commands. One/two-cycle triangles remain explicitly blocked.

The [original framebuffer cartridge](../../tests/roms-src/n64/rdp_triangle_fill/README.md) passes **795,648 cases /
10,343,424 actual 64-bit words**. Its color domain covers all 65,536 halfword
patterns combined with their complements, three pixel sizes, both sources and
both directions. The separate 9,216-row matrix covers eight geometry/scissor
profiles, all 32 triangle opcode bytes, three field selections, both sources
and three pixel sizes. Actual framebuffer words, guards, CURRENT, STATUS and
MI pending are independently graded. The executed mutant fails all **9,288**
short cases; thirteen field and seven stream controls reject. This does not
exhaust all 32-bit fill words or arbitrary native geometry combinations.
Artifacts: `build/artifacts/owned/n64_rdp_triangle_fill/`.

Component gates verify **24,576 coefficient basis packets**, **2,097,152
halfword routing packets**, **1,572,864 signed-Y packets**, **4,096 header
combinations** and **640 atomic rejections**. The modular incremental edge
oracle matches **7,062,117 quarter-edge captures**. Ninety-six packet/field
fill dispatches plus mirrored golden spans verify actual RAM, bounds/kill
rejection and payload versus real FullSync. Final component UBSAN passes with
clean compilation. The earlier DP failure gate now explicitly submits an
unsupported one-cycle triangle; legal empty fill triangles can complete.

**15 affected N64 verification gates pass**, including the original rectangle,
fill-pipeline and packet cartridges, production packet-state replay, controller
ports, state and two-process TCP netplay. The four integration gates pass in
**103.14 seconds**; seven component gates pass in **3.11 seconds** and three
state/controller gates in **0.39 seconds**. All **103 original archives**
reproduce. State remains **VN64S024 / 8,478,944 bytes**; these raster calculations
add no serialized machine fields. Four digital ports and TCP capabilities
remain present; native analog/accessories and timing remain pending.

The private Mario probe was repeated for **2,000 scheduling quanta /
200,000,000 CPU attempts**. No terminal CPU fault is observed, video is uniform,
and the same one-cycle packet **CF000FFA0FFAF006** remains blocked: **22/22**
words, start **0x227160**, fetched CURRENT **0x227210**, reason 1. Report:
`build/artifacts/retail/n64-triangle-fill.json`. Core SHA-256:
**67f08620c2e60314900b3b3ece19c65765343413f322f1c002f866d4e561b47b**;
trace SHA-256: **339076695f22f7a0255e8352f5c670dc9e029dcd9a935767e54603b0075155f8**.
No commercial bytes are archived.

N64 remains **45% = 9/20 delivery milestones**. Shading, texture sampling,
coverage/blending/depth, hidden RDRAM bits, native FIFO/raster timing, crash
partial writes, complete CPU/devices/RSP/audio, native boot/retail play and the
complete native/physical corpus remain unfinished. Unsafe profiles/bounds
reject before writes under the functional convention. CI/CD remains disabled;
the full Nintendo implementation goal remains active.

## RDP subpixel coverage helpers

The triangle renderer now shares clipped quarter-row geometry with an eight-sample
coverage helper. Component checks compare the packed mask to an independent
point-coordinate oracle: **134,217,728 boundary-pair observations**,
**655,360 mixed-row/range observations**, all **256 coverage bytes** and
**576 coverage destination states**. Fixed triangle masks also check edge
samples. The component passes undefined-behavior sanitization.

This helper is preparation for one-cycle rendering. It is not yet connected
to framebuffer coverage storage, blending or depth testing, and has no guest
coverage-output corpus. The original fill renderer uses the shared geometry;
shade interpolation and shaded/textured/depth-tested triangles remain pending.
N64 remains **45% = 9/20 delivery milestones**. CI/CD remains disabled.

The private Mario probe was repeated for **2,000 scheduling quanta /
200,000,000 CPU attempts**. No terminal CPU fault is observed; video is uniform.
The same packet **CF000FFA0FFAF006** remains blocked (reason 1), with **22/22
words** retained, start **0x227160** and fetched CURRENT **0x227210**.
Boot/playability remain unverified. Local report:
`build/artifacts/retail/n64-coverage.json`. Core SHA-256:
**435f192a075e654bab2055115497fd0da284e6ddc43dde5e422a3d29cbfaa944**;
trace SHA-256: **6679232a8169c2169dc180f0e532286be90bcd16f6ff6d58491373752d3d5477**.
No commercial ROM bytes are archived.

The shared geometry also passes the original framebuffer cartridge: **795,648
cases / 10,343,424 actual 64-bit words** independently graded. The executed
mutant fails all **9,288** short cases; all thirteen field and seven malformed
stream controls reject. The existing DP and triangle fill component gates pass
in **3.03 seconds**; the registered coverage gate passes in **1.18 seconds**.
Artifacts: `build/artifacts/owned/n64_triangle_coverage_regression/`.

## Triangle row interpolation

A functional interpolation module now prepares **RGBA, S/T/W and Z** from
decoded triangle coefficients. It handles modular row accumulation, the
major-edge load subrow, fractional-X correction, clipped-start wrap, quantized
pixel increments, coverage-position shade/depth correction and depth-gradient
normalization. Arithmetic follows the
[RDP author's rasterizer model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/rasterizer.c);
these software oracles do not establish physical raster timing.

The separate incremental host oracle passes **196,608 row captures** and
**2,949,120 pixel captures**, including all eight interpolants. Additional
checks cover **327,680 depth-normalization inputs**, **4,194,304 shade-gradient
corrections** (all signed 13-bit gradients, both axes and every coverage mask),
**12,582,912 depth corrections** (all 22-bit depth codes under three selected
masks/gradients), and **131,072 constant-shade/mask combinations**. Invalid
row/pixel bounds reject atomically. The final component passes UBSAN with
clean warning-as-error compilation.

Five related component gates pass in **2.98 seconds**; the fault-runner gate
passes in **6.90 seconds**. The repository's
repeatable fault runner first verifies the unmodified module, then requires
all **eight deliberately faulty module variants** to fail the independent
oracle; compilation errors do not count as rejection. Artifacts:
`build/artifacts/component/n64_triangle_attributes/`. From the repository root:

```sh
cmake --build build --target n64_rdp_triangle_attributes_test
ctest --test-dir build -R '^n64_rdp_triangle_(attributes_test|attribute_mutants)$' --output-on-failure
```

This module is not yet connected to one/two-cycle framebuffer rendering. It
has component evidence, with no guest interpolation-output corpus yet.
Perspective division, texture sampling, blending, hidden memory bits and depth
testing remain pending. Production core/trace executable hashes still match
the preceding private Mario report (`build/artifacts/retail/n64-coverage.json`):
Mario remains blocked at its shaded/textured/depth-tested triangle, with
uniform video and unverified boot/playability. No commercial bytes are
archived. Plugin **0.29.0**, ABI v2 and **VN64S024** remain unchanged.
N64 stays **45% = 9/20 delivery milestones**. CI/CD remains disabled; the full
Nintendo implementation goal remains active.

## One-cycle RGBA32 triangle pixels

Plugin **0.30.0**, ABI v2, connects quarter-row geometry, coverage masks,
RGBA interpolation and combiner source routing to actual one-cycle RGBA32
framebuffer stores. Supported inputs include shade, primitive/environment
colors, ONE/ZERO and primitive LOD. One-cycle uses the second combiner program.
All four coverage destinations and optional RGBA32 memory-coverage reads work;
with AA disabled, the upper-left coverage sample gates stores. Width is a
stride, not an X clip. Unsupported profiles and unsafe bounds reject before
writes under the functional convention. Real FullSync remains the IRQ source.

The original one-cycle cartridge covers **792,576 cases / 10,303,488 actual
64-bit observations**: all 65,536 RG pairs with complemented BA, three programs,
both directions and both RDRAM/wrapping XBUS sources, plus a 6,144-case matrix
of eight geometry profiles, eight programs, fields, sources, coverage
destinations and memory-read settings. Actual framebuffer/guard words and
CURRENT/STATUS/MI are independently graded. The guest contains numeric commands
and capture instructions, with expected pixels only in the separate host
oracle. This does not exhaust arbitrary RGBA colors, gradients or native
pipeline combinations. Artifacts: `build/artifacts/owned/n64_rdp_triangle_shade/`.

Combiner component checks pass **2,170,880 RGB captures** and **4,194,304 alpha
captures**, both cycles, all selector products under declared source patterns
and each RGB selector under all 512 varied nine-bit patterns. The complete
source router accepts explicit combined/texel/key/noise/conversion/LOD inputs;
those inputs are not yet generated by the renderer. Eight fixed framebuffer/
coverage goldens and **47 atomic profile/packet/bounds rejections** pass.
Final routing and pixel-component UBSAN pass with warning-as-error compilation.
Alpha arithmetic has component evidence; RGBA32 framebuffer alpha stores
coverage, not the combiner alpha.

The **12 affected component/state/controller gates** pass in **7.58 seconds**.
The three original rectangle/fill-pipeline/packet regression cartridges and
new shade component pass in **98.82 seconds**. Production two-process TCP
netplay passes in **2.69 seconds**. All **104 original archives** reproduce;
the complete build is warning-free. State remains **VN64S024 / 8,478,944
bytes**; the independent one-cycle profile adds no retained feedback fields.

The private Mario probe was repeated for **2,000 scheduling quanta /
200,000,000 CPU attempts** with no terminal CPU fault. Video remains uniform;
packet **CF000FFA0FFAF006** is still blocked (reason 1), **22/22 words**, start
**0x227160**, fetched CURRENT **0x227210**. Boot/playability remain unverified.
Report: `build/artifacts/retail/n64-one-cycle-shade.json`. Core SHA-256:
**549f2bda0b3de2dc7d7606dec23b54dd539f62b846933967392f894b355e5f1d**;
trace SHA-256: **ca4c14e6dc6d1473b17af09f831fb16092c8ec7b83bc6fb97839a94b6218ad1f**.
No commercial ROM bytes are archived.

N64 remains **45% = 9/20 delivery milestones**. Complete native CPU/devices/
RSP/RDP/audio and physical timing/corpora, retained combiner feedback, keying,
texture fetch/LOD/perspective, blending/AA/dither/alpha comparison, other
framebuffer formats and hidden bits, Z testing and native retail play remain
pending. Four digital ports/TCP remain present; analog/accessories/local
multiplayer frontend routing remain unfinished. CI/CD remains disabled; the
full Nintendo implementation goal remains active.

The full original framebuffer corpus passes all **792,576 cases**. Its
**6,216-case** short baseline passes; the executed capture mutant fails all
6,216 cases, thirteen field controls each detect one mismatch, and seven
malformed-stream controls reject. Together the evidence above accounts for
**18 distinct affected N64 gates**; it is not a fresh whole-core gate batch.
Completion rendering and whitespace checks pass.

## Texture tile coordinate processing

The texture pipeline now has a functional tile-coordinate module for one/
two-cycle sampling. It accepts signed 16-bit S/T with five fractional bits
after division/LOD saturation, applies all sixteen shift encodings, subtracts
fractional tile origins, clamps integer/fractional coordinates, and applies
mask/mirror rules. Mask widths above ten saturate at ten; mask zero enables
clamping. Upper-bound clamping takes precedence for inverted bounds. Coupled
neighbor coordinates preserve the native model's distinct S/T wrap rules and
mirror endpoints. The implementation follows the
[RDP author's coordinate model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tcoord.c)
and [coupled mask model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tex.c).

Independent scalar division/modulo/reflection oracles pass **134,217,728
coordinate/shift/mask/flag captures**: all 65,536 raw coordinates, sixteen shifts,
sixteen masks, clamp/mirror combinations and both axes, at one selected bound
pair. A second **134,217,728 captures** cover every 12-bit origin/upper-bound
pair on both axes, four probes derived from the unshifted bounds and varied
shift/mask/flag settings. Those bound pairs do not cross every possible raw
coordinate and flag combination. **65,536 paired-axis captures** and fixed S/T
wrap goldens also pass. The final component passes UBSAN and warning-as-error
compilation.

The registered coordinate component passes in **3.49 seconds**. The repeatable
fault runner passes in **14.39 seconds**: it compiles and verifies an unchanged
baseline, then requires all eight faulty production-header variants to fail
the independent oracle. Compiler errors never count as rejection. Three
related interpolation/shade/combiner component gates pass in **1.18 seconds**.
Artifacts: `build/artifacts/component/n64_texture_coordinates/`. From the
repository root:

```sh
cmake --build build --target n64_rdp_texture_coordinates_test
ctest --test-dir build -R '^n64_rdp_texture_coordinate' --output-on-failure
```

This helper has component evidence and no guest coordinate-output corpus yet.
It is not connected to TMEM texel fetch or pixel rendering. Texture formats,
TLUT sampling, filtering, perspective/LOD and texture-driven combiner inputs
remain unfinished; physical timing is not established by software oracles.
Current production core/trace hashes still match the preceding private Mario
report at `build/artifacts/retail/n64-one-cycle-shade.json`. Mario remains
graphics-blocked with uniform video and unverified boot/playability. Native
hardware/retail completion remains unproven. Plugin **0.30.0**, ABI v2 and
**VN64S024** remain current. N64 stays **45% = 9/20 delivery milestones**;
CI/CD remains disabled and the full Nintendo implementation goal remains active.

## TMEM texel and palette reads

A functional texel reader now decodes canonical big-endian TMEM bytes with
odd-row interleaving, address wrapping and split RGBA32 banks. RGBA, CI, IA,
I and YUV format/size encodings include the native model's alias behavior.
YUV results retain signed U/V and unconverted Y/A; they are not presented as
converted RGB. TLUT index decoding and explicit palette-bank reads support
RGBA5551 and IA16 entries. Bank selection/remapping in the full sampling
pipeline remains separate. The layout follows the
[RDP author's TMEM reader model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tmem.c).

Independent byte-address/permutation and fixed-code expansion oracles pass
**3,145,728 address captures**, **8,388,608 packed-format captures** and
**1,050,624 palette decodes**. Address tests include signed coordinate patterns
across every format/size code and every tile base/line pair under selected
RGBA32 probes. Packed tests cover all 65,536 halfword patterns with complemented
upper-bank data at four selected S/T positions; they do not exhaust arbitrary
32-bit colors or all YUV triples. Palette tests cover all 16-bit entries at
two indices/all four lanes/both interpretations, plus every index/lane under
one selected TMEM pattern. A further **12,288 actual load/fetch captures**
verify RGBA16/RGBA32 data through the existing TMEM loader, two rows, three
wrap-sensitive bases and 256 byte patterns.

Final UBSAN and warning-as-error compilation pass. The two registered reader/
fault gates pass in **5.71 seconds**; three related tile/TMEM/coordinate gates
pass in **5.78 seconds**. The repeatable fault runner requires the unmodified
baseline to pass and all eight deliberately faulty production-header variants
to fail the independent oracle. Compilation failures never count as rejection.
Artifact header SHA matches the current implementation. Artifacts:
`build/artifacts/component/n64_texel_fetch/`. From the repository root:

```sh
cmake --build build --target n64_rdp_texel_fetch_test
ctest --test-dir build -R '^n64_rdp_texel_fetch_' --output-on-failure
```

This helper has component evidence and no guest texel-output corpus yet. It
is not connected to the pixel renderer. Coupled multi-texel sampling, filtering,
YUV conversion, perspective/LOD, texture-driven combiner routing and complete
blending/depth/hidden-memory behavior remain unfinished; software oracles do
not establish physical timing. Current production core/trace hashes still
match the private Mario report at
`build/artifacts/retail/n64-one-cycle-shade.json`. Mario remains graphics-blocked
with uniform video and unverified boot/playability. Plugin **0.30.0**, ABI v2
and **VN64S024** remain current. N64 stays **45% = 9/20 delivery milestones**.
CI/CD remains disabled; the full Nintendo implementation goal remains active.


## Texture filter and conversion arithmetic

Prepared four-corner texels now support the RDP's triangular filtering,
mid-texel four-corner average, signed nine-bit previous-texel feedback and
four-coefficient color conversion. Chroma fractions account for YUV S parity;
chroma and luma choose their triangles separately. Results retain nine bits
for the combiner. This arithmetic follows the
[RDP author's texture pipeline model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tex.c).

An independent weighted-corner oracle passes **67,174,400 RGB lattice cases**,
**2,097,152 chroma lattice cases**, **1,310,720 previous-texel cases** and
**2,359,296 conversion cases**. RGB probes cover every pair of two eight-bit
pattern seeds at all 32-by-32 fractional positions; these correlated corner
patterns do not exhaust arbitrary four-corner colors. Chroma probes cover
selected signed patterns, S parity and mid-texel modes. Feedback probes cover
all encoded nine-bit R/G pairs at five selected fractional positions.
Conversion probes cover all nine-bit coefficient/chroma pairs under selected
branch modes, plus all K1/K2 pairs under selected previous-texel inputs.
Explicit center goldens distinguish four-corner averaging from triangular
filtering. Warning-as-error compilation and UBSAN pass.

The registered arithmetic gate and repeatable production-fault runner pass.
The runner requires its unmodified baseline to pass and all nine deliberately
faulty production-header variants to fail the independent oracle; compilation
failures never count as rejection. Faults exercise triangle selection,
rounding, chroma parity, center averaging, feedback sign/center correction,
result width, coefficient oddness and mixed chroma/luma selection. Artifacts:
`build/artifacts/component/n64_texture_filter/`. From the repository root:

```sh
cmake --build build --target n64_rdp_texture_filter_test
ctest --test-dir build -R '^n64_rdp_texture_filter_' --output-on-failure
```

This helper has component evidence and no guest filter-output corpus yet.
It accepts prepared corners and explicit options; coupled TMEM/TLUT fetch,
mode/convert command state, persistent previous texels and pixel rendering
are not connected. Point sampling, perspective/LOD, texture-driven combiner
routing and full blending/depth/hidden-memory behavior remain unfinished.
Software oracles do not establish physical timing. Current production
core/trace hashes still match the private Mario report at
`build/artifacts/retail/n64-one-cycle-shade.json`. Mario remains graphics-blocked
with uniform video and unverified boot/playability. Plugin **0.30.0**, ABI v2
and **VN64S024** remain current. N64 stays **45% = 9/20 delivery milestones**.
CI/CD remains disabled; the full Nintendo implementation goal remains active.


## Coupled TMEM and TLUT sampling

Four-corner TMEM reads now preserve neighboring-row carry after the initial
8-bit row normalization. Format aliases retain the scalar reader's decoding;
YUV chroma uses its coupled horizontal stride while luma uses the ordinary
neighbor. YUV4/8 and palette modes remap B/A corners when the chroma and luma
triangles differ. TLUT reads select physical banks by corner and chroma
triangle, supporting both RGBA5551 and IA16 interpretations. Nearest TLUT mode
reads all four banks of one index. These rules follow the
[RDP author's coupled TMEM model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tmem.c).

Independent byte-address, fixed-code expansion and lane-assignment oracles
pass **2,097,152 raw quad captures**, **8,388,608 palette quad captures** and
**3,145,728 row-carry captures**. Quad captures include 65,536 selected signed
S/T and neighbor-difference patterns across every format/size encoding;
palette captures test both interpretations and coupled/nearest modes against
nonidentical physical banks. Row-carry probes cover every tile base/line pair
for RGBA, YUV and intensity at every size, with a selected row-255-to-256
transition. These patterns do not exhaust all arbitrary TMEM contents or all
independent coordinate/neighbor combinations.

A further **1,536 composed load/filter captures** run RGBA16/RGBA32 data through
the actual TMEM loader, tile coordinate processing, coupled fetch and mid-texel
filtering. Four source pixels and their independently averaged colors are
checked across 256 patterns and three wrap-sensitive bases. **1,024 actual
TLUT load/fetch captures** verify full 256-entry palette loads, CI8 sampling,
both palette interpretations and coupled/nearest modes. The palette load tile
uses the zero-size descriptor prescribed by the
[SDK's LoadTLUT macros](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm).

Warning-as-error compilation and UBSAN pass. The registered coupled-fetch
and production-fault gates pass. The unmodified runner baseline passes and
all nine compiled faults are rejected, covering row carry, chroma stride,
palette banks, lane remapping, nearest index selection, YUV nibble/alpha/luma
addressing and palette type. Compilation failures never count as rejection.
The existing scalar fetch, scalar fault, descriptor, coordinate and filter
gates also pass after the shared scalar decoder refactor. Artifacts:
`build/artifacts/component/n64_texel_quad/`. From the repository root:

```sh
cmake --build build --target n64_rdp_texel_quad_test
ctest --test-dir build -R '^n64_rdp_texel_quad_' --output-on-failure
```

This module has component and composed-path evidence, with no guest sampling
output corpus yet. Pixel rendering, mode/convert command state, persistent
previous texels, point sampling without TLUT, perspective/LOD and complete
blending/depth/hidden-memory behavior remain unfinished. Software oracles do
not establish physical timing. Production core/trace hashes still match the
private Mario report at `build/artifacts/retail/n64-one-cycle-shade.json`.
Mario remains graphics-blocked with uniform video and unverified boot/playability.
Plugin **0.30.0**, ABI v2 and **VN64S024** remain current. N64 stays
**45% = 9/20 delivery milestones**. CI/CD remains disabled; the full Nintendo
implementation goal remains active.


## Affine textured triangle pixels

The functional one-cycle RGBA32 renderer now samples TMEM and feeds the
current TEXEL0 into cycle one of the combiner. Signed affine S/T from native
triangle interpolants pass through tile shift/clamp/mask processing. Point
sampling, triangular filtering, mid-texel averaging and coupled/nearest TLUT
sampling use the previously verified fetch/filter modules. TLUT supports
RGBA5551 and IA16. YUV filtering retains unconverted signed chroma; conversion
command state is not implemented in this path. The native cycle-zero texture
and cycle-one combiner ordering follows the
[RDP author's pipeline model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/rasterizer.c).

The new repository-owned
[texture triangle ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_triangle_texture)
passes **786,432 cases / 10,223,616 actual 64-bit framebuffer/guard/register
observations**, with zero mismatches. It loads real source bytes through DP
texture-image/tile/load/bounds commands and emits observed RAM, CURRENT,
STATUS and MI pending. Its host oracle independently decodes logical source
pixels, computes weighted-corner filters and walks a quarter-row point grid.

The source domain covers every 16-bit word and complemented partner under
selected RGBA16/RGBA32/IA16 point probes and both command sources. The sampling
domain covers all 32-by-32 fractional pairs, every format/size encoding and
alias, point/triangular/average modes, both triangle directions and
RDRAM/wrapping XBUS using one selected texture pattern. It does not exhaust
arbitrary RGBA32 colors, independent corners, gradient/tile/geometry
combinations, physical timing or retail graphics. This guest corpus has no
TLUT pixel cases yet; four independent component framebuffer goldens cover
coupled/nearest palette rendering with both palette interpretations.

The short guest control passes; a separately executed wrong-capture ROM is
rejected in every row. All thirteen field corruptions and seven stream
integrity/domain/trailer controls are rejected. Four texture combiner
framebuffer goldens cover passthrough and texture/primitive RGB/alpha factors;
35 unsupported-profile rejections preserve RAM. UBSAN and warning-as-error
compilation pass. All 22 related machine/DP/RDP/state component gates pass in
**20.32 seconds**. Production-plugin texture/TMEM and existing partial-command
checkpoint gates pass in **0.31 seconds**, requiring exact subsequent actual
guest output and complete-state replay through the netplay ABI, exact state
capacity and atomic invalid-state rejection. All **105 repository-owned
archived ROMs** reproduce from their sources and match the manifest.

Artifacts: `build/artifacts/owned/n64_rdp_triangle_texture/`. From the repository
root:

```sh
python3 tests/roms-src/build.py n64/rdp_triangle_texture --original-only
ctest --test-dir build -R '^n64_rdp_(triangle_texture|texture_plugin_replay_test)' --output-on-failure
```

The private Mario probe was repeated with current plugin/trace binaries:
**200 million CPU attempts**, no terminal CPU fault, the same complete
**22/22-word CF000FFA0FFAF006 packet at 0x227160** and fetched CURRENT
**0x227210**. The textured/depth-tested profile still blocks with reason 1;
video remains uniform and native boot/playability remain unverified. Report:
`build/artifacts/retail/n64-textured-affine.json` (ignored, no commercial bytes).
Plugin **0.31.0**, ABI v2 and **VN64S024 / 8,478,944 bytes** are current.

Supported textured pixels require the cycle-zero filter/passthrough flag;
conversion/persistent previous texels, TEXEL1/LOD/key/noise routes, perspective,
full AA/blending/depth/hidden-memory behavior, complete devices/audio and
physical timing remain unfinished. State completeness is limited to the implemented profile; the full Nintendo
implementation goal remains active. N64 stays **45% = 9/20 delivery milestones**;
CI/CD remains disabled.


## SetConvert and texture color conversion

SetConvert now retains all six nine-bit fields, including the K2 field split
across the command's two words. K0..K3 supply signed odd conversion weights;
K4/K5 supply the native cycle-one combiner routes. The functional affine
RGBA32 path supports color conversion with cycle-zero filtering disabled,
using separate chroma/luma triangle selection, as well as the preceding
filter/passthrough modes. The initial zero weights and programmed zero-field
weights are distinct states. Arithmetic and command decoding follow the
[RDP author's conversion model](https://github.com/ata4/angrylion-rdp-plus/blob/master/src/core/n64video/rdp/tex.c).

The new original
[color-conversion ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_color_convert)
passes **1,318,912 cases / 17,145,856 actual 64-bit framebuffer/guard/register
observations**, with zero mismatches. **1,048,576** cases cover every K0..K3
field encoding independently against 256 selected source-byte patterns under
both command sources. **262,144** cases cover every K4/K5 pair in a real
texture/constant combiner equation with an RGBA16 source family. **8,192**
cases cover all fractional pairs under selected YUV16/YUV32/RGBA16/IA16 tiles,
fixed coefficients and both RDRAM/wrapping XBUS sources. UV, Y and corner
patterns are correlated; independent U/V/Y triples, joint K0..K3 tuples and
all palette/gradient/geometry combinations are not exhaustive.

The host independently models logical source pixels, signed coefficient and
combiner arithmetic, and an explicit quarter-row point grid. A 425-case
control passes; a separately executed wrong-capture ROM is rejected in every
row. All thirteen observation-field corruptions and seven malformed/domain/
trailer stream controls are rejected. **98,304** independent component command
captures cover every coefficient field, opcode aliases, ignored bits and both
command sources. Four component framebuffer goldens distinguish mixed chroma/
luma selection, zero initial weights, odd programmed weights and the signed
coefficient boundary. Warning-as-error compilation and UBSAN pass. The
refactored filter arithmetic's baseline passes and all nine compiled production
faults are rejected by its repeatable runner.

State **VN64S025** appends **8 bytes**, totaling **8,478,952 bytes**, for the
packed fields and a programmed-state flag. Reserved/noncanonical conversion
states reject atomically. S024 imports preserve every preceding byte and
initialize conversion state to zero; the earlier state imports remain tested.
State-offset fixtures were updated for the new tail while preserving their
original field and legacy-import checks. All 46 machine/device/FPU/RSP/RDP/
netplay regression gates pass across the recorded runs. The new production
conversion checkpoint gate and existing texture/partial-packet checkpoint
gates pass: exact subsequent actual guest output, full-state replay, current
netplay-ABI capacity and atomic malformed-state rejection. Production TCP
rollback passes with local sockets enabled. All **106 repository-owned archived
ROMs** reproduce and match their manifest.

Artifacts: `build/artifacts/owned/n64_rdp_color_convert/`. From the repository
root:

```sh
python3 tests/roms-src/build.py n64/rdp_color_convert --original-only
ctest --test-dir build -R '^n64_rdp_(color_convert|convert_test|conversion_plugin_replay_test)' --output-on-failure
```

The private Mario probe was repeated with current plugin/trace binaries at
**200 million CPU attempts**, with no terminal CPU fault. The same complete
**22/22-word CF000FFA0FFAF006 packet at 0x227160**, fetched CURRENT **0x227210**
and reason 1 remain; video is uniform and native boot/playability unverified.
Report: `build/artifacts/retail/n64-color-convert.json` (ignored; no commercial
bytes committed). Plugin **0.32.0**, ABI v2 and S025 are current.

Perspective/LOD, two-cycle feedback and previous texels, full AA/blending/depth/
hidden-memory behavior, complete devices/audio, hardware-wide corpora and
physical timing remain unfinished. The full Nintendo implementation goal
remains active. N64 stays **45% = 9/20 delivery milestones**; CI/CD remains
disabled.

## Perspective texture division

The functional one-cycle RGBA32 triangle path now divides interpolated S/T
by W when perspective mode is enabled. A dedicated integer divider models
15-bit W normalization, piecewise reciprocal interpolation, the exceptional
segment-six endpoint, 17-bit coordinates and overflow/underflow flags.
Native saturation precedes tile shifts, bounds, mask/mirror and TMEM sampling.
Zero and negative W set the positive overflow flag, including its precedence
over simultaneous underflow. Affine mode preserves every signed 16-bit input.
Numerical rules were checked against the
[pinned RDP implementation](https://github.com/ata4/angrylion-rdp-plus/blob/192ad5de5b0f50bfa55f9f97208feac4bc87f885/src/core/n64video/rdp/tcoord.c).
Physical console measurements remain absent.

The scalar gate passes all **4,294,967,296 S/W input pairs**, with complementary
T lanes, against independently encoded reciprocal endpoints and signed
interval overflow tests. All **524,288** 19-bit saturation encodings and all
**65,536** affine coordinate identities also pass. This exhausts each scalar
coordinate/W pair, not all joint S/T/W triples. A smaller 8,388,608-pair boundary
corpus rejects all **nine compiled production faults**; compilation failures
are not counted as oracle rejections. Warning-as-error and UBSAN runs pass
for that boundary corpus and the varying-W framebuffer goldens.

The new original
[perspective texture ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_texture_perspective)
passes **1,572,864 cases / 20,447,232 actual 64-bit framebuffer/guard/register
observations**, with zero mismatches. **524,288** cases cover every W with four
selected S/T pairs; **1,048,576** cover every S with complementary T and eight
selected W values. Both use RDRAM and wrapping XBUS command delivery, real
TMEM loading and FullSync. The host derives numerical division, logical grid
colors and quarter-pixel coverage independently of command packing and the
production implementation. The 288-case control passes, its separately
executed wrong-capture ROM fails every row, and all thirteen field flips and
seven malformed/domain/trailer controls are rejected. Gradient, geometry,
format/palette/filter and joint S/T/W combinations are not exhaustive.

Two component framebuffer goldens use tile seven and W changing across both
X and Y, proving point and filtered rendering use per-pixel division. Eight
unsupported LOD/depth/blend/cycle mode changes reject atomically. The fifteen
existing DP/TMEM/combiner/interpolant/filter/state/replay regression gates pass,
as do the updated textured profile gate and perspective framebuffer gate.
The production perspective checkpoint requires programmed perspective mode
and populated TMEM; exact subsequent actual guest output and the complete
state replay through the netplay ABI. Existing genuine partial-packet,
texture and conversion checkpoint gates also pass after the checkpoint
fixture change. The full build succeeds. All **107 repository-owned ROM
archives** reproduce and match their manifest.

The divider is derived immutable state; serialized machine layout stays
**VN64S025 / 8,478,952 bytes**, ABI v2. Plugin **0.33.0** is current. Repeat:

```sh
ctest --test-dir build -R '^n64_rdp_(texture_divider|perspective)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_texture_perspective/` and
`build/artifacts/component/n64_texture_divider/`. The private Mario probe was
repeated with current plugin/trace binaries at **200 million CPU attempts**,
without a terminal CPU fault. It retains the same complete **22/22-word
CF000FFA0FFAF006 packet at 0x227160**, fetched CURRENT **0x227210** and reason 1.
Video remains uniform; boot and playability remain unverified. Report:
`build/artifacts/retail/n64-perspective.json` (ignored; no commercial ROM bytes
committed).

LOD/mipmapping, two-cycle/previous-texel feedback, full AA/blending/depth/hidden
memory, complete devices/audio, physical timing and hardware-wide corpora
remain unfinished. The full Nintendo implementation goal remains active.
N64 stays **45% = 9/20 delivery milestones**; CI/CD remains disabled.

## One-cycle blending and alpha coverage

The functional RGBA32 triangle path now executes the one-cycle blender after
combiner and texture processing. It supports all cycle-zero color/alpha source
selectors, forced and AA-enabled normalized blending, full-alpha partial
rejection, constant alpha thresholds, coverage-times-alpha, alpha-from-coverage,
color-on-coverage and all coverage destinations. Quantized factors, memory-alpha
shifters for disabled Z comparison and eight-bit force-blend output wrapping
follow the [pinned primary RDP model](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/blender.c).
The native quotient has **32,768 entries**, all compared byte for byte against
a separately compiled primary lookup initializer. Physical console measurements
remain absent. Dithering, Z comparison/update and two-cycle execution still
reject atomically in this path.

SET_FOG_COLOR and SET_BLEND_COLOR retain their full 32-bit colors. Memory RGB
is retained across pixels/commands when image reads are disabled; only memory
alpha/coverage changes to the native default. Image reads update the latch even
when the later alpha/coverage decision suppresses a framebuffer write. Two
framebuffer goldens additionally composite point/filtered perspective texture
samples with primitive alpha over actual framebuffer memory. The existing
varying-W pixel goldens remain passing.

The new original
[one-cycle blending ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_one_cycle_blend)
passes **655,360 cases / 8,519,680 actual 64-bit framebuffer/guard/register
observations**, with zero mismatches. **524,288** cover every selected RGB seed/
alpha pair under four blend/AA/coverage profiles and both command sources;
**131,072** cover all 256 selector combinations against 256 correlated patterns
and both sources. A host quotient/decision/quarter-grid oracle supplies expected
values without importing command packing or the production implementation.
The 1,616-case control passes, its separately executed wrong-capture ROM fails
every row, and all thirteen field flips and seven stream controls are rejected.
The guest uses constant shade, fixed fog/blend colors and background coverage;
joint color/alpha/state/texture/gradient/geometry domains remain incomplete.

Component gates pass all **134,217,728 weight/color equations**, **2,424,832
alpha/coverage/threshold decisions** and **1,310,720 selector/delta/latch
observations**. All **ten compiled production faults** are rejected; compilation
failures do not count as rejection. **1,048,576 fog/blend command captures** cover
selected 16-bit/complement color patterns, both commands, all opcode aliases,
ignored upper fields, RDRAM and wrapping XBUS, and actual FullSync behavior.
Retained-memory framebuffer and atomic unsupported-profile checks pass.
Warning-as-error and UBSAN pass for these gates and perspective compositing.

State **VN64S026** appends **24 bytes**, totaling **8,478,976 bytes**, for fog,
blend and memory RGB. Reserved state bits reject atomically. S025 imports preserve
every preceding payload byte and initialize the new fields to zero; earlier
imports remain covered. State fixture offsets now address their original fields
through the appended tail. The full build, all **48 existing N64 regression
gates**, production TCP rollback, the new component/state/checkpoint gates and
all five partial-packet/texture/conversion/perspective/blender production replay
gates pass. Subsequent actual guest output and full machine state replay byte
for byte through the netplay interface, including current ABI capacity and
malformed-state rejection. All **108 repository-owned ROM archives** reproduce
and match their manifest. Plugin **0.34.0**, ABI v2 and S026 are current.

Repeat from the repository root:

```sh
ctest --test-dir build -R '^n64_rdp_(blender|one_cycle_blend)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_one_cycle_blend/` and
`build/artifacts/component/n64_blender/`. The private Mario probe was repeated
with current plugin/trace binaries at **200 million CPU attempts**, without a
terminal CPU fault. The complete **22/22-word CF000FFA0FFAF006 packet at
0x227160**, fetched CURRENT **0x227210**, reason 1 and uniform video remain.
Native boot/playability are unverified. Report:
`build/artifacts/retail/n64-blender.json` (ignored; no commercial ROM bytes
committed).

Depth/hidden RDRAM memory, LOD/mipmapping, two-cycle/previous-texel feedback,
dithering, complete devices/audio, physical timing and hardware-wide corpora
remain unfinished. The full Nintendo implementation goal remains active.
N64 stays **45% = 9/20 delivery milestones**; CI/CD remains disabled.

## Hidden RDRAM component foundation

The new [hidden RDRAM component](https://github.com/jshojan/veloce/blob/main/cores/n64/src/hidden_rdram.hpp)
retains one ninth bit per physical memory byte, packed as four two-bit halfword
entries per byte. An 8 MiB RAM profile uses a **1,048,576-byte bitmap**. Separate
state retains eight RDP history lanes and a pending delayed byte-write address.
Aligned CPU byte/halfword/word/doubleword metadata, settled halfword bursts,
MI EBUS scatter/gather layout, explicit color/depth metadata and both RDP byte
traversal directions have independent component tests. Depth metadata writes
preserve color history; delayed writes can be checkpointed and replayed. Invalid
widths, alignment, bounds and state fields reject without mutation.

The independent per-byte-bit oracle passes **16,777,216 packed-halfword checks**
across the complete 8 MiB address range, **1,048,576 CPU stores**, **245,760 EBUS
transactions**, **786,816 RDP history writes** and **39,168 settled bursts**.
All **thirteen separately compiled production faults** are rejected after
execution; compilation failures do not count as rejection. Byte-store address
and value parity are independently selected, and mixed halfword LSBs exercise
word/doubleword ordering. Warning-as-error and UBSAN pass. The optional
[pinned primary comparison](https://github.com/jshojan/veloce/tree/main/tests/reference/n64-hidden)
compiles unchanged ares and Angrylion sources fetched outside the repository;
it matches the CPU/EBUS/RDP domains above with zero mismatches. Operand/state
partners are finite and correlated; no exhaustive joint machine claim is made.

This is a **standalone component**. The production machine does not yet connect
CPU/cache/DMA stores, the MI EBUS enable latch or RDP framebuffers to this bitmap.
Machine snapshot **VN64S026**, plugin **0.34.0**, and the **108 owned ROM archives**
remain current. Component replay does not establish production netplay replay
for hidden memory. Masked-store metadata, cached EBUS freeze behavior, machine
integration/serialization, original guest hidden-bit observations, RGBA16
blending and depth compression/comparison/update remain pending. No new guest
ROM or retail compatibility is claimed by this increment.

Repeat the normal offline gates:

```sh
ctest --test-dir build -R '^n64_hidden_rdram' --output-on-failure
```

Mutation artifacts: `build/artifacts/component/n64_hidden_rdram/`.
The last private Mario result remains the
[one-cycle blending probe](https://github.com/jshojan/veloce/blob/main/cores/n64/README.md#one-cycle-blending-and-alpha-coverage):
200 million CPU attempts, complete 22/22-word blocked triangle, uniform video,
boot/playability unverified. No commercial ROM bytes are committed.
N64 stays **45% = 9/20 delivery milestones**; percentages remain in all core
READMEs and the front page. CI/CD remains disabled. The full Nintendo
implementation goal remains active.

## Shared CPU/DMA hidden RDRAM and EBUS observations

The production machine now owns the packed hidden RDRAM bitmap. Ordinary CPU
byte/halfword/word/doubleword stores, masked merge-store pieces, cache writebacks
and settled PI/SI/SP destination transfers update metadata. Uncached MI EBUS
reads gather ninth bits into the low nibble of each word; EBUS stores retain
ordinary RAM data while scattering hidden bits according to the bus width.
CPU merge stores use aligned pieces, grounded in the
[pinned primary instruction source](https://github.com/ares-emulator/ares/blob/9408cb43d4948fc3ea6e152a307a34348df3fe04/ares/n64/cpu/interpreter-ipu.cpp).

Cache fills/writebacks have distinct memory bus hooks. Cached hits retain their
ordinary data while EBUS is enabled. A cached bus transaction in that mode
rejects; the physical CPU freeze and bus timing are not modeled. Supported
uncached EBUS operations leave SP execution and device register access available.
DMA remains settled rather than cycle accurate. RDP color/coverage/depth writes
are **not yet connected** to hidden memory; RGBA16 blending and depth remain
pending. These tests establish CPU/DMA/EBUS metadata for a bounded profile.

The new original
[hidden RDRAM ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/hidden_rdram)
passes **65,536 cases / 589,824 actual RAM/register observations**. An independent
per-byte host oracle grades ordinary data and hidden reads after four store
widths in ordinary/EBUS modes. A helper executes from SP DMEM while EBUS is
active. The 256-case control passes; its separately executed capture mutant
fails every row. All nine field flips and seven stream controls reject. Fixed
addresses and correlated patterns leave joint hardware domains incomplete.

Machine gates additionally pass **205,248 CPU/masked-store captures**, selected
PI/SI/SP destination metadata checks, cached/uncached distinctions and atomic
state replay/rejection. Warning-as-error and UBSAN pass. The earlier component
oracle and thirteen compiled fault controls remain available.

State **VN64S027** appends **1,048,592 bytes**, totaling **9,527,568 bytes**: the
1 MiB bitmap, an eight-byte canonical history word and an eight-byte pending
address word. History reserved bits, invalid pending addresses and malformed
lengths reject before mutation. S026 and older imports preserve their complete
preceding payload, reconstruct hidden halfwords from saved ordinary RAM LSBs
and initialize history/pending state neutrally. Old states cannot recover hidden
information that they did not record. Plugin **0.35.0**, ABI v2 and S027 are
current; the production netplay capacity reports the new exact size.

The production checkpoint replays subsequent actual guest output and whole
machine state byte for byte. Existing state fixtures now address their original
fields through the appended tail. All **109 owned ROM archives** reproduce and
match their manifest.

```sh
ctest --test-dir build -R '^n64_hidden_' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_hidden_rdram/`. RDP hidden framebuffer and
depth integration, exact EBUS freeze/timing and hardware-wide corpora remain
unfinished. N64 stays **45% = 9/20 delivery milestones**. CI/CD remains disabled;
the full Nintendo implementation goal remains active.

The private Mario probe was repeated with current plugin/trace binaries at
**200 million CPU attempts**. It has no terminal CPU fault and retains the
complete **22/22-word CF000FFA0FFAF006 packet at 0x227160**, fetched CURRENT
**0x227210**, reason 1 and uniform video. Boot/playability remain unverified.
Report: `build/artifacts/retail/n64-hidden-memory.json` (ignored; no commercial
ROM bytes committed).

The full build and production TCP netplay gate pass. All 54 selected N64
regression/component/checkpoint gates pass after correcting the older MI guest
helper to execute enabled-EBUS windows from SP DMEM. Its unchanged independent
oracle still passes 393,216 register rows; executed/field/stream controls reject.
The corrected archive reproduces alongside the other 108 owned ROMs.

## Shared RDP hidden coverage and RGBA16 blending

RDP fill rectangles and native fill-cycle triangles now update shared hidden
RDRAM metadata. CI8 writes retain eight history lanes, follow the major-edge
traversal direction and flush delayed upper-bit writes at command completion.
RGBA16/32 fills replicate each halfword's data LSB into its hidden pair. Fill
bounds/unsupported profiles preflight before RAM or metadata mutation.

Functional one-cycle RGBA16 triangles now use the existing shade/texture,
combiner and blender path. Memory RGB reads zero-fill the lower three bits of
five-bit channels, as in the
[pinned RDP framebuffer source](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/fbuffer.c)
and [channel macros](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video.c).
This differs from the VI's display expansion. Memory coverage combines the
ordinary word's low bit with two hidden bits. RGBA16 output retains quantized
RGB and the coverage upper bit in RAM, and the lower coverage pair in hidden
memory. RGBA32 output keeps coverage in its alpha byte, stores the green LSB
pair and clears the second hidden pair. Color writes update retained history.

The new original
[RGBA16 hidden-coverage ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_hidden_rgba16)
passes **655,360 cases / 8,519,680 actual RAM/register observations**. Four pixel
pairs and four gathered hidden-metadata pairs are independently graded alongside
guards and DP/MI completion registers. Both command sources, every selected
RGB seed/alpha pair, four profiles and all 256 blender selector combinations
are covered with finite source partners. The 1,616-case control passes; its
separately executed mutant fails every row, and all thirteen field flips and
seven stream controls reject. Background coverage, geometry and shade are fixed;
full joint texture/geometry/depth and framebuffer-format domains remain pending.

Component gates pass **262,144 complete RGBA16 read encodings**, **1,048,576
paired framebuffer writes**, **196,608 rectangle fill metadata patterns** and
**393,216 clipped/oriented triangle metadata captures**. A per-byte metadata
oracle checks RAM, every hidden halfword and every retained lane, including a
final unpaired CI8 byte. All **nine compiled production faults** are rejected
after execution. Warning-as-error and UBSAN pass.

The machine passes the hidden bitmap into supported RDP paths. **VN64S027**,
**9,527,568 bytes**, ABI v2 and existing legacy imports remain current; plugin
**0.36.0** is current. The production checkpoint replays subsequent actual guest
output and whole state byte for byte. All **110 owned ROM archives** reproduce
and match the manifest.

```sh
ctest --test-dir build -R '^n64_rdp_(framebuffer|hidden)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_hidden_rgba16/` and
`build/artifacts/component/n64_rdp_framebuffer/`. Depth compression/comparison/
update, dithering, LOD/mipmapping, two-cycle feedback/blending, physical timing,
full graphics and hardware-wide corpora remain incomplete. N64 stays **45% =
9/20 delivery milestones**; CI/CD remains disabled and the full Nintendo goal
remains active.

The full build, all **56 selected N64 component/regression/checkpoint gates**,
production TCP rollback and the new framebuffer production checkpoint pass.
The private Mario probe reaches **200 million CPU attempts** without a terminal
CPU fault; the complete **22/22-word CF000FFA0FFAF006 packet at 0x227160**,
fetched CURRENT **0x227210**, reason 1 and uniform video remain. Native
boot/playability are unverified. Current binary hashes match the ignored report
`build/artifacts/retail/n64-hidden-framebuffer.json`. No commercial ROM bytes
are committed.

## Rejected color history and depth component foundation

The one-cycle path now retains RGBA16/32 color-write history even when alpha or
coverage rejects the ordinary framebuffer store. RGBA16 uses the finalized
coverage pair; RGBA32 uses the computed green LSB pair and a zero second pair.
RAM and current hidden storage remain untouched by these rejected writes.
Later even-byte CI8 fills consume that retained history. Blender RGB equations
are now evaluated for rejected pixels too; the accepted-store decision is
unchanged. This follows the primary
[rejected-write path](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/rasterizer.c).

The new original
[rejection-history ROM](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_rejected_history)
passes **655,360 cases / 8,519,680 actual RAM/register observations**. RGBA16
packets are followed by eight single-even-byte CI8 fills; the guest then captures
pixels, gathered hidden bits, guards and completion registers. Independent
quarter-grid/coverage/history arithmetic grades the stream. The 1,616-case
control passes, its separately executed mutant fails every row, and all thirteen
field flips and seven stream controls reject. Fixed background, geometry and
shade leave joint hardware domains incomplete. **4,096 component captures**
separately check rejected RGBA16/RGBA32 stores followed by CI8 history reads.
All **eleven compiled framebuffer faults** are rejected after execution.

A new standalone
[depth component](https://github.com/jshojan/veloce/blob/main/cores/n64/src/rdp_depth.hpp)
implements 18-bit compression, stored-depth decompression, native delta encoding,
precision adjustment, coplanar handling, four comparator modes, coverage scaling,
blend enable and current blender shifts. Paired depth stores split delta bits
across ordinary/hidden memory and preserve color-write history. **This component
is not yet connected to production triangle rendering.** Primitive depth,
interpixel shifter state, depth/blender sequencing and guest depth observations
remain pending; no new retail-depth compatibility is claimed.

The independent component oracle passes every 18-bit compression input and
all 16-bit stored/delta encodings, **19,136,512 selected depth decisions** and
**4,194,304 paired stores**. All **twelve compiled depth faults** are rejected;
compile failures do not count as rejection. The optional
[pinned primary comparison](https://github.com/jshojan/veloce/tree/main/tests/reference/n64-depth)
compiles unchanged depth code fetched outside the repository and matches all
encoding domains plus **16,777,216 enabled-comparator decisions** with zero
mismatches. Disabled comparison and stores are independently checked rather
than compared through the primary adapter. Incoming delta/position/coverage/
flag partners are selected, not an exhaustive joint comparator corpus.
Warning-as-error and UBSAN pass for depth and framebuffer components.

Plugin **0.37.0**, ABI v2, **VN64S027 / 9,527,568 bytes** and existing legacy
imports are current. The history changes use fields already serialized in S027.
All **111 owned ROM archives** reproduce and match their manifest. Production
checkpoint replay compares actual subsequent guest output and whole-state bytes.

```sh
ctest --test-dir build -R '^n64_rdp_(depth|framebuffer|rejected)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_rejected_history/`,
`build/artifacts/component/n64_rdp_depth/` and
`build/artifacts/component/n64_rdp_framebuffer/`. Production depth, dithering,
LOD/mipmapping, two-cycle execution, physical timing and full graphics remain
unfinished. N64 stays **45% = 9/20 delivery milestones**; CI/CD remains disabled
and the full Nintendo implementation goal remains active.

Final verification passes all **58 selected N64 regression/component/checkpoint
tests** and production TCP netplay. The private Super Mario 64 USA probe runs
2,000 quanta and 200 million CPU attempts without a terminal CPU fault. It still
stops on the complete 22-word `CF000FFA0FFAF006` triangle at `0x227160`
(reason 1), with uniform video. Retail boot and playability remain **unverified**.
The probe records hashes of the current core and trace binaries; its report is
`build/artifacts/retail/n64-depth-history.json`. Commercial ROM bytes are excluded.
