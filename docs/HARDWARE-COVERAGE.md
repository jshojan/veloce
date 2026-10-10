# Nintendo implementation and hardware test coverage

Current N64 evidence: [RDP texture transfers and current Mario evidence](#rdp-texture-memory-transfers). Earlier increment reports are historical.

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
| Nintendo 64 | `n64_plugin` (experimental homebrew/native handoff) | 28,127/28,127 defined integer results matched; 28,160 observations, with 33 undefined divide inputs unscored; doubleword multiply/divide: 262,623/262,623 defined LO/HI pairs matched on exhaustive byte pairs and full-width boundaries; merge memory: 3,158,016/3,158,016 observations matched at every lane on all repeated-byte pairs and selected full-width patterns; digital SI/PIF: 16,384/16,384 button combinations and 131,072 register values matched; interrupt gates: 65,536/65,536 cases and 524,288 register values matched; FPU controls: 8,192/8,192 cases and 65,536 register values matched | Not measured; retail boot, graphics/RSP audio microcode, analog/controllers beyond port 1 and most platform hardware remain |
| Virtual Boy | `vb_plugin` (experimental homebrew only) | 6,512,640/6,512,640 result/PSW pairs matched across ADD/SUB/CMP/OR/AND/XOR repeated-byte pairs and full-width boundaries, plus SHL/SHR/SAR patterns and all low-byte amounts; 4,210,688/4,210,688 multiply/divide snapshots matched with 8,704 divide-zero exception/RETI cases; 4,685,824/4,685,824 TRAP/illegal exception snapshots matched across all writable nonfatal PSW combinations and full reserved low-bit encodings; 4,982,784/4,982,784 address-trap snapshots matched across matching/disabled/aliased addresses and entry/return; 17,055,744/17,055,744 CAXI/port snapshots matched across comparisons/replacements, width/alignment, unsigned input and preserved flags; 14,680,064/14,680,064 arithmetic bit-string snapshots matched across eight operations, byte pairs, all offsets and twelve lengths; 100,712,448/100,712,448 immediate-instruction snapshots matched across all wide/short immediates for twelve instructions on sixteen patterns/all flags | Not measured; remaining CPU/NVC instructions, timing, VIP/VSU, controllers and most platform hardware remain |
| Nintendo DS | `nds_plugin` (experimental homebrew only) | 44,238,336/44,238,336 dual-CPU result/CPSR/source snapshots matched across all sixteen data-processing operations, repeated-byte pairs and full-width boundaries, every NZCV and both ARM9 Q states; 38,338,560 six-family multiply and 51,426,816 Thumb result/CPSR/source/PC observations matched with undefined ARM7 flags excluded; 49,694,208 nine-word ARM/Thumb memory and 9,437,184 eight-word long-call snapshots matched with every bit graded; 3,720,960 stack/block snapshots matched; 4,096/4,096 digital keypad combinations matched | Not measured; firmware/retail boot, ARM block transfers, interrupts, graphics/audio, peripherals and timing remain; DSi has a separate partial plugin |
| Nintendo DSi | `dsi_plugin` (original marked images only) | 387,072/387,072 original RAM captures; 67,109,008 component byte/width observations; 100-frame TCP host/viewer states and eight guest words match oracle | Not measured; firmware/retail boot, full SCFG/MBK, physical timing and most console hardware remain; [limits](../cores/dsi/README.md) |
| Nintendo 3DS | `ctr_plugin` (original `.v3ds` only) | 25,165,824 original reversal captures; 36,123,648 component register snapshots and 541,065,300 RAM bus observations; 4,096 digital inputs; 100-frame production TCP host/viewer states and twelve guest words match baseline/oracle | Not measured; native boot, remaining CPU/peripherals, GPU/audio, physical timing and wireless multiplayer pending; [limits](../cores/3ds/README.md) |
| GameCube | `gc_plugin` (original marked images only) | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Not measured; most CPU and console hardware remain; [limits](../cores/ppc/README.md) |
| Wii U | `wiiu_plugin` (original `.vwiiu` only) | 25,165,824 original immediate captures; 28,311,552 component register snapshots and 8,724,152,428 RAM bus observations; dense/sparse replay and synthetic digital input; 100-frame production TCP states/twelve guest words match baseline/oracle | Not measured; native boot, remaining CPU/peripherals, GPU/audio, GamePad, physical timing and wireless multiplayer pending; [limits](../cores/wiiu/README.md) |
| Wii | `wii_plugin` (original marked images only) | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Not measured; most CPU and console hardware, including MEM2/IOS, remain; [limits](../cores/ppc/README.md) |

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

Additional requested platform families without a core in this worktree include
Nintendo Switch. Famicom Disk System and other Nintendo accessories still need
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
baseline's complete 135,791,096-byte state; twelve guest words from ARM11_0,
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
stay synchronized. CI/CD remains disabled. Full regression verification is pending.

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
