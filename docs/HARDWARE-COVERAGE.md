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
| Nintendo 64 | `n64_plugin` (experimental homebrew/native handoff) | 28,127/28,127 defined integer results matched; 28,160 observations, with 33 undefined divide inputs unscored; doubleword multiply/divide: 262,623/262,623 defined LO/HI pairs matched on exhaustive byte pairs and full-width boundaries; merge memory: 3,158,016/3,158,016 observations matched at every lane on all repeated-byte pairs and selected full-width patterns; digital SI/PIF: 16,384/16,384 button combinations and 131,072 register values matched; interrupt gates: 65,536/65,536 cases and 524,288 register values matched | Not measured; retail boot, graphics/audio, analog/controllers beyond port 1 and most platform hardware remain |
| Virtual Boy | `vb_plugin` (experimental homebrew only) | 6,512,640/6,512,640 result/PSW pairs matched across ADD/SUB/CMP/OR/AND/XOR repeated-byte pairs and full-width boundaries, plus SHL/SHR/SAR patterns and all low-byte amounts; 4,210,688/4,210,688 multiply/divide snapshots matched with 8,704 divide-zero exception/RETI cases; 4,685,824/4,685,824 TRAP/illegal exception snapshots matched across all writable nonfatal PSW combinations and full reserved low-bit encodings; 4,982,784/4,982,784 address-trap snapshots matched across matching/disabled/aliased addresses and entry/return; 17,055,744/17,055,744 CAXI/port snapshots matched across comparisons/replacements, width/alignment, unsigned input and preserved flags; 14,680,064/14,680,064 arithmetic bit-string snapshots matched across eight operations, byte pairs, all offsets and twelve lengths; 100,712,448/100,712,448 immediate-instruction snapshots matched across all wide/short immediates for twelve instructions on sixteen patterns/all flags | Not measured; remaining CPU/NVC instructions, timing, VIP/VSU, controllers and most platform hardware remain |
| Nintendo DS | `nds_plugin` (experimental homebrew only) | 44,238,336/44,238,336 dual-CPU result/CPSR/source snapshots matched across all sixteen data-processing operations, repeated-byte pairs and full-width boundaries, every NZCV and both ARM9 Q states; 38,338,560 six-family multiply and 51,426,816 Thumb result/CPSR/source/PC observations matched with undefined ARM7 flags excluded; 49,694,208 nine-word ARM/Thumb memory and 9,437,184 eight-word long-call snapshots matched with every bit graded; 3,720,960 stack/block snapshots matched; 4,096/4,096 digital keypad combinations matched | Not measured; firmware/retail boot, ARM block transfers, interrupts, graphics/audio, peripherals and timing remain; DSi has a separate partial plugin |
| Nintendo DSi | `dsi_plugin` (original marked images only) | 387,072/387,072 original RAM captures; 67,109,008 component byte/width observations; 100-frame TCP host/viewer states and eight guest words match oracle | Not measured; firmware/retail boot, full SCFG/MBK, physical timing and most console hardware remain; [limits](../cores/dsi/README.md) |
| Nintendo 3DS | `ctr_plugin` (original `.v3ds` only) | 25,165,824 original reversal captures; 36,123,648 component register snapshots and 541,065,300 RAM bus observations; 4,096 digital inputs; 100-frame production TCP host/viewer states and twelve guest words match baseline/oracle | Not measured; native boot, remaining CPU/peripherals, GPU/audio, physical timing and wireless multiplayer pending; [limits](../cores/3ds/README.md) |
| GameCube | `gc_plugin` (original marked images only) | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Not measured; most CPU and console hardware remain; [limits](../cores/ppc/README.md) |
| Wii U | `wiiu_plugin` (original `.vwiiu` only) | 25,165,824 original immediate captures; 28,311,552 component register snapshots and 8,724,152,428 RAM bus observations; dense/sparse replay and synthetic digital input; 100-frame production TCP states/twelve guest words match baseline/oracle | Not measured; native boot, remaining CPU/peripherals, GPU/audio, GamePad, physical timing and wireless multiplayer pending; [limits](../cores/wiiu/README.md) |
| Wii | `wii_plugin` (original marked images only) | 49,152/49,152 arithmetic and 74,752/74,752 branch and 42,108,928/42,108,928 condition and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched | Not measured; most CPU and console hardware, including MEM2/IOS, remain; [limits](../cores/ppc/README.md) |

