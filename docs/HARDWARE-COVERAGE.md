# Nintendo implementation and hardware test coverage

The goal includes Nintendo core plugins, repository-owned ROMs covering the
hardware, machine-readable register observations, netplay, seamless controllers,
and documentation. The full goal is incomplete. Existing test-subset scores
are not measurements of overall hardware or product completion.

## Implemented cores and original ROMs

| Core family | Plugin | Original hardware evidence | Full core completion |
|---|---|---|---|
| NES/Famicom | `nes_plugin` | ADC/SBC zero-page arithmetic: 524,288/524,288 A/P pairs verified (100% of both operands, C and D states) | Not measured; other CPU instructions/addressing/timing and peripheral corpora remain |
| GB/CGB | `gb_plugin` | DMG WRAM/ADD probe: 2/2 checks; byte ALU E-register corpus: 8,388,608/8,388,608 A/F pairs matched (100% of eight operations, every operand pair, and all 16 incoming flag states); unary accumulator instructions: 73,728/73,728 A/F pairs matched (18 instructions × all bytes × all flags) | Not measured; other CPU operands/instructions, timing/PPU/APU/MBC/CGB corpus remains |
| SNES | `snes_plugin` | Settled multiplication: 65,536/65,536 operand pairs verified (100%); division: 3,328 selected boundary cases verified; math timing: 504,832/504,832 register bytes matched at CPU phases 2–18 (100% of declared patterns/phases) | Not measured; timed full operand domain, refresh/DMA interactions, and remaining hardware corpora pending |
| GBA | `gba_plugin` | ARM MOVS register shifts: 65,536/65,536 result/CPSR pairs verified; all low-byte amounts and C/V inputs on 16 operands | Not measured; remaining CPU modes/instructions, timing and peripheral corpora pending |
| Nintendo 64 | `n64_plugin` (experimental homebrew only) | 28,127/28,127 defined integer results matched; 28,160 observations, with 33 undefined divide inputs unscored; doubleword multiply/divide: 262,623/262,623 defined LO/HI pairs matched on exhaustive byte pairs and full-width boundaries; merge memory: 3,158,016/3,158,016 observations matched at every lane on all repeated-byte pairs and selected full-width patterns; digital SI/PIF: 16,384/16,384 button combinations and 131,072 register values matched; interrupt gates: 65,536/65,536 cases and 524,288 register values matched | Not measured; retail boot, graphics/audio, analog/controllers beyond port 1 and most platform hardware remain |
| Virtual Boy | `vb_plugin` (experimental homebrew only) | 6,512,640/6,512,640 result/PSW pairs matched across ADD/SUB/CMP/OR/AND/XOR repeated-byte pairs and full-width boundaries, plus SHL/SHR/SAR patterns and all low-byte amounts; 4,210,688/4,210,688 multiply/divide snapshots matched with 8,704 divide-zero exception/RETI cases; 4,685,824/4,685,824 TRAP/illegal exception snapshots matched across all writable nonfatal PSW combinations and full reserved low-bit encodings; 4,982,784/4,982,784 address-trap snapshots matched across matching/disabled/aliased addresses and entry/return; 17,055,744/17,055,744 CAXI/port snapshots matched across comparisons/replacements, width/alignment, unsigned input and preserved flags; 14,680,064/14,680,064 arithmetic bit-string snapshots matched across eight operations, byte pairs, all offsets and twelve lengths; 100,712,448/100,712,448 immediate-instruction snapshots matched across all wide/short immediates for twelve instructions on sixteen patterns/all flags | Not measured; remaining CPU/NVC instructions, timing, VIP/VSU, controllers and most platform hardware remain |
| Nintendo DS | `nds_plugin` (experimental homebrew only) | 44,238,336/44,238,336 dual-CPU result/CPSR/source snapshots matched across all sixteen data-processing operations, repeated-byte pairs and full-width boundaries, every NZCV and both ARM9 Q states; 38,338,560 six-family multiply and 51,426,816 Thumb result/CPSR/source/PC observations matched with undefined ARM7 flags excluded; 49,694,208 nine-word ARM/Thumb memory and 9,437,184 eight-word long-call snapshots matched with every bit graded; 3,720,960 stack/block snapshots matched; 4,096/4,096 digital keypad combinations matched | Not measured; firmware/retail boot, ARM block transfers, interrupts, graphics/audio, peripherals, timing and DSi enhanced mode remain |

