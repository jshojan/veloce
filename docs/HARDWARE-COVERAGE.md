# Nintendo implementation and hardware test coverage

Current N64 evidence: [One-cycle texture rectangles and Mario intro](#one-cycle-texture-rectangles-and-mario-intro). Earlier increment reports are historical.

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
| Nintendo 64 | `n64_plugin` (experimental homebrew/native handoff) | 28,127/28,127 defined integer results matched; 28,160 observations, with 33 undefined divide inputs unscored; doubleword multiply/divide: 262,623/262,623 defined LO/HI pairs matched on exhaustive byte pairs and full-width boundaries; merge memory: 3,158,016/3,158,016 observations matched at every lane on all repeated-byte pairs and selected full-width patterns; digital SI/PIF: 16,384/16,384 button combinations and 131,072 register values matched; interrupt gates: 65,536/65,536 cases and 524,288 register values matched; FPU controls: 8,192/8,192 cases and 65,536 register values matched | Not measured; retail boot, graphics/RSP audio microcode, analog/accessories/presence detection and most platform hardware remain |
| Virtual Boy | `vb_plugin` (experimental homebrew only) | 6,512,640/6,512,640 result/PSW pairs matched across ADD/SUB/CMP/OR/AND/XOR repeated-byte pairs and full-width boundaries, plus SHL/SHR/SAR patterns and all low-byte amounts; 4,210,688/4,210,688 multiply/divide snapshots matched with 8,704 divide-zero exception/RETI cases; 4,685,824/4,685,824 TRAP/illegal exception snapshots matched across all writable nonfatal PSW combinations and full reserved low-bit encodings; 4,982,784/4,982,784 address-trap snapshots matched across matching/disabled/aliased addresses and entry/return; 17,055,744/17,055,744 CAXI/port snapshots matched across comparisons/replacements, width/alignment, unsigned input and preserved flags; 14,680,064/14,680,064 arithmetic bit-string snapshots matched across eight operations, byte pairs, all offsets and twelve lengths; 100,712,448/100,712,448 immediate-instruction snapshots matched across all wide/short immediates for twelve instructions on sixteen patterns/all flags; 16,252,928 Nintendo-instruction snapshots across six instructions and the signed 17-bit multiply boundary | Not measured; remaining CPU/NVC instructions, timing, VIP/VSU, controllers and most platform hardware remain |
| Nintendo DS | `nds_plugin` (experimental homebrew only) | 44,238,336/44,238,336 dual-CPU result/CPSR/source snapshots matched across all sixteen data-processing operations, repeated-byte pairs and full-width boundaries, every NZCV and both ARM9 Q states; 38,338,560 six-family multiply and 51,426,816 Thumb result/CPSR/source/PC observations matched with undefined ARM7 flags excluded; 49,694,208 nine-word ARM/Thumb memory and 9,437,184 eight-word long-call snapshots matched with every bit graded; 3,720,960 stack/block snapshots matched; 4,096/4,096 digital keypad combinations matched | Not measured; firmware/retail boot, ARM block transfers, interrupts, graphics/audio, peripherals and timing remain; DSi has a separate partial plugin |
| Nintendo DSi | `dsi_plugin` (original marked images only) | 387,072/387,072 original RAM captures; 67,109,008 component byte/width observations; 100-frame TCP host/viewer states and eight guest words match oracle | Not measured; firmware/retail boot, full SCFG/MBK, physical timing and most console hardware remain; [limits](../cores/dsi/README.md) |
| Nintendo 3DS | `ctr_plugin` (original `.v3ds` only) | 25,165,824 original reversal captures; 36,123,648 component register snapshots and 541,065,300 RAM bus observations; 4,096 digital inputs; 100-frame production TCP host/viewer states and twelve guest words match baseline/oracle | Not measured; native boot, remaining CPU/peripherals, GPU/audio, physical timing and wireless multiplayer pending; [limits](../cores/3ds/README.md) |
| GameCube | `gc_plugin` (original marked images only) | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Not measured; most CPU and console hardware remain; [limits](../cores/ppc/README.md) |
| Wii U | `wiiu_plugin` (original `.vwiiu` only) | 25,165,824 original immediate captures; 28,311,552 component register snapshots and 8,724,152,428 RAM bus observations; dense/sparse replay and synthetic digital input; 100-frame production TCP states/twelve guest words match baseline/oracle | Not measured; native boot, remaining CPU/peripherals, GPU/audio, GamePad, physical timing and wireless multiplayer pending; [limits](../cores/wiiu/README.md) |
| Wii | `wii_plugin` (original marked images only) | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Not measured; most CPU and console hardware, including MEM2/IOS, remain; [limits](../cores/ppc/README.md) |
| Nintendo Switch | `switch_plugin` (uncompressed NSO0 integer profile) | 1,572,864 original observations / 7,864,320 actual register words; 13,877,248 independent component integer cases; full state/register-stream replay | Not measured; full CPU/MMU/HOS, firmware, compressed/hash-checked images, GPU/audio, devices/controllers, saves/netplay, timing and retail compatibility pending; [limits](../cores/switch/README.md) |

The experimental N64 plugin now executes original MIPS integer homebrew ROMs.
It accepts the documented homebrew and original/native IPL3 handoff profiles;
Super Mario 64 passes the former DIV.S, C.LT.S, DPC_STATUS, vector multiply and SET_COMBINE command-state blockers. The longer trace passes SET_Z_IMAGE and actual buffer clears, then reaches 200 million CPU attempts without a terminal CPU fault; shaded/textured/depth-tested triangles remain blocked, video stays uniform, and retail boot/playability remain unverified. Its corpus observes 28,160 GPR/HI/LO results across 20 instructions,
with 28,127/28,127 defined selected-pattern results matching an independent
oracle. The 33 undefined divide inputs are recorded but unscored. All low-byte
shift amounts are covered on 16 patterns per width; full operand domains and
full core completion are not measured. Pipeline/timing, remaining TLB/cache/full CP0 behavior,
FPU physical pipeline behavior, remaining RSP/RDP, PIF/IPL/CIC boot, VI filters and retail graphics/audio, analog and multiplayer controllers,
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
peripherals/timing remain pending. DSi now has a separate partial plugin.

Run `python3 tests/roms-src/build.py --original-only --check` to reproduce the
original ROM images and verify their source and image SHA256 ledger. Run
`ctest --test-dir build -L owned --output-on-failure` to execute them. Raw
register files and reports remain locally in `build/artifacts/owned/`.
CI/CD is disabled for now.

The NES, GB ALU/unary, SNES, GBA, N64, Virtual Boy, DS, GameCube and Wii corpora are graded by host integer oracles that read
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

Nintendo Switch now has an experimental original AArch64/NSO0 process core;
its native hardware implementation remains incomplete. Famicom Disk System and
other Nintendo accessories still need separate validation.

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

The frontend has netplay interfaces/UI and the NES, DS, DSi, 3DS and Wii U cores expose `INetplayCapable`.
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

## GameCube and Wii integer profiles

Both new plugins execute original PowerPC programs with big endian supplied RAM
and independent host arithmetic grading. Each 49,152-case corpus covers six
add/subtract operations, sixteen operand patterns squared, every incoming
SO/OV/CA state and both OE/Rc forms. Every bit of six captured words is graded.
The two profiles share CPU code; passing both does not establish physical
Gekko/Broadway differences. Component checks additionally enumerate every
arithmetic GPR field encoding and validate aliases, memory, branches, loader
rejection, exact state replay and dynamic plugin lifecycle. Original marked
images, synthetic output transport and direct payload startup are supplied test
conditions. Retail boot, graphics/audio, controllers, netplay and most remaining
hardware are pending; [profile details](../cores/ppc/README.md).

The branch corpora additionally match 74,752 snapshots per profile: 34,816 BC,
34,816 BCLR and 5,120 defined BCCTR cases with all BI fields and canonical BO
options, both LK states, CR/CTR boundaries and indirect low-bit alignment. The
seven captured words include branch marker, LR/CTR/CR/XER, actual PC anchor
and untouched GPR. Every captured bit is independently graded. Three executed
mutants must fail 256/144/128 reduced cases respectively; all field mutations
and seven malformed streams are rejected. Component checks match 4,227,072
branch snapshots across both profiles, including every displacement field,
absolute/relative and link states, wrap-around and old LR target semantics.
An original counted loop retains exact state across frame-boundary replay.
Noncanonical BO fields and newer BH semantics are outside these hardware
claims; prediction/timing and most platform hardware remain pending.

Each condition-register corpus matches 42,108,928 eight-word snapshots: signed
and unsigned register/immediate comparisons, eight CR logical operations,
MCRF and MCRXR. Every 16-bit compare immediate is tested on sixteen operand
patterns, all CR fields and both SO states; logical instructions cover every
bit-field encoding on four CR patterns/all XER flags. Host grading checks
results, preserved GPRs/CR fields, XER flags/byte count, stimulus encoding/seeds
and a sentinel. Runtime instruction generation uses the supplied cache-free
profile; it does not verify physical instruction-cache synchronization. The
component gate matches 101,941,248 comparison/condition snapshots across both
profiles, additionally forcing every defined logical truth input and alias,
all compare GPR fields and all MCRF source/destination nibbles. Known plugin
guest CR/XER captures and exact state replay, including MCRXR flag clearing,
are verified. Full 32-bit operands, all CR words, physical exceptions/timing
and the remaining console hardware remain pending. CI/CD stays disabled;
[manual corpus details](../tests/roms-src/gc/condition_registers/README.md).

### PowerPC integer logical coverage

Each profile additionally matches 6,324,224 original nine-word snapshots:
eight register logical families on sixteen operand patterns/all XER flags/both
Rc states, and all immediate values for six forms on eight operand patterns and
both SO states. The host grades result, recorded/preserved CR fields, unchanged
XER, sources, encoding, seeds and sentinel using separate byte truth tables.
Executed result/CR/XER mutants fail 512/192/512 reduced cases; nine field
mutations fail and seven invalid streams return ERROR. Component checks match
21,856,256 additional snapshots across both profiles, cover every register
encoding/alias and every bit's truth inputs, all immediate values and boundary
immediate register fields, and exact replay of a known guest loop through both
plugins. The corpus uses fixed RS/RA/RB; full encoding/alias checks are component
evidence, not original ROM evidence. No full 32-bit operand-pair, physical
cache/timing or retail compatibility claim follows; [declared domains](../tests/roms-src/gc/logical_registers/README.md).

### PowerPC rotate/mask coverage

Each profile additionally matches 9,437,184 original ten-word snapshots:
4,194,304 RLWIMI, 1,048,576 RLWINM and 4,194,304 RLWNM cases. All encoded
shifts and masks are covered on eight source patterns, four insert destinations,
four upper-register-count patterns and both SO/Rc states. The host grades the
result, CR/XER, sources, initial destination, instruction encoding/seeds and
sentinel using circular mask walks and arithmetic rotation. Executed
result/mask/insert controls fail 3,584/1,472/496 reduced snapshots; ten field
mutations fail and seven malformed streams return ERROR. Component checks
match 113,258,496 additional snapshots across both profiles, include every GPR
field/alias on boundary masks, walking source bits at every shift/mask, all XER
flags and boundary byte-count preservation, and exact replay of known captures
through both plugins. Original fixed-field ROM evidence is separate from
component alias enumeration. These finite domains do not exhaust 32-bit
operand pairs or physical cache/timing behavior; [domain details](../tests/roms-src/gc/rotate_registers/README.md).

### PowerPC shift/unary coverage

Each profile additionally matches 4,416,528 original nine-word snapshots:
196,608 register shifts, 8,192 SRAWI, 16,384 EXTSB, 4,194,304 EXTSH and 1,040
CNTLZW observations. All byte/halfword input bits and shift counts are covered
within declared upper/source patterns/all XER flags/both Rc states; leading-zero
results cover 0–32. The host grades result, CR/XER, sources, instruction/seeds
and sentinel using separate arithmetic and bit-length references. Executed
result/carry/count mutants fail 3,856/640/1,280 reduced snapshots; nine field
mutations fail and seven invalid streams return ERROR. The count mutant uses a
separate 3,072-case register-shift domain and preserves captured original inputs.
Component checks match 37,042,208 additional snapshots across both profiles,
include every GPR/count encoding and alias on boundaries, walking sources,
all XER byte counts and exact machine/plugin replay, including CA changing
from zero to one. Reserved unary RB fields reject without register mutation.
Full 32-bit operand/count pairs, CR/XER words and physical cache/timing/exception
behavior remain unverified; [declared domain](../tests/roms-src/gc/shift_registers/README.md).

### PowerPC carry arithmetic coverage

Each profile additionally matches 25,168,384 original nine-word snapshots:
2,560 NEG/ADDZE/ADDME/SUBFZE/SUBFME and 25,165,824 ADDIC/ADDIC./SUBFIC
observations. All immediates are covered within sixteen source patterns/all XER
flags; unary forms cover all OE/Rc states. Independent host mathematics and
unsigned carry/no-borrow inequalities grade result, CR/XER, sources, encoding,
seeds and sentinels. Executed result/stale-CA/cleared-SO mutants fail exactly
272/120/140 reduced snapshots; nine field mutations fail and seven invalid
streams return ERROR. Component checks match 56,083,456 additional snapshots
across both profiles, include every GPR encoding/alias, walking/complemented
sources, all XER byte counts and exact machine/plugin state replay before
NEGO. sets OV/SO while preserving CA. Nonzero reserved unary RB fields reject
before architectural mutation. Full 32-bit sources, CR/XER words, physical
cache/timing/exceptions and console completion remain unverified;
[domain details](../tests/roms-src/gc/carry_registers/README.md).

### PowerPC multiply/divide coverage

`gc_multiply_registers` and `wii_multiply_registers` each match 8,421,376
nine-word observations: 8,420,320 defined results and 1,056 undefined division
rows with defined flags/preserved fields checked. The
[declared domain](../tests/roms-src/gc/multiply_registers/README.md) covers
MULHW/MULHWU/MULLW/DIVW/DIVWU on sixteen operand patterns/all XER flags/OE/Rc
and MULLI on every 16-bit immediate. Independent arbitrary precision host
arithmetic masks undefined quotient and recorded CR0.LT/GT/EQ only. CR0.SO,
XER and preserved fields remain graded. Executed wrong-result/high-word/flags
controls fail exactly 328/32/54 reduced rows. Undefined result mutations pass;
defined SO/OV mutations on undefined rows each fail one row. Strict malformed
stream controls and nine capture-field mutations pass their rejection gates.

The component gate matches 26,443,776 snapshots across both profiles,
including 280,640 deterministic undefined-result profile fixtures. Its oracle
uses 16-bit product limbs and restoring division; register/alias/byte-count,
reserved encoding, known guest and state replay checks pass, including with
undefined-behavior sanitization. Raw files (~634 MiB/profile) and JSON reports
stay under `build/artifacts/owned/gc_products/` and `wii_products/`.
No full 32-bit operand-pair or physical cache/timing claim is made. CI/CD
remains disabled; run the local gates:

```sh
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_multiply_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

### PowerPC integer memory coverage

`gc_memory_registers` and `wii_memory_registers` each match 12,586,368 ten-word
observations through their production plugins. The
[declared domain](../tests/roms-src/gc/memory_registers/README.md) covers fourteen
load/store displacement forms at every aligned signed 16-bit displacement on
eight patterns/three aliases, plus eighteen indexed forms on eight offsets.
Update and algebraic halfword loads and byte-reversed transfers are included.
CR/XER seeds are selected per group, not crossed with every input. The host
uses independent byte arrays and endian/sign arithmetic to grade every bit of
result, base, index, CR/XER, target word, surrounding guards, instruction and
source. Executed result/sign/update mutants fail exactly 1,968/120/768 reduced
rows; ten field mutations and seven strict invalid streams are rejected.

The component gate matches 27,560,000 snapshots across both profiles:
26,356,500 valid transfers plus 1,203,500 invalid-form/access diagnostics.
Complete GPR arrays, PC/count, special registers and sixteen neighboring bytes
are checked with a separate byte oracle. Register fields/aliases, ordinary
RB=0, special RA=0, reserved Rc and update restrictions are covered. A known
mixed-transfer guest and exact replay before STWUX's RAM/base update pass.
The full component gate passes undefined-behavior sanitization. Raw files
(~1.02 GiB/profile) and JSON reports stay in `build/artifacts/owned/gc_memory/`
and `wii_memory/`. Physical cache/MMU, exceptions and timing remain pending.
CI/CD stays disabled; use local gates:

```sh
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_memory_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

### DSi plugin and original RAM evidence

The [DSi plugin](../cores/dsi/README.md) is now separately loadable, sharing the
DS ARM interpreter with a distinct loader/state/controller identity and 16 MiB
RAM profile. `dsi_ram_registers` matches 387,072 fourteen-word original captures:
three RAM-limit modes, four banks, either writing CPU, three store/read aliases,
all aligned width/lane pairs and 256 byte/complement patterns. An independent
host byte-array oracle grades every field. Executed bank/width/mode mutants
fail exactly 1,512/1,038/4,032 reduced rows; fourteen field mutations and seven
strict invalid streams are rejected. Raw output (~44 MiB) and reports stay in
`build/artifacts/owned/dsi_ram/`.

`dsi_machine_test` matches 67,109,008 byte/width bus observations, writing each
supplied RAM byte once with an address-derived value and reading three mirrors
through the other CPU. Boundary widths, RAM-limit controls/locks, high-RAM
code, atomic loader/state rejection, DS/DSi coexistence and exact replay pass.
The component gate also passes undefined-behavior sanitization. These are
supplied-profile byte checks, not physical chip-control/timing evidence.

`dsi_netplay_plugin_test` passes production TCP rollback with two independent
100-frame peer states matching the 16,875,896-byte solo baseline and eight
guest words matching the independent oracle. Host owns one keypad; viewer
replay is supported, DSi wireless multiplayer is pending. DS component and
production netplay regression gates pass. Local TCP gates require loopback
socket permission; CI/CD remains disabled. All 64 original ROMs reproduce
against the source/image ledger. Firmware/retail boot, full SCFG/MBK and most
DSi hardware remain pending; full platform completion is unmeasured.

```sh
ctest --test-dir build -R '^(dsi_|nds_machine_test|nds_netplay_plugin_test)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

### 3DS ARM11/ARM9 integer plugin evidence

The distinct [3DS plugin](../cores/3ds/README.md) now executes original integer
programs on two ARM11 contexts and one ARM9, with 128 MiB FCRAM, 512 KiB AXI
RAM and 1 MiB ARM9 private RAM. The original reversal ROM matches
25,165,824 seven-word captures across both ARM11 contexts, ARM/Thumb forms,
REV/REV16/REVSH, all NZCVQ combinations and all complemented-half inputs.
An independent host byte oracle checks every field, with executed negative
controls and strict stream rejection. Thumb CPSR capture follows BX back to
ARM; direct component checks cover T preservation.

The component gate matches 36,123,648 register snapshots and 541,065,300 bus
observations, covering aliases/conditions/selected GE and full-width patterns,
all supplied RAM bytes with one chosen pattern, permitted sharing/ownership,
boundary atomicity, 4,096 digital HID combinations, loader/state rejection,
a three-context guest and exact full-state replay. The original fixed-field
ROM and component alias enumeration are separate evidence. Physical timing,
MMU/cache/coherency, firmware/native boot, GPU/audio, most peripherals and
3DS wireless/link multiplayer remain pending. Switch still lacks a plugin. No overall
completion percentage follows from these finite domains. Raw output and
JSON grades stay in `build/artifacts/owned/3ds_reversal/`; CI/CD stays disabled.

### 3DS host/viewer netplay evidence

The [3DS netplay probe](../tests/roms-src/3ds/netplay_probe/README.md) now passes
production IPv4 TCP rollback. Both independent peers match the 100-frame solo
baseline's complete 135,791,128-byte state; twelve guest words from ARM11_0,
ARM11_1 and ARM9 match the independent host arithmetic oracle. Executed input,
field and stream controls fail; ROM/transport and lifecycle rejection checks
pass. Fast states write directly into caller buffers and preserve canonical
bytes/full-state hash, including the far end of each RAM region. Component
and undefined-behavior sanitizer checks pass. NES/DS/DSi production network
regressions remain separate gates.

The host owns one HID keypad; viewer corrections replay that one instance.
Physical 3DS wireless/link multiplayer, Internet/cross-platform sessions,
compression/page sharing and sustained real-time performance remain pending.
A window-8 ring uses about 1.14 GiB per peer plus core/scratch memory. Reports,
register files and full states stay in `build/artifacts/netplay-3ds/`.
All 64 original ROMs reproduce from their source/image ledger. CI/CD remains disabled.

### Wii U original integer and full mapped-RAM evidence

The separate [Wii U plugin](../cores/wiiu/README.md) now executes original
three-context PowerPC programs, sharing the instruction module with GC/Wii
through an explicit bus interface. GC/Wii loader/maps/state remain intact.
The full 32 MiB MEM1/2 GiB MEM2 physical profile uses sparse 4 KiB pages;
missing pages represent zero bytes, without shrinking mapped address ranges.
Canonical states omit zero pages and retain every nonzero byte.

The original ADDI/ADDIS ROM matches 25,165,824 nine-word register captures:
all three contexts, full 16-bit immediates, four source values, zero/ordinary
bases and all incoming XER flag combinations. Independent host arithmetic
checks result, preserved flags and every captured stimulus/tag. Executed
result/width/CR controls, nine field mutations and seven invalid streams fail.
The component gate matches 28,311,552 register snapshots and 8,724,152,428 RAM
bus observations, with all RT/RA aliases on selected values and every RAM byte
written once/read through three contexts. Full dense and sparse replay, page
boundaries, atomic rejection, high-end code, a synthetic 4,096-value digital
input latch and plugin lifecycle pass. The full component gate passes
undefined-behavior sanitization and GC/Wii regression passes. These are finite supplied-profile
checks; full 32-bit operands, cache/MMU/coherency and physical timing are not
proved. Raw files/grades stay in `build/artifacts/owned/wiiu_immediates/`.

Firmware/native formats, Starbuck/IOSU/Cafe, remaining CPU instructions,
Latte/GX2 GPU, DSP/audio, native GamePad/KPAD/VPAD, storage, native wireless multiplayer
and most platform hardware remain pending. Switch still lacks a plugin.
All 64 original ROMs reproduce from the ledger. No overall hardware percentage
is inferred. CI/CD remains disabled; use the local gates in the core README.

### Wii U sparse-state host/viewer netplay

The Wii U core now supports production host/viewer replay for original programs.
Both independent TCP peers match the 100-frame solo canonical state byte-for-byte,
and twelve guest words—including high MEM2 readbacks—match an independent host
arithmetic oracle. Input-dependent far-page clearing/growth changes the sparse
serialized size; the final state is 17,392 bytes while retaining the complete
32 MiB MEM1/2 GiB MEM2 logical map. The original input/field/stream controls and
ROM/transport/lifecycle rejection checks pass. Current fast states preserve the
previous format; a 100-frame snapshot matches the previous serializer exactly.

Rollback and frontend snapshot buffers now use the current size query, growing
slots as needed within the stable maximum. Variable-state tests exercise delayed
and reordered growth/shrinkage, exact state/hash convergence and query/write/
allocation failure. High-water slot capacities are retained; dense workloads
still cost large states. Native GamePad/VPAD/KPAD, wireless multiplayer, physical
devices/two machines, Internet/cross-platform and sustained performance remain
unverified or pending. Reports are in `build/artifacts/netplay-wiiu/`.
The full Wii U component gate passes undefined-behavior sanitization; all
seven netplay gates pass. CI/CD stays disabled; all 64 original ROMs reproduce
against the ledger.

## GameCube/Wii settled digital controller gates

`gc_controller_serial` and `wii_controller_serial` each independently grade
4,096/4,096 digital combinations, sixteen guest register words per row. The
actual frontend input schedule and loaded core execute repository-owned ROMs.
One executed command mutation, sixteen field mutations and seven malformed
stream controls are rejected per platform. `ppc_serial_test` matches 262,144
selected-port/input/connection snapshots and 196,608 transfer-length snapshots,
checks receive preservation/errors, W1C, invalid accesses, legacy/current
states and actual plugin input replay. `ppc_machine_test` continues to pass.
These gates passed with undefined-behavior sanitization for the component code.

```bash
ctest --test-dir build -R '^(ppc_(serial|machine)_test|(gc|wii)_controller_serial|input_hotplug_test)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
python3 tests/update_completion.py --check
```

The ledger now includes 64 original ROMs. Controller polling, analog axes,
origin calibration, VI/wire timing, CPU SI interrupts, accessories, four-port
frontend input and Wii Remote support remain pending. CI/CD remains disabled.
The [delivery completion ledger](../docs/CORE-COMPLETION.md) is separate from
hardware-domain pass fractions and legacy external-suite accuracy scores.
The optional `tests/n64_retail_probe.py` takes a locally supplied N64 ROM;
Super Mario 64 now loads and reaches 5,897,464 IPL3 step attempts, then faults at the unimplemented VI Current register; boot/playability remain unverified. Its ROM bytes never enter git.

## N64 cartridge DMA and IPL3 handoff

The [PI corpus](../tests/roms-src/n64/pi_dma/README.md) matches 524,288 cases and
8,388,608 guest register values across all even lengths 2–65,536, four cartridge
offsets, two RDRAM addresses and both PI mask states. The [handoff corpus](../tests/roms-src/n64/ipl3_handoff/README.md)
matches twelve startup captures without bundling firmware. Executed guest mutants,
all captured-field mutants and seven invalid streams per corpus are rejected.
All twenty-four N64 CTests pass; the PI component also passes undefined-behavior sanitization.
Settled aligned ROM reads and explicit HLE startup are implemented; RI/RDRAM
initialization, native retail boot, PI busy/misalignment/peripheral behavior,
graphics/audio and physical timing remain pending. The ledger contains 64 original
ROMs. CI/CD remains disabled.

## N64 RI configuration register evidence

The [original RI corpus](../tests/roms-src/n64/ri_registers/README.md) matches
524,288 cases and 3,145,728 guest reads. Every 19-bit refresh value and every
mode/configuration/selection combination is covered; latency is correlated.
Executed selection mutation, six field mutants and seven invalid streams reject.
The component checks current-load latching, access/state failure atomicity,
legacy conversion, exact replay, reset and loaded-plugin execution. RI refresh
scheduling, analog calibration, bus-error production, native RAM remapping and
physical timing remain pending. Super Mario 64 advances to an unimplemented register read
(unimplemented VI Current register 803275F4, EPC 803275F4, Cause 1C) after 5,897,464 startup step attempts; it is
not bootable/playable. The corpus ledger contains 64 original ROMs.

## N64 MI control evidence

The [original MI corpus](../tests/roms-src/n64/mi_controls/README.md) matches
393,216 rows / 3,145,728 guest captures: mode command words across all eight
flag states and interrupt-mask commands across all 64 masks, with actual SI/PI
DMA requests pending. Executed count mutation, eight field mutants and seven
invalid streams reject. The component checks 16,777,216 mode transitions, all
mask transitions, injected DP delivery/acknowledgment/ERET, exact replay, legacy
conversion, atomic rejection and production-plugin execution; it also passes
undefined-behavior sanitization. All twenty-four N64 gates pass. MI repeat/upper device-register effects exist; RAM-repeat/EBus transactions
and native RAM remapping remain pending.
Super Mario 64 stops at unimplemented VI Current register (EPC 803275F4, Cause 1C) after 5,897,464 startup
step attempts; retail gameplay, graphics/audio and physical timing remain pending.
All 64 original ROMs reproduce. CI/CD remains disabled.

## N64 RDRAM device evidence

The [original device corpus](../tests/roms-src/n64/rdram_devices/README.md) matches
14,336 cases / 86,016 guest values across four devices, all writable delay
fields and repeated-byte storage patterns. Executed mutation, six field mutants
and seven invalid streams reject. Components check all device ID encodings,
delay/repeat combinations, selectors/register aliases, discovery, MI upper/repeat,
state replay and rejection, legacy conversion and loaded-plugin execution.
All twenty-four N64 gates pass; device components pass undefined-behavior sanitization.
Native RAM remapping, analog calibration, cache timing/errors, physical bus timing, repeat RAM
and EBus memory remain pending. Super Mario 64 reaches unimplemented VI Current register after
5,897,464 step attempts (EPC 803275F4, Cause 1C); gameplay/video/audio are unverified.
All 64 original ROMs reproduce. CI/CD stays disabled.

## N64 functional primary cache evidence

The [original cache corpus](../tests/roms-src/n64/cache_coherence/README.md) matches
2,097,152 cases / 12,582,912 guest values across every D-cache index, byte lane
and initial byte, with cached/uncached alias mutation and dirty writeback. An
executed writeback mutation, six field mutants and seven invalid streams reject.
Components additionally cover all physical tag values, instruction staleness/refill,
dirty eviction, widths/merged stores, PI DMA staleness, CACHE operations/privilege,
exact dirty/stale state replay and legacy conversion. All twenty-four N64 gates
pass; cache components pass undefined-behavior sanitization. Timing/hazards,
TLB/address modes, cache errors, native RAM remapping and device bus transactions
remain incomplete. Super Mario 64 stops at unimplemented VI Current register 803275F4 after 5,897,464 attempts
(EPC 803275F4, Cause 1C); video/audio and gameplay remain unverified. All 64
original ROMs reproduce; CI/CD remains disabled.


## N64 SP interface evidence

The [original SP controls](../tests/roms-src/n64/sp_controls/README.md) match
196,608 cases / 1,572,864 guest values over every writable status/interrupt
seed, all twelve command pairs and four command states. The
[original SP DMA](../tests/roms-src/n64/sp_dma/README.md) matches 65,536 cases /
393,216 guest values across every raw length, both directions/banks and four
wrap boundaries. Executed mutations, all field mutations and seven malformed
streams reject per corpus. Component evidence checks full DMA bytes/guards,
262,144 count/stride transfers, PC/semaphore masks, CPU interrupt delivery/ERET,
cache staleness, states, reset and the loaded shared plugin. SP components pass
undefined-behavior sanitization; all twenty-four N64 CTests pass. All 64 original
ROMs reproduce. CI/CD stays disabled.

RSP scalar/vector execution, DMA queues/clocks, unaligned stride, open-bus DMA,
IO-full and exact address/length readback physical validation remain pending.
Super Mario 64 reaches unimplemented VI Current register `$803275F4` after 5,897,464 startup
attempts (EPC `$803275F4`, Cause `$1C` hex). Gameplay/video/audio remain
unverified. N64 is 35% on the delivery checklist; no complete CPU/device
milestone is claimed.


## N64 AI audio evidence

The [FIFO ROM](../tests/roms-src/n64/ai_fifo/README.md) matches 2,048 cases /
22,528 guest values. The [PCM ROM](../tests/roms-src/n64/ai_pcm/README.md)
matches nine guest FIFO/IRQ captures; the production plugin outputs 66,169 stereo
pairs covering all 65,536 signed sample values per channel to register files.
Independent host oracles reject executed mutations and malformed streams.
Components cover all length/address/rate fields, dividers, two-buffer FIFO,
start/promotion IRQs, delayed carry, playback-time RAM/cache visibility, pause,
IRQ/ERET, bounded host output, state replay/rejection and identical buffered/
streaming samples and states. The AI component passes undefined-behavior
sanitization. All twenty-four N64 gates pass; all 64 original ROMs reproduce.

N64 now has functional guest PCM output and **35%** delivery-checklist completion.
Physical CPU/DAC timing, serial/analog behavior, status clock fields, dynamic rate
phase behavior, RSP microcode, graphics and native retail playability remain
unverified. Super Mario 64 reaches unimplemented VI Current register after 5,897,464 attempts
(EPC 803275F4, backing word 8D8D0010, Cause 1C hex). No commercial ROM bytes enter
git. CI/CD stays disabled.

## N64 FPU controls and branch corpus (2026-10-10)

CFC1/CTC1 and BC1F/BC1T/BC1FL/BC1TL now implement the defined VR4300
control-register profile. CU1-disabled instructions raise coprocessor unusable
with CE=1. CTC1 commits FCR31 before raising enabled cause or unimplemented
operation FPE. Read-only FCR0 selects implementation 0x0B/revision zero.

The [original control ROM](../tests/roms-src/n64/fpu_controls/README.md)
matches **8,192/8,192** cases / **65,536** guest register values, with actual
guest exception capture. An executed mutation fails every reduced row; eight
field mutations fail and seven incomplete/malformed streams are rejected.
The component covers **1,048,576** defined control combinations,
**524,288** branch offset/condition/kind combinations and **1,024** CU1-disabled
format/control-index cases, plus GPR transfers, delay/BEV/nested exception
context, pending-exception replay and atomic state rejection. S010 appends an
8-byte FCR31 scalar; S009 converts with neutral FCR31 and preserves pending
AI/FIFO/host phase state. Earlier state conversions remain supported.

All **65** repository-owned ROMs reproduce from the ledger. The FPU component
passes undefined-behavior sanitization. Raw register streams and independent
reports are under `build/artifacts/owned/n64_fpu_controls/`.
N64 stays **35%** on the delivery checklist: FPR data transfers, arithmetic,
conversions, comparisons, NaN/denormal behavior and physical pipeline timing
remain pending; no full CPU or hardware-accuracy milestone is awarded.
This original cartridge has not been validated on physical hardware.

Super Mario 64 now passes CFC1 and reaches unimplemented VI Current register after
**5,897,464** attempts (EPC `803275F4`, instruction-cache and backing word
`8D8D0010`, Cause `1C` hex). The production plugin still reports uniform video
and zero retail PCM pairs. Boot and playability remain unverified. No commercial
ROM bytes enter the repository. CI/CD remains disabled.

All **26/26 N64 CTests pass** (345.82 seconds); the complete run includes every
original N64 register ROM, PCM ABI oracle and all nine component tests.

## N64 TLB translation and fault corpus (2026-10-10)

TLBR/TLBWI/TLBWR/TLBP now control a real 32-entry paired-page TLB. The CPU
translates fetch/load/store/merge/CACHE access with seven page sizes,
ASID/global/valid/dirty checks, 32/64-bit region privileges, ERL bypass, xkphys
CCA, masked CP0 fields and DMFC0/DMTC0. Translation faults record
BadVAddr/EntryHi/Context/XContext and use refill/general/extended vectors.
Virtually indexed cache aliases retain separate stale data until maintenance.

The [original ROM](../tests/roms-src/n64/tlb_translation/README.md) matches
**114,688/114,688** cases / **1,261,568** guest register values. An executed
ASID mutation fails all 256 reduced rows; eleven field mutations fail; seven
malformed streams are rejected. The component checks **3,670,016** ASID/global/
half cases, **2,097,152** physical frame selections, **79,188** offsets and
**4,096** mask inputs and **229,376** independent permission/CCA cases, plus
controls, CPU faults/fetch/cache aliases and state
continuation. S011 stores 32 entries in 1,024 bytes and accepts S010 with neutral
TLB entries. Old state conversions preserve their existing fields.

Duplicate-hit shutdown/TS behavior, physical timing and CP0/TLB hazards remain
pending. Neutral reset tags, rejection of undefined masks and RI on duplicate
translation matches are explicit emulator conventions. Unsupported physical
bus access produces IBE/DBE in this bus profile; physical open-bus behavior is
unverified. This cartridge has not been tested on a physical console.
N64 remains **35%** on the delivery checklist; full hardware completion is unknown.

Mario 64 now passes TLBWI and reaches unimplemented VI Current at `A4400010`
after **5,897,464** attempts (EPC `803275F4`, actual decoded/backing word
`8D8D0010`, Cause `1C` hex / DBE). The tracer distinguishes actual CPU exception
entry from software EXL writes and permits nonterminal guest-handled exception
classes. The production probe still sees uniform video and zero retail PCM;
boot/playability remain unverified. Commercial bytes are kept out of git.

All **28/28 N64 CTests pass** (328.64 seconds), including all original N64
register ROMs, PCM ABI oracle and ten component tests. The final DMCP0
32-bit-mode restriction and independent permission checks are verified by the
affected TLB gates and the updated component sanitizer run. All **66** original
ROMs reproduce from their source/image ledger. CI/CD remains disabled.

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

The [register ROM](../tests/roms-src/n64/vi_registers/README.md) matches **65,536
rows / 983,040 guest values**. The [framebuffer ROM](../tests/roms-src/n64/vi_framebuffer/README.md)
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

The [original register ROM](../tests/roms-src/n64/fpu_data/README.md) matches
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

The [integer ROM](../tests/roms-src/n64/fpu_integer_conversions/README.md) matches
**524,288 rows / 2,621,440 captured words**. The
[edge ROM](../tests/roms-src/n64/fpu_conversion_edges/README.md) matches
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

The [pair ROM](../tests/roms-src/n64/fpu_arithmetic_pairs/README.md) matches
**131,072 rows / 786,432 captured words**. The
[edge ROM](../tests/roms-src/n64/fpu_arithmetic_edges/README.md) matches
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

The [ALU cartridge](../tests/roms-src/n64/rsp_scalar_alu/README.md) matches
**704,512 rows / 4,931,584 actual captured words**, including all repeated-byte
pairs for ten functions and all fixed/variable shifts on 256 patterns. The
[immediate cartridge](../tests/roms-src/n64/rsp_scalar_immediates/README.md) matches
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

The [register cartridge](../tests/roms-src/n64/dp_registers/README.md)
matches **262,144 rows / 2,621,440 captured words**, using an original RSP
program to observe both control views. The [fill cartridge](../tests/roms-src/n64/dp_fill_rectangles/README.md)
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
| [Vector loads](../tests/roms-src/n64/rsp_vector_loads/README.md) | 786,432 | 19,660,800 | Twelve forms × 16 byte elements × 16 alignments × 256 byte-pattern seeds |
| [Vector stores](../tests/roms-src/n64/rsp_vector_stores/README.md) | 786,432 | 25,952,256 | Same domain, including every byte of surrounding wrapped memory |
| [Scalar/vector transfers](../tests/roms-src/n64/rsp_vector_transfers/README.md) | 1,048,576 | 10,485,760 | Every halfword payload × every byte element, with scalar zero and flag masks/signs |

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
required by each instruction. VMUDM/VMADM multiply signed VS by unsigned VT;
VMUDN/VMADN multiply unsigned VS by signed VT. This operand order and the
published fixture transcription were corrected in the mixed-product increment. Source lanes are captured before
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
| [Multiply pairs](../tests/roms-src/n64/rsp_multiply_pairs/README.md) | 917,504 | 16,515,072 | Seven load-multiply functions × 16 selectors × 256 repeated-byte VS values × 32 groups of eight repeated-byte VT values |
| [Accumulate edges](../tests/roms-src/n64/rsp_accumulate_edges/README.md) | 294,912 | 7,372,800 | Nine accumulate/round functions × 8,192 groups of eight raw halfword values × four partner/48-bit accumulator patterns; selectors distributed through the groups |

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
| [Comparison pairs](../tests/roms-src/n64/fpu_comparison_pairs/README.md) | 262,144 | 1,835,008 | S/D × all sixteen predicates × all -32..31 integer pairs × both old condition bits |
| [Comparison edges](../tests/roms-src/n64/fpu_comparison_edges/README.md) | 348,384 | 2,438,688 | Every exponent with cycling fraction patterns; special/sign pairs, full RM/enable/FS/condition products on ten profiles and reserved formats |

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
| [Comparison pairs](../tests/roms-src/n64/rsp_comparison_pairs/README.md) | 262,144 | 4,718,592 | 256 repeated-byte VS values × 32 groups of eight repeated-byte VT lanes × four incoming flag profiles × eight functions |
| [Comparison edges](../tests/roms-src/n64/rsp_comparison_edges/README.md) | 262,144 | 4,718,592 | 8,192 groups of consecutive VS halfwords × boundary VT lanes × four incoming flag profiles × eight functions |

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
| [Integer pairs](../tests/roms-src/n64/rsp_integer_pairs/README.md) | 2,883,584 | 51,904,512 | Every repeated-byte operand pair × all 32 incoming lane flags × eleven functions; identity selection |
| [Integer edges](../tests/roms-src/n64/rsp_integer_edges/README.md) | 2,883,584 | 51,904,512 | Every VS halfword × all 32 incoming lane flags × eleven functions; rotating selectors and selected boundary VT lanes |

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

The original [divider/move ROM](../tests/roms-src/n64/rsp_divider/README.md)
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

The [original tile command ROM](../tests/roms-src/n64/rdp_tile_commands/README.md)
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
The [repeatable reference harness](../tests/reference/n64-tmem/README.md) records the exact domains.
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

The [original transfer-command ROM](../tests/roms-src/n64/rdp_tmem_commands/README.md)
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

The [internal arithmetic kernel](../cores/n64/COMBINER-MATH.md) now implements special nine-bit
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

The [original guest](../tests/roms-src/n64/controller_netplay/README.md)
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

## Switch AArch64 and native executable profile

The new emulator ABI v2 [Switch plugin](../cores/switch/README.md) executes an
[original NSO0 guest](../tests/roms-src/switch/arithmetic_registers/README.md).
**1,572,864 arithmetic observations / 7,864,320 actual register words** match
an independent unbounded-integer oracle. Its executed mutant fails all 384 short
cases; five field and seven malformed-stream controls reject. The component
passes **13,877,248 integer cases**, all 256 condition/NZCV combinations, SP/XZR,
loader/map guards and complete state replay. Counts cover explicitly stated
finite domains; they do not establish full CPU/hardware accuracy.

The loader accepts a checked uncompressed NSO0 subset, rejects compression/hash
flags, and maps text/RO/data/BSS with permissions into a fixed 4 MiB process
profile and test stack. `VNXS0001` captures **4,194,648 bytes**, pinned to the
image. Production ABI replay checks actual register streams and complete states.
Firmware/HOS, compressed/hash-checked images and other formats, MMU/native RAM,
full CPU/extensions/exceptions, GPU/audio, native devices/controllers, persistent
saves, netplay, timing and retail playability remain pending. Original bytes
are MIT; no firmware/keys/SDK/commercial files are included. Switch is now
**25% = 5/20 delivery milestones**. CI/CD remains disabled; the full Nintendo
implementation goal remains active.


## AArch64 multiply and divide

Plugin **0.2.0**, emulator ABI v2, now implements MADD/MSUB in both widths,
SMADDL/SMSUBL and UMADDL/UMSUBL, SMULH/UMULH, and 32/64-bit SDIV/UDIV.
Aliases with Ra=XZR provide MUL/MNEG and signed/unsigned long multiply/negate.
Operands use XZR, not SP. W results zero-extend, all forms preserve NZCV,
zero divisors produce zero, and signed division truncates toward zero with
MIN/−1 retaining the architectural low-width result. Four 32-bit limbs produce
the upper product without host 128-bit extensions. Signed division uses
unsigned magnitudes to avoid host signed overflow. No state layout changes:
`VNXS0001` and the 4 MiB process profile remain unchanged.

The [new original guest](../tests/roms-src/switch/muldiv_registers/README.md)
passes **2,071,552 rows / 12,429,312 actual words**: all ordered pairs of 256
repeated-byte and 16 unique boundary values, two accumulator/NZCV seeds and
fourteen forms. Its independent unbounded-integer oracle grades source operands,
accumulator, result, actual NZCV and context. The executed result-bit mutant
fails all 448 short cases; six field and seven stream controls reject. A
production ABI gate compares real register output and full states after restore
while this guest executes. Artifacts: `build/artifacts/owned/switch_muldiv/`.

The bit-serial full-product/long-division component checks **33,147,520 snapshots**:
**33,144,832** table-pair/form/accumulator cases across all sixteen initial NZCV
values, plus **2,688** register-routing/alias cases. It checks source, SP,
unrelated-register, flag and PC/step preservation; reserved base encodings
reject before architectural writes. All fourteen original opcode forms match
the [pinned LLVM encoding reference](../tests/reference/switch-muldiv/README.md).
That is an encoding comparison, not execution on a physical console.

All **six Switch gates pass** in **14.26 seconds**; component UBSAN passes with
warning-free compilation. The full build passes and all 96 original archives
reproduce. Completion renderer, changed local documentation links and whitespace
pass. Switch stays **25% = 5/20 fixed delivery milestones**: the whole CPU,
firmware/HOS/retail boot, native memory/MMU/devices, GPU/audio, controllers,
saves/netplay, hardware timing and exhaustive native corpus remain incomplete.
CI/CD remains disabled. The full Nintendo implementation goal remains active.


## AArch64 conditional select

Plugin **0.3.0**, emulator ABI v2, adds 32/64-bit CSEL, CSINC, CSINV and CSNEG.
All sixteen conditions consume actual NZCV; AL and NV select the first source.
False conditions select the second source unchanged, incremented, inverted or
negated. Arithmetic wraps at the result width, W writes zero-extend, both
sources and the destination use XZR semantics, and flags/SP are preserved.
Reserved S/bit-11 encodings stop with a diagnostic before architectural writes.
`VNXS0001`, the 4 MiB process map and scheduling remain unchanged.

The [original guest](../tests/roms-src/switch/select_registers/README.md) passes **524,288 rows / 2,621,440 actual words**:
all ordered pairs of sixteen boundary/mixed-bit operands, all sixteen NZCV
inputs and all sixteen condition codes across eight forms. Real STR/MRS
capture sources, result, flags and context; a separate boolean-table and
unbounded-integer host oracle grades the file. It caught a boundary constant
error in the initial builder, corrected before shipment. The executed result
mutant fails all **8,192** short rows; five field and seven stream controls
reject. The production ABI gate replays real output and complete states while
this guest executes. Artifacts: `build/artifacts/owned/switch_select/`.

The independent truth-mask and bit-level component passes **151,846,912
snapshots**: 151,519,232 repeated-byte/boundary pair/form/flag/condition cases
and 327,680 register-number, alias/XZR cases across all flag/condition inputs.
All eight guest forms match [pinned LLVM opcode fields](../tests/reference/switch-select/README.md); physical
execution is unverified. The previous core failed the component and stopped
the guest before any register row, establishing the missing instruction path.

All **nine Switch gates pass** in **22.03 seconds** after the full rebuild,
including the six existing regressions. Component UBSAN passes with warning-free
compilation. Full build, completion renderer and whitespace pass; all **97
original archives** reproduce. Switch stays **25% = 5/20 fixed delivery
milestones**: full CPU/extensions/exceptions, HOS/native boot, native RAM/MMU,
GPU/audio/controllers/devices, saves/netplay, physical timing and the complete
original hardware corpus remain pending. CI/CD remains disabled, no commercial
bytes are included, and the full Nintendo implementation goal remains active.


## AArch64 variable shifts and unary bit operations

Plugin **0.4.0**, emulator ABI v2, implements LSLV/LSRV/ASRV/RORV in both
widths, RBIT, REV16, REV32 (including W REV), X REV64, CLZ and CLS. Shift
counts mask to five/six bits; arithmetic shifts sign-fill, and zero rotations
avoid host shifts by the operand width. Byte reversal is local to the selected
group, CLS excludes the sign bit, W writes zero-extend, and all forms preserve
NZCV. Operands/destinations use XZR semantics and leave SP unchanged. Reserved
unary encodings reject before architectural writes. `VNXS0001`, ABI/state
layout, fixed process map and scheduling remain unchanged.

The [original guest](../tests/roms-src/switch/bitops_registers/README.md) passes **1,120,096 rows / 5,600,480 actual words**:
272 repeated-byte/full-width boundary operands, all shift counts 0–255, two
flag seeds and eight variable-shift forms (1,114,112 rows), plus eleven unary
forms (5,984 rows). Actual source/count/result/MRS NZCV/context are emitted by
guest STR instructions. Independent unbounded arithmetic, binary strings and
byte-sequence operations grade these files. The executed result mutant fails
all **172** short rows; five field and seven malformed-stream controls reject.
The production ABI gate replays actual register output and complete states.
Artifacts: `build/artifacts/owned/switch_bitops/`.

The separate per-bit component passes **9,218,304 snapshots**: 9,121,792
variable-shift cases, 47,872 unary cases, and 48,640 alias/XZR/register-routing
cases. It covers all sixteen initial NZCV values and six extra count patterns
with upper bits set. All registers, flags, SP and counters are checked; reserved
opcode/Rm/S and shift controls reject. The previous production core stopped
the original guest before any register row, and the component failed before
implementation. All nineteen original forms match the [pinned LLVM encoding
reference](../tests/reference/switch-bitops/README.md); this does not claim physical execution.

All **twelve Switch gates pass** in **24.92 seconds** after a warning-free full
build, including nine existing regressions. Component UBSAN passes with clean
compilation. All **98 original archives** reproduce; actual report identities,
completion renderer, changed local links and whitespace pass. Switch stays
**25% = 5/20 fixed delivery milestones**: full CPU/extensions/exceptions, native
memory/MMU, firmware/HOS/retail boot, GPU/audio/controllers/devices, persistent
saves/netplay, physical timing and the complete native hardware corpus remain
pending. These finite operand domains do not claim complete hardware accuracy.
CI/CD remains disabled. The full Nintendo implementation goal remains active.


## AArch64 bit-field and extract

Plugin **0.5.0**, emulator ABI v2, adds W/X SBFM, BFM, UBFM and EXTR. This
includes their fixed-shift, sign/zero-extension and field insertion/extraction
aliases. BFM preserves bits outside the field; SBFM extends the source end bit;
EXTR reads Rn:Rm in high:low order. W results zero-extend, NZCV/SP remain
unchanged, and register aliases/XZR follow native operand rules. Reserved
opcode/N/width/immediate encodings stop before architectural writes.
`VNXS0001`, the process map and scheduling remain unchanged.

The [original guest](../tests/roms-src/switch/bitfield_registers/README.md) passes **494,592 rows / 2,967,552 actual words**:
sixteen boundary/mixed-bit inputs, zero/all-ones prior destinations with NZCV
seeds 9/F, and every legal immediate across both widths. There are 15,360
bit-field plus 96 extract instructions. Actual source/high, complement/low,
prior destination, result, MRS NZCV and context are captured by STR. Separate
contiguous-field arithmetic and double-width host integers grade the file.
The executed result mutant fails all **240** short rows; six field and seven
malformed-stream controls reject. Production ABI replay matches actual output
and complete states. Artifacts: `build/artifacts/owned/switch_bitfield/`.

The independent per-bit component passes **247,414,784 snapshots**: 133,693,440
field cases, 113,639,424 extract cases and 81,920 register/alias/XZR cases.
Every legal immediate, all sixteen initial NZCV values and full EXTR ordered
pairs across the 272 repeated-byte/boundary values are checked. The former
core failed the component and emitted no guest row before implementation.
Every one of the **15,456 generated instruction words** matches eight
[pinned LLVM opcode/width patterns](../tests/reference/switch-bitfield/README.md). These are finite operand
domains; physical instruction execution and native timing remain unverified.

All **fifteen Switch gates pass** in **36.35 seconds** after a warning-free full
build, including twelve existing regressions. Component UBSAN passes with clean
compilation. All **99 original archives** reproduce; report identities,
completion renderer, changed local links and whitespace pass. Switch stays
**25% = 5/20 fixed delivery milestones**: the full CPU/extensions/exceptions,
native memory/MMU/devices, firmware/HOS/native boot/retail compatibility,
GPU/audio/controllers, persistent saves, netplay, physical timing and the
complete original native corpus remain pending. CI/CD remains disabled. The
full Nintendo implementation goal remains active.


## AArch64 logical immediates

Plugin **0.6.0**, emulator ABI v2, adds W/X AND, ORR, EOR and ANDS immediate
instructions. Encoded runs of ones rotate and repeat at the selected element
width. All-ones elements, zero selector and W/N mismatches are rejected before
architectural writes. Source register 31 is XZR; destination 31 is SP for the
three non-flag operations and XZR for ANDS. W writes zero-extend. ANDS sets N/Z
and clears C/V; other forms preserve NZCV. `VNXS0001`, ABI, process map and
scheduling remain unchanged.

The [original guest](../tests/roms-src/switch/logical_imm_registers/README.md) passes **1,449,984 rows / 7,249,920 actual words**:
sixteen boundary/mixed-bit source values, NZCV seeds 9/F and all **45,312**
legal instruction encodings (four operations × 3,648 W plus 7,680 X encoded
masks). Duplicate encodings of the same mask are included. Actual source,
result, MRS flags, incoming flags and context are captured by STR. Independent
forward run/element descriptors and binary-string logic grade the file. The
executed result mutant fails all **128** short rows; five field and seven
malformed-stream controls reject. Production ABI replay matches actual output
and full states. Artifacts: `build/artifacts/owned/switch_logical_imm/`.

The independent forward-element/per-bit component passes **197,296,128
snapshots**: 197,197,824 legal-mask/source/flag cases and 98,304 register/alias/
SP/XZR cases. All **20,224** reserved encoded masks/operations are rejected
with preserved registers, SP and flags. An initial component reference
assertion was corrected: a 32-bit element repeats in X width, giving
`0x100000001`, not scalar one. The former core failed the component and
emitted no guest register row. All 45,312 words match [pinned LLVM fixed
opcode/width fields](../tests/reference/switch-logical-imm/README.md); physical execution remains unverified.

All **eighteen distinct Switch gates pass cumulatively**. The first batch
passed seventeen and exposed that reference assertion; after correction the
rebuilt component and production replay pass in **10.18 seconds**. All fifteen
existing regressions and the strict new guest passed the first batch. UBSAN
passes with clean compilation, full build is warning-free, and all **100
original archives** reproduce. Report identities, completion renderer, changed
local links and whitespace pass. Switch stays **25% = 5/20 delivery milestones**:
full CPU/extensions/exceptions, native RAM/MMU/devices, firmware/HOS/boot/retail
compatibility, GPU/audio/controllers, persistent saves/netplay, native timing
and the complete original/physical corpus remain pending. These finite operand
domains do not establish full hardware accuracy. CI/CD remains disabled and
the full Nintendo implementation goal remains active.


## AArch64 conditional comparison

Plugin **0.7.0**, emulator ABI v2, adds W/X CCMP and CCMN with register or
five-bit immediate operands. It evaluates the old condition flags, then writes
arithmetic NZCV on a true condition or the encoded fallback on false. Incoming
carry is not an arithmetic operand; AL/NV are always true. All GPRs/SP remain
unchanged, register sources use XZR, and reserved S/bit-4/bit-10 encodings
reject before architectural writes. `VNXS0001`, ABI, process map and scheduling
remain unchanged.

The [original guest](../tests/roms-src/switch/ccmp_registers/README.md) passes **786,432 rows / 3,932,160 actual words /
12,582,912 individual flag observations**. Each row packs sixteen actual MRS
NZCV nibbles, one for every fallback value, using real UBFM/ORR instructions.
Guest STR captures left/right operands, incoming flags, packed results and
context; no expected values are in the image. Every condition and incoming
NZCV value, sixteen boundary/mixed-bit sources, all register operand pairs and
all 32 five-bit immediates are checked in both widths. The separate oracle
uses boolean conditions and unbounded signed/unsigned arithmetic. The executed
mutant fails all **12,288** short rows; five field, sixteen packed-lane and
seven malformed-stream controls reject. Production ABI replay matches actual
packed output and complete states. Artifacts: `build/artifacts/owned/switch_ccmp/`.

The separate truth-mask and bit-serial full-adder component passes **14,155,776
snapshots**: 4,194,304 register, 8,388,608 immediate and 1,572,864 alias/XZR
cases. Every incoming/fallback NZCV value and condition is covered, including
source aliases and every register number. The previous core fails this new
component and stops the guest before any row. All **2,112 compare base words**
match the [pinned LLVM opcode/width fields](../tests/reference/switch-ccmp/README.md). These finite operand
domains do not establish physical execution, full-width pair enumeration or
native hardware timing.

All **twenty-one Switch gates pass** in **76.99 seconds** after a warning-free
full build, including all eighteen existing regressions. Component UBSAN passes
with clean compilation. All **101 original archives** reproduce; report
identities, completion renderer, changed local links and whitespace pass.
Switch stays **25% = 5/20 delivery milestones**: full CPU/extensions/exceptions,
native memory/MMU/devices, firmware/HOS/native boot/retail compatibility,
GPU/audio/controllers, persistent saves/netplay, physical timing and complete
native/physical corpus remain pending. CI/CD remains disabled. The full
Nintendo implementation goal remains active.


## RDP complete command packets

Plugin **0.28.0**, emulator ABI v2, fetches complete native command packets,
including all eight triangle forms (4–22 64-bit words) and two-word texture
rectangles. CURRENT advances with each fetched word. Partial packets survive
an END boundary and continue on extension; coefficient words cannot become
commands. A complete unimplemented draw retains its packet and reports a
blocker without drawing or asserting FullSync. Freeze/flush and nominal issue
counters retain their functional-profile conventions; physical FIFO depth,
prefetch/raster timing and reserved electrical behavior remain ungraded.

The [original cartridge](../tests/roms-src/n64/rdp_packets/README.md) passes **200,704 cases / 1,404,928 actual words**:
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

The [original framebuffer cartridge](../tests/roms-src/n64/rdp_triangle_fill/README.md) passes **795,648 cases /
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

## One-cycle triangle depth rendering

Production one-cycle RGBA16/RGBA32 triangles now use the existing depth-image
address and decoded/interpolated Z coefficients. Alpha/coverage adjustment
precedes depth comparison. The comparator supplies coverage, overflow, blend
enable and current memory-alpha shifts. Accepted color writes precede depth
updates, including aliased buffers; depth stores preserve retained color history.
Rejected pixels retain their native color history without writing color or Z.
Comparison-only, update-only and combined paths are supported. Complete color
and depth row bounds are checked before RAM, hidden bits or RGB latches change.

The independent pipeline oracle passes **8,388,608 selected combined decisions**:
every stored depth word and hidden pair, four Z modes, and eight selected
incoming Z/delta/color/alpha/coverage/mode partners. The component test also
checks actual triangle RAM/hidden results, DP command dispatch, comparison-only,
update-only and combined execution, buffer alias ordering, and atomic alignment/
bounds rejection. All **nine compiled pipeline faults** are rejected after
execution. Warning-as-error and UBSAN pass. The prior complete depth codecs and
pinned primary comparator gates remain applicable; this new pipeline corpus is
not an exhaustive joint operand corpus or a physical-console measurement.

The new original MIT cartridge
[`rdp_triangle_depth`](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_triangle_depth)
passes **524,288 cases / 11,010,048 actual 64-bit RAM/register observations**:
every stored 16-bit depth word, all four Z modes and RDRAM/wrapping-XBUS command
delivery, with selected incoming Z, alpha and fixed geometry/coverage partners.
Guest CPU writes initialize paired depth metadata; guest instructions capture
four color words, four color EBUS words, four depth words, four depth EBUS words,
two guards and three DP/MI registers. Two register lines per case stay within
the sink's line limit. Independent host quarter-grid, compression and comparator
arithmetic grades the stream. The 80-case control passes, its separately
executed wrong-capture ROM fails every row, and all **21 field flips and seven
stream controls** reject. No commercial ROM or SDK bytes are included.

Plugin **0.38.0**, ABI v2 and **VN64S027 / 9,527,568 bytes** remain current.
Existing serialized depth-image, blender, packet and hidden-memory fields suffice;
no state-layout expansion is needed. Production checkpoint replay compares
subsequent actual guest output and whole-state bytes. All **112 original ROM
archives** reproduce against the manifest. N64 remains **45% = 9/20 delivery
milestones**; full graphics/retail compatibility is incomplete. Primitive depth,
interpixel/two-cycle shifter history, dithering, LOD/mipmapping and physical timing
remain pending. CI/CD stays disabled and the Nintendo implementation goal is active.

```sh
ctest --test-dir build -R '^n64_rdp_(depth|triangle_depth)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_triangle_depth/` and
`build/artifacts/component/n64_rdp_depth_pipeline/`.

The current private Super Mario 64 USA probe runs **2,000 quanta / 200 million
CPU attempts** without a terminal CPU fault. It still stops on the complete
22-word `CF000FFA0FFAF006` triangle at `0x227160` (reason 1), with uniform video;
retail boot/playability remain **unverified**. The report at
`build/artifacts/retail/n64-triangle-depth.json` records hashes that match the
current plugin and trace binaries. The depth implementation expands supported
owned profiles; it does not make the remaining Mario profile playable.

Final gates verify all **60 selected N64 regression/component/checkpoint tests**
and production TCP netplay. The initial regression run passed 59 tests; its
remaining gate still rejected inert Z-mode fields. That expectation was updated
to check identical RAM/latch results with comparison disabled, while missing-
hidden-memory comparison/update rejection remains checked. The corrected gate
and final expanded RGBA16/RGBA32 pipeline test both pass. The full build,
warning-as-error/UBSAN pipeline run, nine executed faults and 112-ROM
reproducibility check pass. Current one-cycle ordering and memory-alpha shifts
are grounded in the pinned
[rasterizer](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/rasterizer.c)
and [blender](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/blender.c)
software reference; physical pipeline timing remains unverified.

## Primitive depth and S028 replay

SetPrimDepth now retains its canonical 15-bit Z and raw 16-bit delta. The
ignored high Z bit is masked, Z reaches the comparator as the command value
multiplied by eight, and delta is used directly without triangle-gradient
normalization. One-cycle primitive depth selection overrides triangle Z and
derivatives; it also supplies the memory-alpha delta shifter when Z comparison
is disabled. Interpolated depth remains available. Command and selection
semantics are grounded in the pinned
[rasterizer](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/rasterizer.c)
software reference; physical pipeline timing is not verified.

The independent component gate passes **4,194,304 command captures**: each
16-bit field, four selected partners, all four opcode aliases and DRAM/XBUS
fetch. It independently checks **4,194,304 RGBA16/RGBA32 paired Z stores** with
primitive depth overriding conflicting triangle coefficients. All **65,536 raw
delta values** also pass both the shifter arithmetic and actual no-Z triangle
path. Reset and reserved-state-bit rejection pass. All **seven compiled faults**
are rejected after execution; compile failures are not counted as rejections.
Warning-as-error and UBSAN pass. The individual fields are complete; their full
joint command/render operand space is not exhausted.

The original MIT cartridge
[`rdp_primitive_depth`](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_primitive_depth)
passes **1,048,576 cases / 22,020,096 actual 64-bit RAM/register observations**.
It varies each command field over all 65,536 bit patterns with selected partners,
all four Z modes and RDRAM/wrapping-XBUS delivery. Conflicting triangle Z values
verify primitive selection. Guest instructions read color/depth RAM and hidden
metadata plus guards and DP/MI completion registers, using two lines per case.
Independent host quarter-grid/compression/comparator arithmetic grades actual
results. The 160-case control passes, its separately executed wrong-capture ROM
fails every control row, and **21 field flips / seven stream controls** reject.
Initial stored depth, color and alpha partners vary with the input; geometry and
coverage are fixed. No commercial ROM, SDK or expected-result bytes are included.

Plugin **0.39.0**, ABI v2, **VN64S028 / 9,527,576 bytes** are current. Eight bytes
for canonical primitive depth are appended after the S027 prefix. Imports of
S027 preserve existing RAM, hidden history, depth-image and other state bytes,
with primitive depth initialized to zero; earlier imports remain supported.
The state gate checks FullSync replay, all 33 reserved bits and neutral S027
conversion. Production netplay checkpoint replay compares subsequent actual
guest output and the complete state; invalid primitive state rejects atomically.
All **113 original ROM archives** reproduce against the manifest.

```sh
ctest --test-dir build -R '^n64_rdp_primitive' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_primitive_depth/` and
`build/artifacts/component/n64_rdp_primitive_depth/`. N64 stays **45% = 9/20
delivery milestones**. Dithering, LOD/mipmapping, two-cycle/interpixel state,
physical timing and full retail graphics remain unfinished. CI/CD remains
disabled; the full Nintendo implementation goal remains active.

The current private Super Mario 64 USA probe runs **2,000 quanta / 200 million
CPU attempts** without a terminal CPU fault and still stops on the complete
22-word `CF000FFA0FFAF006` triangle at `0x227160`, reason 1. Video stays uniform;
retail boot and playability remain **unverified**. The current plugin and trace
hashes match `build/artifacts/retail/n64-primitive-depth.json`. This feature
expands the implemented one-cycle profiles; Mario's remaining graphics profile
is still unsupported.

Final verification passes all **63 selected N64 regression/component/checkpoint
tests** and production TCP netplay, including the S028 capacity, FullSync,
reserved-bit, S027 conversion and primitive guest checkpoint gates. The full
build, independent component/guest controls, UBSAN and 113-ROM reproducibility
gates also pass. The frontpage/core percentage verifier is synchronized.

## Ordered dithering and retail command progress

One-cycle RGBA16/RGBA32 rendering now supports magic-matrix, Bayer and disabled
RGB/alpha dither selectors (0, 1 and 3). Alpha/coverage multiplication precedes
pixel alpha dithering; alpha-coverage selection replaces that dithered alpha.
Shade alpha always receives the ordered seed before blender selection. Final
RGB rounds after blending, including rejected pixels whose green bit feeds
retained color history. Field mode halves the matrix row index. RGB-disabled
alpha modes use the native Bayer seed basis. Random selectors, random alpha
thresholds and combiner noise remain unsupported rather than substituted.

The pipeline-mode bit 55 and unused SetOtherModes bits 54, 32–35 and 15
are accepted in the settled production one-cycle profile. Pipeline-mode timing
is not modeled; acceptance does not claim its stall behavior. The original
[GBI header](https://ultra64.ca/files/documentation/online-manuals/man/header/gbi.htm)
identifies pipeline mode separately from the obsolete HW 1.0 color-dither field. Untextured triangles also ignore inactive texture controls
41–51 when the supported combiner does not depend on texture/LOD inputs. The
[pinned dither](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/dither.c),
[combiner](https://github.com/ata4/angrylion-rdp-plus/blob/9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc/src/core/n64video/rdp/combiner.c)
and SetOtherModes/rasterizer reference code ground this settled behavior;
physical pipeline timing remains unverified.

The independent component oracle passes **18,432 periodic/field matrix-seed
cases**, all **2,048 byte-color/seed rounds**, **18,874,368 alpha/coverage/shade
decisions** and **27,648 RGBA16/RGBA32 triangle captures** including ignored
fields, alpha gates, both alpha-coverage flags and final packing. A separate
rejected-pixel check verifies that dithered green updates retained history. All **fifteen
compiled production faults** are rejected after execution. Warning-as-error and
UBSAN pass. The optional
[pinned primary adapter](https://github.com/jshojan/veloce/tree/main/tests/reference/n64-dither)
matches the 18,432 seed cases and 2,048 channel rounds with zero mismatches;
alpha/triangle sequencing uses the independent oracle, not that adapter.

The original MIT
[`rdp_ordered_dither`](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_ordered_dither)
cartridge passes **1,179,648 cases / 15,335,424 actual 64-bit RAM/register words**:
65,536 coupled RGB/alpha input patterns, nine ordered-selector combinations and
RDRAM/wrapping-XBUS commands. Command-source partners select ordinary alpha or
coverage-times-alpha plus alpha-coverage selection; geometry/background are
fixed. Guest instructions read pixels, gathered hidden metadata, guards and
DP/MI completion registers. The independent host oracle imports no production
packing/math. The 252-case control passes, its separately executed wrong-capture
ROM fails every control row, and all **13 field flips / seven stream controls**
reject. This is a finite selected joint domain, not complete graphics hardware.

Plugin **0.40.0**, ABI v2, **VN64S028 / 9,527,576 bytes** remain current; ordered
matrices add no mutable state. Existing state imports and primitive depth are
preserved. Production checkpoint replay checks subsequent actual guest output
and full-state bytes. All **114 original ROM archives** reproduce against the
manifest. N64 stays **45% = 9/20 delivery milestones**. Full graphics/retail
compatibility remains incomplete, CI/CD stays disabled and the Nintendo goal
remains active.

The private Super Mario 64 USA probe runs **2,000 quanta / 200 million CPU
attempts** without a terminal CPU fault or unsupported RSP/DP stop. The original
22-word `CF000FFA0FFAF006` blocker and following untextured triangle now execute;
the tested DP stream completes (CURRENT = END = `0x2273f0`, status `0x80`).
**Nonuniform video is observed for the first time**, but the actual capture is
still corrupted colored output, not verified gameplay. Boot/playability flags
remain false. `n64_boot_trace` now records mode, images, scissor, combine,
primitive depth and tile descriptors for diagnosing later profiles. An optional
fifth argument to `retail_core_probe` writes the last nonuniform framebuffer as
PPM. Probe/core/trace hashes are recorded/checked; commercial bytes and private
captures are not committed.

```sh
ctest --test-dir build -R '^n64_rdp_(dither|ordered_dither)' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rdp_ordered_dither/`,
`build/artifacts/component/n64_rdp_dither/` and
`build/artifacts/retail/n64-ordered-dither.json`. The private framebuffer capture
is `build/artifacts/retail/n64-ordered-dither.png` (PPM alongside it).

Final gates pass all **65 selected N64 regression/component/checkpoint tests**,
production TCP netplay, the full owned guest/control stream, all fifteen
compiled faults, UBSAN and the 114-ROM reproducibility check. The expanded
dither test also checks rejected color history. Current private core, trace
and probe hashes match the report. Frontpage/core percentages stay synchronized.

## Corrected RSP mixed products and Mario logo

VMUDM/VMADM now multiply **signed VS × unsigned VT**; VMUDN/VMADN multiply
**unsigned VS × signed VT**. Their load/accumulate, 48-bit wrapping, destination
saturation and alias behavior are retained. The former implementation and both
arithmetic oracles reversed the signed operand. Published fixture arrays were
also transcribed with the wrong operand order: the hardware-test author's
[assembler takes `(VD, VT, VS)`](https://github.com/thelemmy/nemu64-test/blob/main/src/rsp/rsp_assembler.rs).
The corrected fixture fails against the old implementation and passes the fix.

The original multiply and accumulation ROMs are regraded with corrected
unbounded-integer arithmetic: **917,504 / 294,912 cases** and
**16,515,072 / 7,372,800 actual captured 64-bit words**, respectively. The
accumulation cartridge's VMADM seed now multiplies signed one by its unsigned
low halfword. Its archived image and the generator ledger are updated; the
full domains remain the same. Reduced controls now cover both operand signs:
**1,792 / 144 cases**. Wrong-capture cartridges, **18 / 25 field mutations** and
seven malformed streams per suite must be rejected. No guest computes a verdict.

The corrected component oracle checks **16,777,216 raw-halfword/function/selector
cases**, **8,388,608 register aliases**, **524,288 VSAR encodings** and selected
48-bit accumulator boundaries. All **four compiled signedness faults** are
rejected after execution; compiler failures are not counted. UBSAN and the
warning-as-error mutant build pass. The optional
[pinned primary comparison](https://github.com/jshojan/veloce/tree/main/tests/reference/n64-rsp-mixed)
checks **6,299,072 mixed-product cases**, all vector registers and all accumulator
lanes against unchanged parallel-rsp at `9ab776a2ce28899769c4f911403234cac62fc799`,
with zero mismatches. These are finite domains, not every 2^32 operand pair or
physical-console timing evidence.

The private **Super Mario 64 (USA)** probe now produces a **recognizable Super
Mario 64 logo**, replacing the corrupted framebuffer. The **2,000 scheduling
quanta / 200 million CPU attempts** trace has no terminal CPU exception or
unsupported RSP/DP stop and executes **27,628,331 RSP instructions**. A local
differential diagnostic compares vector registers, accumulator, flags and DMEM
for **16,898,862 vector instructions** from the same input state against the
pinned RSP implementation, with zero mismatches. Before the fix, its first mismatch was
VMADN at SP PC `0x4b8`, word `4a81218e`; 4,014 RDP commands independently
matched the pinned renderer's RAM output from the same supplied inputs, helping
locate the corrupted upstream data. Those private diagnostics are observations,
not committed retail acceptance gates. The captured logo establishes intro
rendering; **full retail boot, gameplay, controller-driven playability and
complete graphics/audio accuracy remain unverified**. Commercial bytes, private
captures and third-party implementations are not committed.

At that increment, plugin **0.41.0**, ABI v2 and **VN64S028 / 9,527,576 bytes** were current. The fix
adds no state fields. N64 remains **45% = 9/20 delivery milestones**, and the
frontpage/core percentages stay synchronized. Random dithering/noise,
LOD/mipmapping, two-cycle/interpixel feedback, complete peripherals and physical
timing remain unfinished. CI/CD stays disabled; the full Nintendo goal is active.

```sh
ctest --test-dir build -R '^n64_rsp_(multiply_test|multiply_pairs|accumulate_edges)$' --output-on-failure
python3 tests/n64_rsp_mixed_mutants.py
python3 tests/roms-src/build.py --original-only --check
```

Artifacts: `build/artifacts/owned/n64_rsp_multiply_pairs/`,
`build/artifacts/owned/n64_rsp_accumulate_edges/`,
`build/artifacts/component/n64_rsp_mixed/` and
`build/artifacts/retail/n64-mixed-signed.json`. The private logo capture is
`build/artifacts/retail/n64-mixed-signed.png` (PPM alongside it). Probe/core/trace
SHA256 identities are recorded and checked.

Final gates: **65 selected N64 regression/component/checkpoint tests pass**,
along with both complete original guest corpora and controls, the pinned primary
comparison, all four executed faults, UBSAN, production TCP netplay, the full
build and the **114-ROM reproducibility check**. Completion/whitespace checks
pass and current private core/probe/trace hashes match the report.


## Copy-cycle transfers and Mario title head

The next private Mario stop was E4 `e40b4324000782e8` in copy cycle mode.
The core now executes E4/E5 texture rectangles and copy-cycle triangle packets
through bank-coupled 64-bit TMEM transfers. It preserves raw palette lanes,
neighbor masks/mirrors, odd-row bank swaps, copy coordinate shifts, perspective
and copy LOD tile selection. Raster writes support 4/8/16-bit framebuffer sizes,
inclusive rectangle bottoms, scissor/field selection, both triangle traversals,
raw 16-bit alpha and 8-bit pair masks, hidden halfword bits and retained history.
Inactive combiner, blender, RGB dither and depth fields do not affect copy data.
Unsupported random CI8 alpha thresholds, 32-bit framebuffer crash behavior and
out-of-range writes reject before mutation. This is functional evidence;
physical DMA/crash timing and complete one/two-cycle LOD are pending.

The original MIT [`rdp_copy_rectangles`](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_copy_rectangles)
cartridge passes **1,572,864 cases / 20,447,232 actual captured 64-bit words**.
All 65,536 coupled seeds run across three framebuffer sizes, both rectangle
coordinate orientations, alpha on/off and RDRAM/wrapped-DMEM command sources.
The guest emits framebuffer, MI EBUS hidden-RAM, guard and DP/MI register words;
the independent Python oracle grades the strict stream. Its **336-case** control
passes; an executed wrong-capture cartridge fails every row; **13 field faults**
and **seven malformed streams** are rejected. This selected joint domain does
not cover the entire Cartesian coordinate/TMEM corpus.

The normal component test passes **131,072 RGBA16 bank fetches** and
**786,432 rectangle RAM/hidden comparisons**, including multiple bursts, flipped
axes, alpha masks, atomic bounds rejection, palette lanes and empty CI8 history.
**Ten compiled production faults** are rejected after actual execution. UBSAN
and warning-as-error compilation pass. The optional [pinned comparison](https://github.com/jshojan/veloce/tree/main/tests/reference/n64-copy)
against unchanged Angrylion RDP Plus at
`9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc` has zero mismatches in
**4,194,304 bank fetches** and **98,304 rectangle/triangle RAM, hidden-bit and
retained-history comparisons**. The external renderer remains outside the repo.
These counts describe software comparisons, not physical-console validation.

The private **10,000 scheduling quanta / one billion CPU attempts** Mario probe
now renders its **animated title head**, with no terminal CPU exception or
unsupported RSP/DP stop. This extends the earlier logo observation;
**gameplay and full retail playability remain unverified**. Private ROM bytes and
captures are not committed. The diagnostic tools now accept the same sorted
port-zero digital input script, and record its SHA256 alongside the core/probe/
trace hashes. Quantum means frontend scheduling quantum; the CPU trace uses
100,000 nominal attempts per quantum. This script is diagnostic input, not a
controller-driven playability acceptance gate.

At that increment, plugin **0.42.0**, ABI v2 and **VN64S028 / 9,527,576 bytes** were current.
Copy uses existing serialized TMEM/DP/hidden state; no new state fields are added.
N64 stays **45% = 9/20 delivery milestones**. Frontpage and all core percentages
share the same fixed ledger. CI/CD remains disabled.

```sh
ctest --test-dir build -R '^n64_rdp_copy_(test|mutants|checkpoint_test)$' --output-on-failure
python3 tests/roms-src/n64/rdp_copy_rectangles/run.py
python3 tests/roms-src/build.py --original-only --check
python3 tests/n64_retail_probe.py /path/to/private.z64 --frames 10000 --capture build/artifacts/retail/n64-copy.ppm --output build/artifacts/retail/n64-copy.json
python3 tests/n64_retail_probe.py /path/to/private.z64 --frames 10000 --input-script tests/input/n64-title-start.txt --capture build/artifacts/retail/n64-copy-start.ppm --output build/artifacts/retail/n64-copy-start.json
```

Artifacts: `build/artifacts/owned/n64_rdp_copy_rectangles/`,
`build/artifacts/component/n64_rdp_copy/` and the private retail paths above.

Final verification: **67/67 selected N64 regression/component/checkpoint tests**
pass in **164.40 seconds**, including production copy output/full-state replay.
Production two-process TCP rollback also passes. The full build, original full
copy corpus and all controls, ten executed faults, UBSAN and **115 original ROM
archive reproductions** pass. Completion and whitespace checks pass.

The final idle and scripted Start/A probes each execute **272,121,722 RSP
instructions** and observe **500,082 audio sample pairs** in 10,000 quanta. Both
have no terminal CPU/RSP/DP stop. Their final captured framebuffer and recorded
CPU/DP/RSP trace are identical; **no menu transition or controller-driven gameplay
was observed**. The audio count does not establish audible correctness. Current
core/probe/trace and input-script hashes match the recorded reports. Captures are
`build/artifacts/retail/n64-copy.png` and `n64-copy-start.png` (PPMs alongside).


## Cached controller reads and current Mario stop

A private SI trace explains why Start/A previously had no observed effect: Mario
formats its controller packet once, then repeatedly reads it. The old core
sampled buttons on the format write and returned stale replies on later reads.
At the scripted Start transition, the trace had **49 reads but only three writes**;
PIF control was already zero and its retained replies still contained zero buttons.
The [cached libultra read path](https://github.com/n64decomp/libreultra/blob/master/src/io/contreaddata.c),
[Mupen64Plus SI](https://github.com/mupen64plus/mupen64plus-core/blob/master/src/device/rcp/si/si_controller.c)
and [ares PIF](https://github.com/ares-emulator/ares/blob/master/ares/n64/pif/hle.cpp)
corroborate the separate formatting and read-time communication phases.

The original `PifJoybus` module now latches five channel offsets on the format
flag, clears that flag without sampling buttons, and refreshes the four digital
controller replies before every SI read DMA. Padding, skipped/reset channels,
receive-status clearing, command lengths, inactive layouts and bounded malformed
packets have explicit behavior. A flag-zero replacement write preserves the
latched offsets. Native analog, controller presence/paks, cartridge Joybus/CIC,
asynchronous SI/Joybus timing and physical-console validation remain incomplete.

The new original MIT [cached controller cartridge](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/controller_cached_reads)
passes **16,384 cases / 262,144 actual captured 64-bit words**, with exactly one
format write for the entire run. Each case polls one 14-button combination,
yields for a real frontend input transition, then polls its complement. Four
packet words, untouched tail/control and SI/MI registers are captured per read,
plus the two registers after acknowledgment. Its strict 7/9-word chunks respect
the sink line limit. The **16-case** control passes, an executed wrong-capture
cartridge fails every case, and **16 field mutations / seven malformed streams**
are rejected. The earlier **16,384-case / 131,072-word** controller corpus also
passes. These exhaust the implemented digital bit combinations, not the full
physical controller/Joybus/timing corpus.

The component gate passes **131,072 cached digital reads** across all four active
ports, **81,920 command/receive-length/channel captures**, all **256 control
bytes**, padding/skip/reset and atomic malformed-state checks. The machine gate
checks real SI/RAM/interrupt effects, format/read separation, flag-zero replacement,
exact continuation and **287 atomic state rejections**. **Ten compiled production
faults** fail after execution, including removal of SI read-time polling; compiler
failures are not counted. UBSAN and warning-as-error builds pass.

Plugin **0.43.0** retains ABI v2 and writes **VN64S029 / 9,527,616 bytes**.
Five validated big-endian channel-offset words are appended to S028; current
save/restore and production host/client replay preserve the retained packet
layout and actual guest output. Earlier formats import with channels disabled,
since those states lack the retained layout. Such sessions need a new guest
format command; restart older retail sessions if they only issue cached reads.
Existing field/rejection/legacy tests now address their same verified byte
positions relative to the longer state.

The final private **10,000-quanta / one-billion-attempt** Start/A probe changes
execution and reaches a new DP stop: one-cycle E4 **`e4300260002801e0`**, reason 1,
other mode **`802c3f00443079`**. It executes **119,825,466 RSP instructions** with no
terminal CPU exception or unsupported RSP operation. The final capture remains
the title head with **PRESS START**; **no menu transition or gameplay is verified**.
The observed **500,082 audio sample pairs** do not establish audible correctness.
This exposes the next missing graphics profile; the former idle/copy title-head
reports are historical. Private bytes/captures and reference implementations are
not committed. Core/probe/trace/input-script fingerprints match the private report.

N64 remains **45% = 9/20 delivery milestones**. The frontpage and every core
README share the fixed ledger. CI/CD remains disabled and the whole goal is active.

```sh
ctest --test-dir build -R '^n64_pif_(joybus_test|state_test|checkpoint_test|joybus_mutants)$' --output-on-failure
python3 tests/roms-src/n64/controller_cached_reads/run.py
python3 tests/roms-src/n64/controller_serial/run.py
python3 tests/roms-src/build.py --original-only --check
python3 tests/n64_retail_probe.py /path/to/private.z64 --frames 10000 --input-script tests/input/n64-title-start.txt --capture build/artifacts/retail/n64-cached-start.ppm --output build/artifacts/retail/n64-cached-start.json
```

Artifacts: `build/artifacts/owned/n64_controller_cached_reads/`,
`build/artifacts/component/n64_pif_joybus/`,
`build/artifacts/retail/n64-cached-start.json` and private
`n64-cached-start.png`/`.ppm` in that retail directory.

Final gates: all **70 selected N64 regression/component/checkpoint tests** pass.
The first batch passed 64; six older PI/RI/MI/RDRAM/cache/SP tests still used
S028 tail distances and passed their **57.00-second** rerun after those distances
were corrected. Their rejection checks remain at the same serialized byte fields.
The new plugin checkpoint additionally rejects malformed Joybus offsets atomically.
Full build, production TCP rollback, both complete controller guest corpora and
controls, ten executed faults, UBSAN and **116 original archive reproductions**
pass. Completion/whitespace checks and current private binary/script fingerprints
are verified. These functional gates do not establish the full native hardware
corpus or playable Mario; the one-cycle E4 profile is the next observed stop.

## One-cycle texture rectangles and Mario intro

The cached-controller increment exposed a one-cycle E4 stop. E4/E5 rectangles
now decode into native constant edges and signed S/T interpolants, then use the
existing one-cycle triangle pixel pipeline. The exclusive bottom edge, flipped
texture derivatives, scissor/field, point/three-point/TLUT sampling, combiner,
ordered dither, alpha/coverage, blender, hidden metadata and primitive depth
therefore share the tested implementation. Copy and fill keep their established
profiles. Two-cycle pixels, complete texture LOD and random dithering/noise remain
unsupported. Plugin **0.44.0** retains ABI v2 and **VN64S029 / 9,527,616 bytes**;
this change adds no serialized fields.

The original MIT [`rdp_texture_rectangles`](https://github.com/jshojan/veloce/tree/main/tests/roms-src/n64/rdp_texture_rectangles)
cartridge captures **1,048,576 cases / 13,631,488 actual 64-bit words** through
real DP and MI EBUS operations. All 65,536 coupled raw coordinate/derivative
seeds run with both framebuffer sizes, flips, filters and command sources.
XBUS also enables alpha comparison and alternating field parity. An independent
integer oracle checks framebuffer, hidden bits, guards and DP/MI registers.
The 224-case boundary cartridge passes; a separately executed corrupted capture
fails every row, and thirteen field plus seven strict-stream controls reject.
This selected domain does not cover every Cartesian combination.

An independent C++ arithmetic gate checks **524,288 rectangle RAM/hidden
observations**. Four compiled and executed production faults are rejected:
inclusive bottom, exchanged derivatives, derivative scale and negative derivative
loss. The optional pinned renderer comparison checks **32,768 rectangles**, each
including every RAM byte and hidden halfword, history and retained memory color.
Fractional bounds, fields, formats/sizes, palette/perspective/filtering, ordered
dither, blending and primitive depth vary in coupled seeds. See the repository's
`tests/reference/n64-rectangle/README.md` for the exact pin and reproduction.
Software comparison is not physical hardware validation.

The private Super Mario 64 USA probe with
`tests/input/n64-title-menu.txt` reaches the **Peach castle intro**, beyond the
Mario title head. Its bounded **1,000,000,000 CPU attempts** record no terminal
CPU exception, unsupported RSP operation or DP stop. The capture shows Peach and
the castle sky; it does not verify menu navigation, playable movement, sound
correctness or complete boot. Private ROM bytes and captures stay outside git.
Artifacts: `build/artifacts/retail/n64-rectangle-menu.json` and matching PPM;
owned evidence: `build/artifacts/owned/n64_rdp_texture_rectangles/` and
`build/artifacts/component/n64_rdp_rectangle/`.

N64 remains **45% (9/20 delivery milestones)**. The complete retail, graphics,
controller and timing milestones remain uncredited. All core and front-page
percentages are synchronized from the fixed ledger. CI/CD remains disabled.

This increment also restores a signed-depth test boundary accidentally altered
by the prior state-size edit, and adds section garbage collection to the optional
copy reference commands so they link without unused parallel renderer routines.

Verification for this increment: **72/72 selected N64 component/checkpoint
regressions pass in 86.69 seconds**, production TCP netplay passes in 3.05 seconds,
full build completes without warnings, component UBSAN and four executed fault
controls pass, and **117 original archives reproduce**. The original rectangle
capture suite and all of its negative controls pass. The private report's core,
probe, trace, script and capture hashes match the final local artifacts.


## Game Boy result-channel integration (2026-10-11)

Current Game Boy evidence and remaining scope are in the
[core README](../cores/gb/README.md#result-channel-branch-integration-2026-10-11).
Legacy adapter bootstrap verdicts and source reproducibility do not establish
whole-platform hardware accuracy. The original arithmetic/unary corpora retain
independent register grading. Unbanked ROM+RAM headers 08/09 now have a fixed
8 KiB functional RAM profile, with no new state fields; licensed-cartridge
hardware validation remains unavailable for these header types.


## N64 signed stick wire input (2026-10-11)

The original [cached analog cartridge](../tests/roms-src/n64/controller_analog_reads/README.md)
now exhausts the 65,536 signed X/Y byte pairs, with a changed complementary input
between cached SI reads: **65,536 cases / 1,048,576 actual 64-bit words** matched.
Digital masks vary with the case; this does not exhaust masks times coordinates.
Four-port component tests cover **524,288 cached analog observations** and retain
all digital/packet grammar gates. S029 full-state/actual guest output replay,
selected SDL virtual-controller endpoints/dead zone/capture suppression, canonical
recording/playback/turbo, and production two-peer TCP rollback all pass. The TCP
gate grades 600 words independently and matches complete 9,527,616-byte states.
Thirteen executed production Joybus faults are rejected, including neutral-X,
axis swap and wrong-port Y. The guest controls reject sixteen field faults, seven
malformed streams and an executed wrong-capture cartridge. All **128** original
ROM archives rebuild against the manifest.

The canonical N64 v1 input word reserves bits 14/15 as a tag; valid legacy
fourteen-button masks retain neutral axes. Frontend selected-pad left-stick input
uses a 15% axial dead zone and +/-80 range; T/F/G/H supply keyboard movement,
with arrows still mapped to the D-pad. Analog editor fields are available in the
movie window. This is a deterministic input policy and wire-byte profile, not
physical calibration/optical counter accuracy. Dedicated analog remapping,
controller origin/reset/accessories/presence, physical timing and full local
multiplayer assignment remain pending. The previous private Mario courtyard
probe used digital input; analog Mario playability is not yet verified.
**N64 stays 45%** of the fixed milestone ledger; CI/CD remains disabled.


## N64 private courtyard inputs and native movie continuation (2026-10-11)

The private SM64 US checkpoint now verifies bounded analog walking, ascending
A jumps and B punches through cache-aware RAM fields and matched neutral controls.
A 1,000-quantum Y=+80 run moves Z from 4354 to 3713.6298828125; a 150-quantum A
run raises Y from 260 to 430; a 100-quantum B run reports punch action `00800380`.
Neutral runs retain the standing action and original position. All requested
continuation traces complete without a terminal CPU, RSP or DP stop. The original
file grader rejects the actual neutral continuations substituted for each action
with matched checkpoint/duration and valid state fingerprints.

Production Application/Default Input/Default TAS execute 180 analog movie quanta:
ordinary, recording and playback full S029 states match; six seek boundaries match
and clear queued audio. An executed neutral movie changes actual guest action and
position. The new original-cartridge movie gate independently checks SI RAM after
positive and neutral movies. A separate original checkpoint gate matches 100+200
resumed calls against 300 uninterrupted calls, including complete state,
framebuffer, absolute labels, inspector file preservation and bad-state/range
rejections. ROM archive count remains **128**; no ROM generator changed.

Private data stays in ignored `build/artifacts/retail/analog/`. Source, commands,
primary watch bindings and precise limits are documented in the
[N64 README](../cores/n64/README.md#private-courtyard-controls-and-movie-checkpoints)
and [retail input guide](../tests/input/N64-RETAIL.md). Castle entry/main-level
progression, full-title playability, native firmware boot, physical peripherals
and timing remain unfinished. Native VI frame scheduling and practical
normal-speed gameplay are pending. **N64 remains 45%; all fixed milestone scores
are unchanged. CI/CD remains disabled.**

## 2026-10-11: N64 scheduling integration

The original `vi_field_schedule` image supplies actual CURRENT-to-RAM loop
observations. Its production plugin file gate checks 5,160 records / 46,440
output words across separate full horizontal/vertical domains and 40 leap/parity
cases, with rational-clock grading and executed wrong-register/old-budget
controls. Native S030 state persists scheduling; legacy import, bounds, termination
and ordinary/rollback equality are covered. This integration-owned evidence
changes frontend frame meaning for new native sessions; it does not award physical
hardware timing accuracy or expand the fixed completion score. Private SM64 VI
controls and 121-field movie replay supplement the original corpus. See
`tests/roms-src/n64/vi_field_schedule/README.md` for precise exclusions.

## 2026-10-11: N64 integer trap profile

The original `integer_traps` cartridge covers every 16-bit trap immediate and
10-bit register software code over its stated full-width operand patterns and
ordinary/taken-delay contexts. Its actual handler writes RAM/CP0 observations;
the host grades 15,728,640 cases / 188,743,680 words without guest expected answers.
Actual opposite-condition controls fail all 15,360 cases; twelve field faults,
six strict stream errors and an order fault are rejected. Native full-state/
rollback replay covers the output/exception path. A 37,810,176-observation
component gate adds all source register encodings, defined 32-bit operands,
nested/BEV behavior and actual branch/likely-annul cases. No full-CPU or physical
pipeline accuracy claim is made; consult the cartridge README for exclusions.


## 2026-10-11: N64 linked memory

N64 0.48.0 implements LL/LLD and SC/SCD with ERET link clearing and S031
live-link checkpoint persistence. The original cartridge grades 1,048,576 cases /
10,485,760 actual RAM words over every signed offset, both widths, four input
patterns and successful/ERET failure paths. Executed missing-ERET controls and
strict result/stream faults are rejected independently. Component checks cover
registers, privilege, faults and diagnostic LLAddr behavior. Native complete-state
rollback and S030 imports are verified. See the core and original cartridge
READMEs for commands and exact scope. Cache interference is a conservative
functional model; physical interference, complete CPU/timing and whole-title
retail playability remain unverified. N64 stays 45%; CI/CD stays disabled.

All **131 original archives** reproduce; private SM64 VI control continuations
pass on the current build. Completion scores are unchanged.

Private SM64 production movie verification on **0.48.0** also passes: **121 native
VI frames**, complete ordinary/record/play states and **six exact seek boundaries**;
an executed neutral-input movie differs. Production two-peer TCP netplay passes.


## 2026-10-11: N64 disabled COP2 exception priority

N64 **0.49.0** now raises coprocessor unusable (CpU=11, CE=2) for disabled
COP2 and its word/doubleword loads/stores, before reserved suboperations or data
address/translation errors. The component gate covers all 2^26 payloads for all
five major opcodes plus metadata contexts: **335,585,280 observations**, with
unchanged operands/HI/LO/BadVAddr and no data-bus accesses.

The original [COP2 cartridge](../tests/roms-src/n64/cop2_unusable/README.md)
exports **9,056,256 cases / 108,675,072 actual RAM/CP0 words**. The native domain
includes every memory offset over sixteen address patterns, all move register
encodings, all branch offsets and low sixteen operation-function bits, in ordinary
and actual taken-delay contexts. Its independent oracle checks raw handler
metadata and ERET; executed COP1 substitution, field/stream faults and full-state
ordinary/fast rollback are checked. See the cartridge README for exact exclusions.

Enabled expansion-COP2 behavior remains unsupported with the existing RI
convention; COP2 is not the RSP vector unit. Physical exception/pipeline timing,
complete CPU/device behavior and full-title playability remain unfinished.
S031/state capacity and fixed completion scores are unchanged: **N64 stays 45%**.
CI/CD remains disabled. Rules follow NEC VR4300 §6.4.3 and §6.4.12.

Verification: **11 selected gates pass** (two COP2 gates, eight machine/integer/
linked-memory/native-checkpoint/TAS regressions, and production two-peer TCP
netplay). Private SM64 native VI walking/jumping/punching and stationary controls
pass on 0.49.0. All **132 original archives** reproduce; completion renderer and
whitespace checks pass. The full Release build passes.


## 2026-10-11: N64 CP0 physical Watch exceptions

N64 **0.50.0** implements physical eight-byte WatchLo address comparison, R/W
selection, EXL masking, Watch code 23 and register reserved-bit/readback masks.
Alignment/translation precede Watch; Watch precedes data bus access. Instruction
fetch and CACHE are excluded. Scalar, merge, FP and linked accesses share the
check. The existing S031 CP0 fields preserve an armed WatchLo without changing
format or the 9,527,616-byte capacity.

The original [Watch cartridge](../tests/roms-src/n64/watch_exceptions/README.md)
exports **4,194,816 cases / 58,727,424 RAM/CP0 words**: every eight-byte granule of
8 MiB RDRAM, read/write and ordinary/actual taken-delay contexts, plus 512
R/W/lane/matching/EXL controls with executed untrapped memory readback. Its armed
checkpoint compares twelve ordinary/rollback records and full final states.
Executed wrong-exception code, eight result faults and six malformed streams
are rejected independently.

The component domain covers every 2^29 granule of the full 32-bit physical
address space at read and write senses. Supplementary width/lane/flags/EXL,
register transfer, translated alias, bus priority and metadata checks use fixed
operand patterns. This is not all watch/target-address pairs or numerical values.
Nonzero WatchHi matching, failed conditional-store watches, physical/deferred
exception behavior, reverse endian and malformed legacy CP0 fields remain
unverified. Complete CPU/device behavior and full-title retail playability stay
unfinished; **N64 remains 45%** of fixed delivery milestones. CI/CD stays disabled.
See the cartridge README for commands and scope; rules follow NEC VR4300 §6.3.8,
§6.4.3 and §6.4.17.

Verification: **14 selected gates pass** (two Watch gates, eleven machine/cache/
integer/COP2/linked-memory/native checkpoint/TAS regressions and production
two-peer TCP netplay). The Watch component gate executes **1,073,745,425
observations**. Private SM64 native VI walking/jumping/punching and stationary
controls pass on 0.50.0. All **133 original archives** reproduce; full Release
build, completion renderer and whitespace checks pass.

## Ordinary N64 frontend local controller routing (2026-10-11)

N64 0.51.0 exposes four local ports. Real SDL virtual gamepads, the production
Application/default input plugin and original `controller_netplay` cartridge
produce independently graded SI replies: 194 cases / 1,164 guest RAM words,
all 24 assignments and each native control on every port with signed-stick
endpoints/dead zone. Executed neutral/capture cases release inputs; an executed
swapped-device assignment is rejected at two port replies. Secondary devices
are isolated from primary fallback/bindings; saved ownership uses unambiguous
GUID/serial matching. No core ABI/state-format change. Local routing now earns
the fixed `multiplayer` milestone (N64 50%). Physical controllers, independent
secondary remapping, accessories and complete native hardware remain pending.
See [the current implementation and commands](../cores/n64/README.md#four-local-frontend-controller-ports-0510).

## Extra-player frontend button profiles (2026-10-11)

Secondary controls are independently configurable as assigned-gamepad buttons,
signed axis thresholds or separate keyboard keys, with isolated Clear/Reset and
per-platform persistence. Native N64 guest SI replies verify 3,026 cases / 18,156
RAM words: 1,008 button-remap cases, 2,016 axis cases and two persistence/move
cases. An actual wrong map is rejected; remapped physical four-port native TAS
recording/playback/seeks match full states and an executed movie mutant differs.
Hotplug/default-plugin gates verify saved/cleared maps, identity and seven
malformed profile rejections. No core ABI/state-layout change. These frontend
checks leave delivery percentages at N64 50%; full native controller accessories,
analog-axis remapping and physical USB/Bluetooth remain pending.

## N64 64-bit integer mode restrictions (2026-10-11)

N64 0.52.0 checks 28 doubleword integer/LWU families before arithmetic or data
access. Kernel32 remains legal; User/Supervisor require UX/SX. EXL/ERL override
the stored privilege. The mapped original guest corpus verifies 5,768,792 cases /
69,225,504 real RAM/CP0 words: eleven complete immediate domains and six complete
immediate-shift fields at four blocked profiles and two actual placements.
Executed NOP, result/sequence and malformed stream controls are rejected; native
checkpoint/rollback records and full states match. The component gate covers
37,186,848 encoding/mode/metadata/no-data observations. This leaves numerical
operand cross-products, physical timing, full CP0/COP1 rights, non-linked 32-bit
address normalization and complete hardware pending. No ABI/state-format change;
N64 stays 50%. See [source, grading and limits](../tests/roms-src/n64/doubleword_privilege/README.md).

## N64 Kernel32 wide integer address formation (2026-10-11)

N64 0.53.0 corrects the explicit Kernel32 VA63:32-ignore rule for LDL/LDR,
LWU, SDL/SDR and LD/SD; LLD/SCD already implement it. Extended modes preserve
full addresses. The original cached-data cartridge grades 4,718,592 actual
cases / 56,623,104 RAM/CP0 words across nine complete immediate domains,
eight lanes, four width/high-half profiles and direct/taken-delay execution.
The independent oracle checks numerical merge/load/store values, SCD success,
alignment/address exceptions, preserved operands and EPC/BD/BadVAddr. Executed
NOP, field and malformed stream controls reject; native/fast rollback records
and complete states match. The component gate checks 4,730,235 address/mode/
metadata/bus observations, including carry/segment boundaries, EXL/ERL, valid
extended mappings and ordinary word/FP error controls. This does not establish
all operands/bases/TLB layouts, reverse endian operation, physical timing or
complete CPU/hardware. No ABI/S031 change; N64 remains 50%. See
[source, file evidence and limits](../tests/roms-src/n64/kernel32_addresses/README.md).

## N64 CP0 read-only software writes (2026-10-11)

N64 0.54.0 preserves BadVAddr, PRId and CacheErr on MTC0/DMTC0; actual address
faults still update BadVAddr. Privilege/CU0/width priority is retained. The
original cartridge uses a real unaligned fault, handler and ERET to seed
BadVAddr, then grades 12,582,912 cases / 150,994,944 real RAM/register words:
two widths, three targets, four complete sixteen-bit source positions, four
surrounding patterns and direct/taken-delay contexts. Actual wrong CP0 reads,
field and malformed-file controls reject; twelve native/fast rollback records
and complete states match. The component gate covers 22,020,097 source/mode/
metadata/preservation observations. Full Status/Config/reserved CP0 behavior,
reset revision, physical hazards/timing and whole hardware remain pending.
No ABI/S031 change; N64 stays 50%. See
[source, register files and limits](../tests/roms-src/n64/cp0_readonly/README.md).

## N64 Config register fields and K0 data reads (2026-10-11)

N64 0.55.0 implements Config's fixed fields and writable EP/BE/CU/K0 mask while
preserving EC. Entry profiles expose fixed fields/BE=1 and retain EC=0; physical
board DivMode/reset behavior remains unverified. The original 16,777,216-case
cartridge grades 201,326,592 actual RAM/register words with EP=0/BE=1 held:
selected source domains, CU/K0 settings, write widths and direct/taken-delay
contexts. Actual cached reads remain stale after an uncached backing-memory
change except K0=2 bypass, which observes fresh bytes. Wrong reads, field and
malformed streams reject; native/fast rollback records and full states match.
The component gate checks 7,356,417 field/source/mode observations, including
all writable-bit patterns/eight EC values. Full little/reverse endian execution,
EP transfers, physical hazards/cache/timing and whole CPU/hardware remain pending.
No ABI/S031 change; N64 stays 50%. See
[source, file evidence and limits](../tests/roms-src/n64/config_fields/README.md).

## N64 Status write fields (2026-10-11)

N64 0.56.0 preserves read-only TS, clears reserved/upper Status bits and updates
documented writable fields after existing CU0/privilege/width checks. The
original 4,194,304-case cartridge grades 50,331,648 real RAM/register words
across both writes, selected source fields/surrounds and direct/taken-delay
contexts. EXL is held for safe native readbacks; unconstrained writes and TS=0/1
fixtures are checked by the 14,680,066-observation component gate. Actual wrong
reads, field and malformed stream controls reject; twelve native/fast rollback
records and complete states match. TS=1 fixtures are not physical shutdown tests.
Duplicate-TLB shutdown/recovery, trace/power/reverse-endian execution, reserved
CP0, physical hazards/reset/timing and complete hardware remain pending.
No ABI/S031 change; N64 stays 50%. See
[source, file evidence and limits](../tests/roms-src/n64/status_fields/README.md).

## N64 diagnostic CP0 fields and functional CACHE consumers (2026-10-11)

N64 0.57.0 constrains software TagLo/TagHi/PErr fields per NEC Figures 5-18
and 6-11. The original cartridge independently grades **12,582,912 cases /
150,994,944 actual RAM/register words**, with both writes, selected full
sixteen-bit domains, surrounds and direct/taken-delay placement. Source/target/
non-target fields, executed wrong-read rejection, twelve field faults, six
malformed streams and native/fast rollback are checked. The component gate
covers **32,505,856 observations**, including **10,485,760 actual CACHE
consumers** across every physical tag and every defined tag state. Physical
reset/timing, undefined tag states and malformed legacy snapshot normalization
are not proved. N64 remains **50%** of delivery milestones, not hardware accuracy.

## 3DS ARMv6 extension/accumulate domains (2026-10-11)

3DS 0.3.0 adds twelve ARM signed/unsigned scalar/dual-byte extension/accumulate
forms and four Thumb scalar extensions. The original image independently grades
13,631,488 seven-word snapshots / 95,420,416 register words on both ARM11s.
Selected sixteen-bit value domains, rotations, two flag surrounds and accumulator
seeds are exhaustive within the documented slice. Component coverage is
10,081,152 observations; register aliases/conditions/NZCVQ/GE and pre-v6/profile/
reserved/PC isolation are checked. Executed negative controls, seven field faults,
seven malformed streams and original archive reproduction are required. Three
native checkpoints in the first sweep compare two-frame continuation output and
complete states. Full ARMv6/media/system behavior, timing/MMU/exceptions, firmware/
retail boot and platform peripherals remain pending. 3DS stays 30%. See the
[original source and domain](../tests/roms-src/3ds/extend_registers/README.md).

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
is newly complete; 3DS remains **30%**. See [source and finite domains](../tests/roms-src/3ds/saturation_registers/README.md).

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
unchanged; 3DS remains **30%**. See [original source and domains](../tests/roms-src/3ds/parallel_registers/README.md).

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
[original source and domains](../tests/roms-src/3ds/pack_difference_registers/README.md).

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
[original source and domains](../tests/roms-src/3ds/wide_media_multiply_registers/README.md).

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
[original source and domains](../tests/roms-src/3ds/dual_media_multiply_registers/README.md).

## Switch integer paired transfers (2026-10-11)

Switch 0.8.0 adds integer LDP/STP/LDNP/STNP/LDPSW with signed offsets and
pre/post-index writeback. The original NSO independently grades **2,490,368
snapshots / 24,903,680 actual register words**, covering all 128 displacement
fields, 19 variants, X8/SP bases, two flag surrounds and repeated-byte seeds.
Executed result/flag/offset mutations reject all 1,520 short rows; unsigned-load
replacement and skipped stores reject all 240 signed-load/640 store rows.
Ten field faults and seven malformed streams reject. The independent byte oracle
and **2,294,067** component observations check truncation, sign extension,
byte order, aliases, base updates and ordered faults. Production replay compares
actual captures and complete 4,194,648-byte states. SP diagnostic 6 and partial
second-access faults have complete machine-state evidence. The state layout is
unchanged; older readers cannot restore diagnostic 6. Full CPU/MMU/exceptions,
physical memory ordering/timing, native firmware/HOS/retail boot, graphics/audio,
controllers and netplay remain pending. Switch stays **25%**; CI/CD remains
disabled. See [original source and finite domains](../tests/roms-src/switch/pair_transfer_registers/README.md).

## Switch single-register memory transfers (2026-10-11)

Switch 0.9.0 adds signed loads and unscaled, unprivileged, pre/post-index and
extended-register offset integer transfers. NZCV, SP/XZR, W upper clearing,
signed byte offsets, UXTW/LSL/SXTW/SXTX scaling and aliases are verified.
The original NSO independently grades **7,188,480 snapshots / 57,507,840 actual
register words**. All signed 9-bit fields have correlated base/flag surrounds;
unsigned fields and register indices are finite selections. The component passes
**17,457,707 observations**, including complete unsigned fields, register aliases
and diagnostic/fault behavior. Complement memory guards ensure off-by-one
controls are observable. Executed result/flag mutations reject 3,328 short rows;
wrong immediate offsets, sign extension and skipped stores reject 1,664/640/
1,024 rows. Eight field faults and seven malformed streams reject. Actual
production captures and complete states replay; single-access SP/cross-map faults
and unprivileged text protection have machine evidence. ABI/state layout remain
unchanged. The supplied process permissions do not establish EL/MMU/SCTLR/UAO
behavior or precise architectural exceptions. Literal/prefetch/atomic/SIMD memory
families, full CPU/platform behavior, timing, firmware/HOS/retail boot,
graphics/audio, controllers and netplay remain pending. Switch stays **25%**;
CI/CD is disabled. See [original source and domains](../tests/roms-src/switch/single_transfer_registers/README.md).

## Switch production netplay (2026-10-11)

Switch **0.10.0** implements `INetplayCapable` with two independently owned
synthetic 32-bit input words and allocation-free fast serialization, restoration
and FNV-1a hashing over the entire canonical state. `VNXS0002` is **4,194,664
bytes**; legacy `VNXS0001` states load with both new inputs neutral. Invalid
states and short save buffers preserve their destinations. Ordinary local frames
supply the host word and neutral joining word. These are test-profile inputs;
**native Joy-Con/HID and HOS input services remain unfinished**.

The [original NSO probe](../tests/roms-src/switch/netplay_probe/README.md) publishes eight actual guest RAM words.
Two production TCP processes after different prior histories match all eight
independently derived results and complete offline baseline states over **100
confirmed frames**, including actual rollback. Executed wrong-host and
wrong-client inputs each mismatch three words and reject; eight field faults,
four malformed register streams, ROM mismatch and four malformed transport
frames reject. The adapter replays 64 individually selected input bits. The
input-port component passes 786,432 observations across 65,536 correlated pairs.
Audio is unimplemented and the test requires zero audio chunks. Graphics,
firmware/retail boot, complete CPU/MMU/hardware/timing and native devices remain
pending. Switch earns the production netplay delivery milestone: **30% = 6/20**.
CI/CD remains disabled. Artifacts: `build/artifacts/netplay-switch/`.

All **32 Switch gates and six existing production TCP netplay regressions** pass
(38 total, 119.00 seconds); all **148 original archives** reproduce. Release
build, completion rendering and whitespace checks pass. The prior Switch plugin
fails the new adapter test because it lacks the netplay interface.

## GameCube/Wii production rollback (2026-10-11)

The shared PowerPC core **0.11.0** exposes `INetplayCapable` for two independently
owned digital GameCube controllers. Netplay feeds native settled SI mode-3
transactions on ports 0/1 and leaves ports 2/3 unplugged. Fast save/load/hash
allocate no memory; all **25,166,340 bytes** of CPU, counters, 24 MiB RAM and SI
state are represented. `VPPC0002` layout is unchanged; legacy `VPPC0001` loads
with neutral SI input. Invalid states and short buffers preserve their destinations.

Original [GameCube](../tests/roms-src/gc/netplay_probe/README.md) and [Wii](../tests/roms-src/wii/netplay_probe/README.md) probes each publish eight actual
guest RAM words. Two independent production TCP peers after different prior
histories match these independent arithmetic/wire references and complete offline
states across **100 confirmed frames**, with actual rollback. Each executed
wrong-host/wrong-client capture corrupts three words and rejects. Eight field
faults, four register stream faults, ROM mismatch and four malformed transport
frames reject. Each adapter replays 64 input-bit cases and checks port ownership,
full-state hash, invalid-state guards, legacy migration and reset/unload.

Native analog, polling/wire timing, four-port multiplayer, origin/rumble, complete
CPU/exceptions/MMU, firmware/retail boot, graphics/audio and Wii Remote/IOS remain
unfinished. No audio is emitted or credited. Both platforms earn the production
netplay milestone: **30% = 6/20 delivery milestones**. Artifacts remain under
`build/artifacts/netplay-gc/` and `build/artifacts/netplay-wii/`; CI/CD is disabled.

Final binaries pass all eight affected PowerPC gates (38.09 seconds), and all
seven existing production TCP core regressions pass. All **150 original archives**
reproduce byte-for-byte. Release builds, completion rendering and whitespace
checks pass. The prior GameCube plugin fails the new adapter gate because it
lacks the netplay interface. The controller layout exposes two player slots;
the ordinary one-word frame method retains one connected controller.

## Native software controller input (2026-10-11)

Virtual Boy **0.2.0** consumes ordinary frontend input through native
software-clocked SCR/SDLR/SDHR transactions. The layout exposes both D-pads,
A/B, L/R, Start and Select; default keyboard/gamepad routes and remap persistence
are verified. The right D-pad uses W/A/S/D or a thresholded right stick. See the
[original cartridge and domain](../tests/roms-src/vb/controller_serial/README.md), grounded in the
[hardware reference](https://files.virtual-boy.com/download/978651/stsvb.html).

All **16,384 button combinations** across three access widths and four selected
mirrors produce **196,608 guest snapshots / 1,572,864 actual WRAM words** matching
independent grading. Executed wrong-capture and missing-clock guests reject
16/48 words across sixteen short rows each; eight field faults and four bad
streams reject. The component passes **311,552 observations**, including latch
isolation, partial clock-state restore, relatch and control/request validation.
Undefined partial data retention and wide-read upper zeros are explicit profile
policies, not physical hardware accuracy claims. Automatic hardware reads,
interrupt producers and physical timing remain unfinished.

Complete state format **`VVBS0003` = 66,208 bytes** appends forty pad-state bytes
without moving the CPU/WRAM fields. `VVBS0002` loads with neutral pad state.
Invalid pad fields reject before any machine mutation; pending software reads
and selected HALT continuations replay. The component caught and fixed an
oversized control-field validation bug. Old readers cannot load new states.

All seven affected controller/state/input gates pass, including the production
GUI input regression with Xvfb. All **151 original archives** reproduce. Release
and final target builds, completion rendering and whitespace checks pass. The
prior plugin fails the new capture contract because it has no native controller
layout. Virtual Boy earns the digital-input milestone: **25% = 5/20**. Full CPU/
NVC, VIP/VSU, native boot, saves, netplay and complete hardware/timing remain
pending; CI/CD stays disabled.

## Nintendo V810 integer extensions (2026-10-11)

Virtual Boy **0.3.0** adds XB, XH, source-register REV, signed **17-bit** MPYHW
and CLI/SEI ID manipulation. The [original instruction cartridge](../tests/roms-src/vb/nintendo_registers/README.md)
emits eight actual register words per snapshot through the frontend-owned file
channel; it contains no expected answers or verdict. Independent grading matches
**16,252,928 snapshots / 130,023,424 actual words**, with zero mismatches.

The finite domain includes every halfword in the specified XB/XH/REV surrounds,
every 17-bit factor on four upper surrounds/four destination patterns/two flag
seeds, and all 262,144 writable PSWs for each of CLI/SEI. Canonical XB/XH source
fields are used. These results do not cover every 32-bit operand pair, physical
timing, noncanonical encodings or the whole CPU. The signed 17-bit quirk follows
the author's [hardware experiment](https://www.virtual-boy.com/forums/t/nintendo-instruction-cycle-counts/); encodings and status effects follow the
[authored hardware reference](https://files.virtual-boy.com/download/978651/stsvb.html).

Three executed faulty guests reject: wrong result and wrong PSW each fail all
1,854 short rows; signed-16 preprocessing fails 224 rows / 288 words, isolated
to multiply. Eight field faults and seven malformed streams reject. Binaries,
ROM, generator, CPU/profile sources and grader remain unchanged through capture
and grading; provenance is recorded alongside the actual files.

The component passes **35,657,783 observations**, comparing complete GPR/SR
arrays through all register aliases, signed 17-bit factors/all low flags and
all writable PSWs for six instruction families. It also checks reserved fields,
unsupported extensions and a tail fetch fault. An additional 65,536 debug-word
patterns verify actual hexadecimal transport and wrong-width rejection.
Complete machine states replay each family; the production shared plugin
replays active output and whole **66,208-byte `VVBS0003`** states for all six
families. The state format and legacy `VVBS0002` pad migration stay unchanged.

All five affected gates pass (full guest capture: **104.67 seconds**; four
component/controller/state regressions: **22.09 seconds**). All **152 original
archives** reproduce; completion rendering and whitespace checks pass. The prior
CPU fails the new register-alias contract. VB remains **25% = 5/20**: no whole-CPU
milestone is earned by this subset. Remaining CPU/NVC, caches, timing, VIP/VSU,
automatic controller polling, native retail boot, saves and netplay remain
unfinished. CI/CD stays disabled; captures remain ignored locally.

## Configured Virtual Boy cartridge SRAM (2026-10-11)

Virtual Boy **0.4.0** adds optional SRAM in the native cartridge region for
explicitly configured original cartridges. Seven power-of-two capacities from
1 KiB through 64 KiB are supported, with board/module mirrors, little-endian
storage, reset retention, battery import/export and complete SRAM states.
See the [original cartridge RAM corpus](../tests/roms-src/vb/cartridge_ram/README.md) for the descriptor, full finite
domain and remaining physical/native-cartridge scope.

The actual shared-plugin guest matches **8,388,608 eight-word snapshots /
67,108,864 words**, zero mismatches, through every byte/value of the 8 KiB module
and four selected mirrors. Neighbor guards and preserved status are graded
independently. All exported battery bytes match the actual guest's final writes.
Executed wrong-read and missing-store controls fail 960/1,023 short rows and
960/2,046 words; eight field faults and seven malformed streams reject.
Capture sources/binaries/ROM/assembler/grader hashes remain unchanged.

The component checks **136,839,168 bus observations**, across every byte/value
of all seven capacities/four mirrors and repeated-half patterns on both wide
widths at selected wrap/end addresses. It checks reset retention, state replay,
legacy neutral initialization and invalid image/state/boundary atomicity.
The production frontend writes/restores exact `.sav` bytes through unload,
reset, reload and fresh plugin-manager sessions. Shutdown flushes data; invalid
battery lengths/files reject before mutation. Tests use isolated save paths.

RAM-bearing state format **VVBS0004** appends an eight-byte capacity and all
SRAM bytes after the prior CPU/WRAM/pad prefix: **66,216 + capacity bytes**.
No-RAM ROMs keep **VVBS0003 = 66,208 bytes**. Legacy VVBS0002/VVBS0003 states
initialize neutral SRAM. Active native complete-state and output replay match;
older readers cannot load VVBS0004.

All seven affected gates pass: full cartridge capture in **43.82 seconds**,
six component/controller/frontend/state regressions in **23.72 seconds**.
The old no-RAM Nintendo guest controls and all six native instruction-family
checkpoint continuations also pass. All **153 original archives** reproduce;
Release builds, completion rendering and whitespace checks pass. The prior core
fails the declared cartridge-capacity contract. VB remains **25% = 5/20**.
Native cartridge RAM-size detection, physical width/lane wiring, wait states
and complete persistent-media scope remain unverified, so no saves milestone
is earned. Remaining CPU/NVC, graphics/audio, timing, native retail boot and
netplay are unfinished. CI/CD stays disabled.

### N64 private castle lobby continuation (2026-10-11)

The [local private SM64 US regression](../tests/n64_retail_castle_entry.py)
replays signed-stick input from the existing standing courtyard checkpoint.
It grades Lakitu's `ACT_READING_NPC_DIALOG`, the rendered VI-field dialogue,
A-button dismissal against a neutral control, the castle door-pull action
against neutral stick input, and a lobby warp with changed guest terrain and
camera pointers. The final private capture shows Mario inside the castle;
states, frames and the commercial ROM stay ignored locally. The route has no
terminal CPU/RSP/DP stop. Main-level progression and full-title playability
remain unverified, so N64 remains **50% (10/20)** of the fixed delivery list.

## N64 functional 4 Kbit EEPROM (0.58.0)

The cartridge Joybus channel now implements 4 Kbit EEPROM identification and
eight-byte read/write commands. The [original guest corpus](../tests/roms-src/n64/eeprom_4k/README.md)
grades **16,512 actual SI/PIF cases / 33,024 register-file words**: blank reads
from every block, 256 writes and immediate reads per block, and a final read
of all 64 blocks after the whole device has changed. A fresh frontend process
reloads the exact 512 saved bytes, a malformed file is rejected, and an
executed wrong-read cartridge fails the host oracle. S032 states preserve the
EEPROM across rollback; older states import erased storage. Busy timing,
16 Kbit EEPROM, other save media, RTC and accessories remain unverified, so the
N64 checklist stays **50% (10/20)**.

## N64 16 Kbit EEPROM evidence (0.59.0)

The [original 16 Kbit cartridge](../tests/roms-src/n64/eeprom_16k/README.md)
grades **66,048 SI/PIF guest cases / 132,096 register-file words** across all
256 blocks. Its host oracle checks a 2,048-byte cross-process save reload,
malformed data rejection and an executed wrong-read mutant. S033 state and
plugin tests check rollback and S032 import. This is bounded functional save
media coverage; busy timing, other save types and automatic 16 Kbit selection
for new native cartridges remain incomplete. N64 remains **50% (10/20)**.

## N64 native EEPROM media identity (0.60.0)

The [original synthetic native-header ROM](../tests/roms-src/n64/native_save_identity/README.md)
executes guest SI/PIF cartridge-status requests for 23 known 16 Kbit IDs,
four 4 Kbit controls and two byte-swapped formats. The production gate grades
all 30 guest identity responses and selected battery sizes. Full EEPROM data
coverage remains in the original 4/16 Kbit cartridges. The 30 captured guest
`$t2` values are written to
`build/artifacts/owned/n64_native_save_identity/registers.txt`. Unknown native IDs
still require a 2,048-byte save or explicit media selection for 16 Kbit.
N64 remains **50% (10/20)**.

## N64 absent controller accessory protocol (0.61.0)

The [original no-Pak cartridge](../tests/roms-src/n64/accessory_absent/README.md)
executes 2,048 guest SI accessory reads and writes over all four controller
ports and 256 address/payload seeds. The file oracle grades all 16,384 actual
guest register words, including complete 64-byte reply packets, and rejects
an executed wrong-register image. An independent component check covers valid
packet CRC and malformed lengths. Controller Pak insertion, storage and
persistence remain pending. N64 remains **50% (10/20)**.

## N64 Controller Pak storage (0.62.0)

Configurable 32 KiB Paks on four controller ports now answer addressed Joybus
read/write commands with address and data CRC handling. An exhaustive component
gate writes and rereads all 4,096 blocks across the four Paks. S034 snapshots
preserve Pak bytes and insertion status; the production frontend writes one
`.mpk1`–`.mpk4` file per enabled slot, with cross-session and malformed-file
gates. Fresh media formatting is deterministic. Original guest filesystem use,
other accessories, additional save media and physical serial timing remain
unverified. The fixed N64 delivery score remains **50% (10/20)**.

## N64 original Controller Pak guest corpus (0.64.0)

The [repository-built MIPS cartridge](../tests/roms-src/n64/controller_pak/README.md)
grades 12,540 full SI reply packets / 100,320 guest register words across
four ports, five system pages per fresh Pak, a synthetic one-page note on each
port and 4,096 storage blocks, followed by 131,072 final Pak bytes. Host
validation checks copied ID headers, checksums, empty and allocated inode
tables, the note directory, 122 free pages per Pak and note data from guest reads.
The harness rejects an executed wrong-register image and three malformed
files. Retail game filesystem APIs and physical timing remain unverified.
The fixed N64 score is **50% (10/20)**.