The experimental N64 plugin now executes original MIPS integer homebrew ROMs.
It accepts the documented homebrew and original/native IPL3 handoff profiles;
Super Mario 64 loads but faults at RDRAM chip configuration, with retail boot unverified. Its corpus observes 28,160 GPR/HI/LO results across 20 instructions,
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
socket permission; CI/CD remains disabled. All 58 original ROMs reproduce
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
All 58 original ROMs reproduce from their source/image ledger. CI/CD remains disabled.

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
All 58 original ROMs reproduce from the ledger. No overall hardware percentage
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
seven netplay gates pass. CI/CD stays disabled; all 58 original ROMs reproduce
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

The ledger now includes 58 original ROMs. Controller polling, analog axes,
origin calibration, VI/wire timing, CPU SI interrupts, accessories, four-port
frontend input and Wii Remote support remain pending. CI/CD remains disabled.
The [delivery completion ledger](../docs/CORE-COMPLETION.md) is separate from
hardware-domain pass fractions and legacy external-suite accuracy scores.
The optional `tests/n64_retail_probe.py` takes a locally supplied N64 ROM;
Super Mario 64 now loads and reaches 32,147 IPL3 step attempts, then faults at missing RDRAM device register `A3F80008`; boot/playability remain unverified. Its ROM bytes never enter git.

## N64 cartridge DMA and IPL3 handoff

The [PI corpus](../tests/roms-src/n64/pi_dma/README.md) matches 524,288 cases and
8,388,608 guest register values across all even lengths 2–65,536, four cartridge
offsets, two RDRAM addresses and both PI mask states. The [handoff corpus](../tests/roms-src/n64/ipl3_handoff/README.md)
matches twelve startup captures without bundling firmware. Executed guest mutants,
all captured-field mutants and seven invalid streams per corpus are rejected.
All thirteen N64 CTests pass; the PI component also passes undefined-behavior sanitization.
Settled aligned ROM reads and explicit HLE startup are implemented; RI/RDRAM
initialization, native retail boot, PI busy/misalignment/peripheral behavior,
graphics/audio and physical timing remain pending. The ledger contains 58 original
ROMs. CI/CD remains disabled.

## N64 RI configuration register evidence

The [original RI corpus](../tests/roms-src/n64/ri_registers/README.md) matches
524,288 cases and 3,145,728 guest reads. Every 19-bit refresh value and every
mode/configuration/selection combination is covered; latency is correlated.
Executed selection mutation, six field mutants and seven invalid streams reject.
The component checks current-load latching, access/state failure atomicity,
legacy conversion, exact replay, reset and loaded-plugin execution. RI refresh
scheduling, analog calibration, bus-error production, RDRAM chip discovery and
physical timing remain pending. Super Mario 64 advances to missing RDRAM device register
(A3F80008, EPC A40000F4, Cause 14) after 32,147 startup step attempts; it is
not bootable/playable. The corpus ledger contains 58 original ROMs.

## N64 MI control evidence

The [original MI corpus](../tests/roms-src/n64/mi_controls/README.md) matches
393,216 rows / 3,145,728 guest captures: mode command words across all eight
flag states and interrupt-mask commands across all 64 masks, with actual SI/PI
DMA requests pending. Executed count mutation, eight field mutants and seven
invalid streams reject. The component checks 16,777,216 mode transitions, all
mask transitions, injected DP delivery/acknowledgment/ERET, exact replay, legacy
conversion, atomic rejection and production-plugin execution; it also passes
undefined-behavior sanitization. All thirteen N64 gates pass. Stored MI bus flags
do not yet implement repeat/EBus/upper transaction effects or RDRAM chip discovery.
Super Mario 64 stops at A3F80008 (EPC A40000F4, Cause 14) after 32,147 startup
step attempts; retail gameplay, graphics/audio and physical timing remain pending.
All 58 original ROMs reproduce. CI/CD remains disabled.