The experimental N64 plugin now executes original MIPS integer homebrew ROMs.
It accepts only the documented `V64HB001` entry profile and cannot run retail
images. Its corpus observes 28,160 GPR/HI/LO results across 20 instructions,
with 28,127/28,127 defined selected-pattern results matching an independent
oracle. The 33 undefined divide inputs are recorded but unscored. All low-byte
shift amounts are covered on 16 patterns per width; full operand domains and
full core completion are not measured. Pipeline/timing, TLB/caches/full CP0,
FPU, RSP/RDP, PIF/IPL/CIC boot, graphics/audio, analog and multiplayer controllers,
save media, remaining interrupt producers and timing, timed/peripheral DMA and N64 netplay remain. See [N64 implementation](../cores/n64/README.md)
and its [original corpus](../tests/roms-src/n64/integer_registers/README.md).

The original [N64 doubleword multiply/divide corpus](../tests/roms-src/n64/muldiv64/README.md)
now verifies DMULT/DMULTU/DDIV/DDIVU with every signed/unsigned byte pair
embedded in 64-bit operands, plus 16×16 full-width boundaries. All 262,623
defined LO/HI pairs matched; 545 undefined inputs were retained but unscored.
Full 64-bit operand enumeration, pipeline stalls and HI/LO hazards remain.

The original [N64 merge-memory corpus](../tests/roms-src/n64/merge_memory/README.md)
now verifies LWL/LWR/LDL/LDR/SWL/SWR/SDL/SDR at every byte lane. All
3,158,016 loaded-register/stored-memory observations matched the independent
byte-array oracle, covering every repeated-byte pair plus 16×16 full-width
patterns. Independent multi-byte operand combinations, transaction timing,
TLB/privilege/reverse-endian modes, MMIO partial writes and physical hardware
remain. Paired access/order and partial-state checks are separate unit evidence.

The original [N64 SI/PIF controller corpus](../tests/roms-src/n64/controller_serial/README.md)
matched every combination of 14 digital buttons: **16,384/16,384 cases**, with
131,072 observed guest values. It also checks controller identification,
absent ports, settled SI DMA, MI mask/pending and interrupt acknowledgement.
This does not measure asynchronous DMA, interrupt pipeline timing, analog sticks,
accessories, multiplayer routing or physical controllers. These remain pending.

The original [N64 interrupt corpus](../tests/roms-src/n64/interrupts/README.md)
matched **65,536/65,536 gates**, with 524,288 observed values for software,
SI and settled timer pending states, MI masking, all CPU interrupt masks and
all IE/EXL/ERL combinations. Component evidence separately covers 2,097,152
logical gates including injected external requests, BD and BEV. This establishes
logical delivery and exception state, not pipeline latency, CP0 hazards,
physical Count timing or missing interrupt producers.

The new [Virtual Boy plugin](../cores/vb/README.md) executes the original V810
[integer corpus](../tests/roms-src/vb/integer_registers/README.md) through frontend
discovery. All 6,512,640 pairs matched; this is 100% of the declared operand/flag
domains. The [multiply/divide corpus](../tests/roms-src/vb/muldiv/README.md)
matched all 4,210,688 nine-word snapshots, including 8,704 zero-divisor cases with
exception context and RETI return. Full 32-bit operands and whole-platform
completion remain unverified. The [exception corpus](../tests/roms-src/vb/exceptions/README.md)
matched all 4,685,824 snapshots across all TRAP parameters, three reserved opcode
families, every writable nonfatal PSW combination and full low-bit encoding
sweeps. Entry/return state is graded independently; physical timing remains
unverified. The [address-trap corpus](../tests/roms-src/vb/address_traps/README.md)
matched all 4,982,784 snapshots, including 131,168 matching enabled cases, all
writable nonfatal PSWs on six addresses, single-bit PC flips and all low-half
offsets. It verifies PC equality independently of board/storage aliases and
records entry and RETI retry context. The [memory transaction corpus](../tests/roms-src/vb/memory_ops/README.md)
matched all 17,055,744 snapshots for CAXI and all port widths; its marked storage
profile shares the memory decoder with ports. Physical port wiring, bus-lock
signals and arbitration/timing remain unverified. The [arithmetic bit-string corpus](../tests/roms-src/vb/bitstrings/README.md)
matched all 14,680,064 fourteen-word snapshots across eight operations, byte pairs,
all source/destination offsets and twelve boundary lengths. It grades memory,
working registers and preserved PSW; overlap, interrupt abort/resume and buffer
bus timing remain unverified. The [immediate corpus](../tests/roms-src/vb/immediates/README.md)
matched all 100,712,448 destination/PSW/source snapshots for all 16-bit and 5-bit
immediates of twelve instructions on sixteen full-width patterns/all low flags.
It executes changing code in WRAM; physical cache coherency and full operand
domains remain unverified. Video, sound,
controller hardware, interrupts/timing and retail compatibility remain pending.

