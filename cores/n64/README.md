# Experimental Nintendo 64 core plugin

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo 64 | **30%** | 6/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo 64 delivered milestones: Native digital controller/keypad registers consume frontend input, Parse a native cartridge/executable format (boot scored separately), Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Super Mario 64 loads but faults at RI initialization after six IPL3 instructions; retail boot, remaining CPU/FPU/devices, RSP/RDP/VI/AI, analog and netplay remain.
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
  memory. SP memory is storage only; no RSP execution or DMA exists yet.

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
Software Cause.IP0/IP1 are writable; SI and PI drive live IP2 after MI masking.
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
producers and reset/NMI/cache-error paths are unverified. The current `VN64S003` state stores Count, Compare, Cause, Status and pending SI/PI/MI state. Device levels are resampled on the next step.

## Explicit homebrew entry profile

Images marked `V64HB001` at normalized header offset `$34` use direct homebrew entry. They also
need the standard `$80371240` big-endian cartridge prefix, an aligned entry in
KSEG0/KSEG1 RAM at header offset `$08`, and a payload starting at ROM offset
`$1000`. The loader zeroes RAM, copies that payload to physical RAM `$400`, and
enters the header's address with zero GPRs/HI/LO and interrupts disabled in
kernel mode. This is a repository-defined test entry profile, **not PIF/IPL/CIC
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
cmake --build build --target veloce n64_plugin n64_machine_test n64_pi_test -j
ctest --test-dir build -R '^n64_' --output-on-failure
```

`VN64S003` states store explicit big-endian scalars, all GPR/CP0 registers,
HI/LO, both PCs, pending delay state, instruction/frame counters, completion,
RAM and SP storage, PIF RAM, SI address/completion, MI mask, latched digital
input and debug yield state, plus 13 PI registers, PI IRQ and boot mode (120 appended bytes). Legacy `VN64S002` states load only for the homebrew profile, with neutral PI state; `VN64S001` is rejected. ROM data is immutable and referenced by identity. Size,
version, ROM identity and basic field validation precede restoration. The
instruction counter counts CPU step attempts including interrupt entry,
**not actual VR4300 cycles**;
each frontend frame is a bounded 100,000-instruction scheduling quantum,
not an emulated VI field. The nominal metadata rate is NTSC/93.75 MHz only.

## Remaining hardware and modern features

PIF/IPL/CIC and retail boot, TLB/address modes, caches and pipeline timing,
full CP0, remaining interrupt producers and timing, LL/SC, traps,
FPU, RSP/vector instructions, RDP, VI, AI, RI and remaining PI DMA/timing, full SI/MI/PIF,
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
PI domain-1 timing values. This is an explicit HLE handoff; PIF ROM execution,
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
64 MiB formats and the loaded plugin. All nine N64 CTests pass; the component
also passes undefined-behavior sanitization.

## Local retail regression target

Super Mario 64 (USA), supplied locally by the user, was probed on 2026-10-10.
The plugin **loads it and attempts six IPL3 instructions**, then faults reading
the missing RI register `$A470000C`. EPC is `$A4000054`, Cause is `$10`,
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
playability verdict. Next work includes RI/RDRAM startup, full CPU/CP0/FPU,
RSP/RDP/VI/device scheduling, AI and save media.