The new [DS plugin](../cores/nds/README.md) loads original ARM9/ARM7 segments
and executes both CPUs against shared main RAM and CPU-specific WRAM. Its
[integer corpus](../tests/roms-src/nds/integer_registers/README.md) matched all
44,238,336 destination/CPSR/source snapshots across sixteen data-processing
operations, byte pairs/full-width boundaries, all NZCV and ARM9 Q states.
The [multiply corpus](../tests/roms-src/nds/multiply_registers/README.md) matched
38,338,560 five-word snapshots across MUL/MLA/UMULL/UMLAL/SMULL/SMLAL, S
clear/set, byte pairs, boundaries and four accumulator seeds. Defined bits are
all graded; ARM7 S-form unpredictable C or C/V bits are captured but unscored.
The [Thumb corpus](../tests/roms-src/nds/thumb_registers/README.md) matched
51,426,816/51,426,816 result/CPSR/source/Thumb-PC snapshots with zero mismatches over every low-register ALU
operation, register ADD/SUB and all shift and low register ALU immediates. Its reduced positive,
executed ADC-to-SBC negative, flag/PC controls and malformed-file checks passed;
ARM7 MUL C is unscored in 1,052,672 cases. High registers, branches and saved
Thumb continuation have separate component evidence.
The [memory corpus](../tests/roms-src/nds/memory_registers/README.md) adds
49,694,208/49,694,208 matched nine-word destination/CPSR/base/offset/source/memory/guard/SP snapshots
for ARM halfword/signed and Thumb transfers, with zero mismatches and no unscored bits. All
halfword values, byte values and immediate fields are included. Aligned words
and halfwords are graded; odd-half and unaligned Thumb word hardware quirks
remain unverified. Component byte oracles separately cover register aliases,
writeback, PC alignment, store widths and rejected transactions.
The [long-call corpus](../tests/roms-src/nds/long_calls/README.md) matched
9,437,184/9,437,184 eight-word snapshots, zero mismatches and no unscored bits.
Adjacent BL/ARM9 BLX pairs cover every high and valid low field individually,
both source halfword positions, sixteen LR seeds and all NZCV/ARM9 Q inputs.
The independent grader checks link, CPSR, target PC, inputs and an ARM-target
memory observation. Separately, components cover 20,971,515 defined combined
encoding pairs, excluding five unpredictable self-suffix targets; actual plugin
save/replay crosses a frame ending between halves. Dynamic RAM code patches
use the cache-free test profile and establish no physical cache coherence.
The [stack/block corpus](../tests/roms-src/nds/multiple_registers/README.md)
matched 3,720,960/3,720,960 twenty-five-word snapshots,
zero mismatches, across every defined PUSH/POP
list and STMIA/LDMIA list/base encoding, sixteen operand seeds, all NZCV/ARM9 Q
and POP-PC architecture differences. Every recorded register/memory bit is
graded; unpredictable lists are excluded. Components separately check all
list encodings including diagnostic stops, unaligned model behavior, injected
bus failures and actual plugin stack replay. Static instruction thunks avoid
runtime code patching. Physical abort/alignment/CP15/cache behavior and bus
ordering/timing remain unverified.
The [keypad corpus](../tests/roms-src/nds/keypad/README.md) matched all 4,096
digital combinations, zero mismatches, through the actual frontend schedule. Both CPUs capture
KEYINPUT halfword/byte reads and ARM7 captures X/Y through EXTKEYIN, with a
shared-RAM sequence handshake. Reserved/unknown register bits are captured but
unscored. Components separately check mapping, access rights, ignored writes,
normalized input state, reset and legacy migration; actual plugin layout and
held/released state tests pass. Lid/debug/touch/SPI, KEYCNT/keypad IRQ and
physical controller use remain unverified or unimplemented.
The [DS netplay probe](../tests/roms-src/nds/netplay_probe/README.md) passes
100 frames through the actual shared netplay plugin and two independent TCP
loopback peers: one host-owned keypad and a viewer. Both complete states equal
a baseline, and eight published guest words match a separate arithmetic oracle.
This covers one DS instance; DS wireless multiplayer remains pending. Audio is
empty, and physical timing, devices, Internet and cross-platform peers are unverified.
Native word serialization preserves the same captured fields and independent
grades; 347,136 reduced integer observations also match the original byte
transport exactly. Output ports are emulator-only and demonstrate no physical
bus timing.
This declares operand/flag coverage, not whole-hardware completion. Shifters,
WRAM banks and exact saved continuation have separate component checks.
Firmware, ARM block transfers, exceptions/interrupts, graphics/audio/touchscreen, physical controllers, most
peripherals/timing and DSi enhanced mode remain pending.

Run `python3 tests/roms-src/build.py --original-only --check` to reproduce the
original ROM images and verify their source and image SHA256 ledger. Run
`ctest --test-dir build -L owned --output-on-failure` to execute them. Raw
register files and reports remain in `build/artifacts/owned/`, and CI uploads
them as `owned-hardware-registers`.

The NES, GB ALU/unary, SNES, GBA, N64, Virtual Boy and DS corpora are graded by host integer oracles that read
every observed register result. The small GB WRAM probe emits its own `CHECK`
lines alongside observed values. The GB ALU and unary ROMs use the existing SB/SC start
observation hook without waiting for link delivery; physical serial timing and
retail boot are unverified.
No original suite scores a screenshot or stdout verdict. External ROM
suites still use their documented legacy detection paths; they have not all
been migrated to original ROMs or register-file scoring.

## Coverage still required

Every core needs explicit, reproducible domains and independent reference
expectations for:

- CPU instructions, addressing modes, flags, stack and bus behavior; exhaustive
  operand enumeration where finite domains are practical, plus specified
  boundaries for wide arithmetic and every supported architectural mode.
- Interrupt entry/exit, halt/stop/reset, priority, and each timing phase that can
  change an externally observed result.
- Memory maps, banks, mirrors, open bus, cartridge/save hardware, and every
  documented read/write register behavior.
- DMA transfers, overlap/priority, CPU stalls, audio transfers, and completion
  interrupts across start phases and lengths.
- Video register behavior, scanline/dot timing, memory access restrictions,
  sprites, blending, and rendering output as raw observed data in files.
- Audio clocks, envelopes, counters, FIFO/DSP behavior, and observed samples.
- Timers, serial/link peripherals, controller protocols, and platform-specific
  coprocessors or accessories.

ROM generators, expected-value oracles, committed binaries, coverage metadata,
and deliberate failure controls are required for each corpus. A new platform
must implement its hardware as a loadable core plugin and execute its own ROMs;
an empty plugin or declared platform name does not count as implementation.

Additional requested platform families without a core in this worktree include
DSi enhanced mode, GameCube/Wii, Nintendo 3DS, Wii U,
and Switch. Famicom Disk System and other Nintendo accessories still need
separate validation. There is no verified completion percentage for these
missing implementations.

The [SNES math timing corpus](../tests/roms-src/snes/math_timing/README.md) measures
intermediate registers and completion boundaries for selected operand patterns.
The core advances pending operations on CPU clock edges, and savestates retain
operand latches, shift state, and remaining clocks. The `VSNES002` internal state
format rejects older unversioned SNES payloads; recreate those states. The
plugin ABI remains v2. DMA/HDMA, DRAM refresh, hardware revisions, and physical
timing remain separate unverified domains.

## Modern feature evidence still required

The SDL virtual-controller test verifies two identical pads, duplicate events,
custom mappings, capture mode, disconnect/reconnect, saved identity restoration,
selection after removal, ambiguous devices, and axis endpoints. Physical devices,
Bluetooth, cross-platform drivers, and multiplayer routing still need testing.

The frontend has netplay interfaces/UI and the NES and DS cores expose `INetplayCapable`.
The [rollback module](../plugins/netplay_default/README.md) implements
bounded history, prediction, late-input restore/replay, stalls, and confirmation
validation. Two delayed actual NES sessions converge byte-for-byte on a 100-frame
baseline running an original controller ROM. Guest RAM observations and states
are retained in files; buffered/streaming replay audio is exercised in the test.
The IPv4 TCP transport also passes a two-process loopback test: both complete
NES states match the baseline after late-input replay, ROM mismatch is rejected,
and four fragmented malformed-message cases fail cleanly. Network reports,
guest registers, logs, and states are saved under `build/artifacts/network/`.
This establishes the tested local transport/replay scenarios, not whole-core
accuracy or Internet play.

The default plugin now uses both modules for direct IPv4 play. The frontend
polls while paused, lets the plugin own each live frame, and gates replay audio.
An additional two-process production-plugin test verifies complete state
equality, mismatch/malformed-message rejection, replay audio, and pause/disconnect
on a replaced core or externally advanced frame. Its register observations and
states are retained under `build/artifacts/netplay-plugin/`. Reset, ROM/core
replacement, and savestate loads disconnect before mutating the live core.
Rendered GUI operation, reconnects, desync detection/recovery, Windows, Internet
conditions, and physical two-machine play remain unverified. Relay, chat,
session codes, additional spectators, and input delay scheduling are not implemented.
Other core families need complete deterministic state and appropriate
multiplayer/link behavior before their netplay can be claimed as verified.

The corrected NES/GB/GBA CRC32 identities can change the identifier of a ROM
previously loaded with the mistyped table. Old savestate filenames and embedded
ROM identifiers may therefore stop matching; recreate those states. The core
state serialization formats for those three cores were unchanged by the CRC
correction. The new SNES `VSNES002` payload is a separate format change described
above.
