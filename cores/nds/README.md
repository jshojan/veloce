# Experimental Nintendo DS core plugin

`nds_plugin` builds `nds.so`/`nds.dll`/`nds.dylib` and implements emulator ABI v2.
The frontend discovers it and selects it for `.nds` files. It executes original
ARM/Thumb integer homebrew on two independent CPU contexts with shared main RAM and
CPU-specific WRAM access. **It does not boot retail DS games or DSi software.**
Full platform completion is unknown.

## Implemented subset

- All sixteen ARM data-processing operations: AND/EOR/SUB/RSB/ADD/ADC/SBC/RSC,
  TST/TEQ/CMP/CMN, ORR/MOV/BIC/MVN. Register/immediate operands, NZCV flags,
  carry/borrow, signed overflow, flag-preserving forms and source aliases.
- MUL/MLA and signed/unsigned long multiplication/accumulation: both result
  words, modulo accumulation, S set/clear, N/Z and preserved defined flags.
  ARM7 unpredictable multiply flags are retained as a model choice.
- LSL/LSR/ASR/ROR register/immediate shifters, RRX, zero/32/greater-than-32
  counts and rotated immediate operands. Logical operations preserve V.
- All ordinary ARM conditions, relative B/BL, BX and ARM9 register BLX, PC operands,
  ARM-state writes to PC and MRS CPSR/MSR CPSR flags. ARM9 flags include writable
  Q. The model ignores ARM7 reserved Q writes; the ARM7 ROM never sets this
  undefined bit and does not grade it as hardware behavior. Control/mode writes
  and SPSR are pending.
- Thumb immediate/register shifts, all sixteen low-register ALU operations,
  ADD/SUB register and small immediate forms, MOV/CMP/ADD/SUB large immediates.
  High-register ADD/CMP/MOV, PC/SP address generation and SP adjustment,
  conditional/unconditional branches, BX and ARM9 register BLX. ARMv4 Thumb MUL
  C is unpredictable and retained as a model choice; equal MUL source/destination
  and pre-ARMv6 low/low high-register encodings stop diagnostically.
- Thumb adjacent two-halfword BL calls on both CPUs and BLX immediate calls on
  ARM9: signed high displacement, unsigned low displacement, tagged return LR,
  word-aligned BLX targets and Thumb-to-ARM transition. Undefined odd BLX and
  unavailable ARM7 BLX stop diagnostically; calls into their own suffix also
  stop. These stops do not model physical exceptions.
- ARM9 LDR-to-PC selects ARM/Thumb state. Thumb PC reads are instruction address
  plus four; high-register PC writes stay in Thumb, and PC-relative address
  generation word-aligns the PC. Normal BX targets select state with bit zero.
- LDR/STR word and LDRB/STRB byte forms, immediate/shifted-register offsets,
  pre/post indexing, write-back and unaligned word-load rotation.
- ARM STRH/LDRH/LDRSB/LDRSH with split immediate/register offsets and
  offset/pre/post indexing. Thumb register, scaled immediate, SP-relative word
  and PC-relative literal transfers, including signed byte/halfword loads.
  Odd halfwords stop diagnostically. Unaligned Thumb word alignment/rotation
  is a model choice, outside the original ROM hardware grade.
- Thumb PUSH/POP and STMIA/LDMIA with ascending register/address order,
  descending PUSH and increment-after POP/block transfers. LDM base-in-list
  keeps its loaded value; STM base-in-list stores the original base when it is
  the lowest listed register. Empty lists and other STM base aliases stop
  diagnostically. ARM9 POP-PC interworks; ARM7 ignores bit zero and stays Thumb.
  Unaligned addresses align each word without rotation in the supplied
  alignment-disabled model; physical alignment/CP15 behavior is unverified.
- 4 MiB main RAM with four mirrors, shared between CPUs; 64 KiB ARM7 private
  WRAM and mirrors; 32 KiB shared WRAM with all four WRAMCNT allocations,
  bank mirrors, ARM9 control and ARM7 status. Missing ARM9 allocation stops
  diagnostically; physical undefined/open-bus reads are not supplied.
- Twelve digital buttons latched from frontend input before each frame.
  Active-low KEYINPUT halfword/byte reads on both CPUs and ARM7-only EXTKEYIN
  X/Y reads. Debug/pen are released and lid open in the supplied profile.
  Legal writes to these read-only lanes are ignored. KEYCNT/IRQ, touch/SPI,
  lid/debug controls and wider accesses spanning other registers are pending.
  The plugin provides a twelve-button configuration layout.
- Validated direct segment loading from original marked cartridges, full
  CPU/RAM save states, bounded dual-CPU scheduling and register-file output.

The original CPU implementation follows the
[ARM architecture manual](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).
DS memory layout is grounded in [BlocksDS memory documentation](https://blocksds.skylyrac.net/docs/internal/memory_map/)
and the original [GBATEK WRAM reference](https://doc.kodewerx.org/documents/gbatek.html#dsmemorycontrolwram).
This core owns its CPU bus interface; it does not inherit GBA BIOS calls, memory
mapping, cartridge prefetch or timing. No SDK, commercial ROM, firmware, BIOS,
Nintendo logo or copied third-party emulator source is included.

## Original cartridge profile

Only images of 512 bytes through 16 MiB with `VDSHB001` at header offset `$180`
and DS unit code zero load. Normal `.nds` ARM9/ARM7 segment descriptors at `$20`
and `$30` specify ROM offset, entry, load address and size. Both segments must be
nonempty, word aligned, disjoint in file and memory, within the image, and have
an entry instruction inside their loaded bytes. ARM9 loads into canonical main
RAM; ARM7 loads into main RAM or its private WRAM. Invalid images leave the
active machine unchanged.

This is an emulator test profile, not firmware boot. It does not validate a
Nintendo logo/header CRC, decrypt secure areas or execute BIOS/firmware. On
load/reset the profile clears RAM and undefined registers, copies both segments,
sets their PCs to their entries, sets CPSR `$DF` (System ARM state, IRQ/FIQ
masked), and assigns all shared WRAM to ARM7. These are supplied test conditions,
not evidence of physical power-on or firmware register values. The ROM remains
immutable; loaded code in RAM is writable. DSi unit codes are rejected.

Emulator-only STRB to `$04FF0000` emits an observed character. STR to `$04FF0004`
marks that CPU finished; only both CPUs finishing terminates the frontend with
unknown status. These addresses are not DS peripherals. The frontend owns the
file and its identity/trailer. Word STR to `$04FF0008` emits the observed
32-bit value as exactly eight lowercase, zero-padded hex digits in one sink
write; it adds no separator, verdict or register computation. This is another
emulator-only transport address. The original byte port remains supported.
The host derives expectations independently; the
ROM contains no expected answers or pass verdict. The 16-bit memory-watch API
accesses the first 64 KiB of main RAM.

Each frame runs at most 100,000 iterations with two ARM9 instruction attempts
and one ARM7 attempt per iteration. Completed CPUs stop immediately. The ratio,
60 Hz and 67 MHz metadata are scheduling choices, not measured bus/pipeline
cycles. Cycle count is the sum of CPU step attempts. There is no cache, MPU,
TCM, asynchronous bus owner or memory contention model.

## Measured coverage and verification

The original [dual-CPU integer corpus](../../tests/roms-src/nds/integer_registers/README.md)
matched **44,238,336/44,238,336 four-word snapshots**, zero mismatches,
across all sixteen data-processing
operations, repeated-byte operand pairs and sixteen full-width boundaries.
Every NZCV input runs on both CPUs; ARM9 also runs both incoming Q states. Each
snapshot captures destination, CPSR and unchanged source registers. ARM9 writes
all its output, signals through main RAM, then ARM7 writes its output. This
exercises two actual guest programs and shared storage through frontend/plugin
discovery, not a host CPU shortcut. No whole-platform percentage is established.

The original [multiply corpus](../../tests/roms-src/nds/multiply_registers/README.md)
matched 38,338,560/38,338,560 five-word observations with zero mismatches
for six multiply/accumulate families,
S set/clear, repeated-byte pairs, full-width boundaries and four accumulator
seeds. Every defined result/source/CPSR bit is independently graded; ARM7 S
forms exclude only their architecturally unpredictable C or C/V bits. Physical
multiply latency and full operand/accumulator domains remain unverified.

The original [Thumb corpus](../../tests/roms-src/nds/thumb_registers/README.md)
matched 51,426,816/51,426,816 five-word observations with zero mismatches
over sixteen ALU operations, register
ADD/SUB, every immediate encoding for shifts and low register ALU operations, all NZCV and ARM9 Q states.
Every case switches ARM → Thumb → ARM and captures the Thumb PC read alongside
result/CPSR/sources. ARM7 MUL's unpredictable C is excluded from scoring.

The original [memory corpus](../../tests/roms-src/nds/memory_registers/README.md)
matched 49,694,208/49,694,208 nine-word observations with zero mismatches for ARM halfword/signed and Thumb
memory transfers. It captures destination/CPSR/base/offset/source, the containing
memory word, two guards and SP. Every recorded bit is graded; no bits are excluded.

The original [long-call corpus](../../tests/roms-src/nds/long_calls/README.md)
matched **9,437,184/9,437,184 eight-word snapshots**, zero mismatches and no
unscored bits. Valid adjacent BL/ARM9 BLX pairs cover every high field and every
valid low field individually, both source halfword positions, sixteen incoming
LR seeds and all NZCV/ARM9 Q inputs. It observes link/CPSR/target-PC/input fields
and an ARM-target memory store of LR. It patches and restores loaded RAM under
the supplied cache-free profile; physical instruction-cache coherence is unverified.

The original [stack/block corpus](../../tests/roms-src/nds/multiple_registers/README.md)
matched 3,720,960/3,720,960 twenty-five-word snapshots,
zero mismatches, across every defined nonempty
PUSH/POP list and every STMIA/LDMIA list/base combination, sixteen distinct
register/memory seeds and all NZCV/ARM9 Q inputs. POP-PC exercises word-aligned
ARM9 ARM targets and both Thumb halfword positions, including ARM7's even
Thumb target. Static thunks avoid runtime code patching. The host grades every
recorded bit; unpredictable lists are excluded from this hardware domain.

The original [keypad corpus](../../tests/roms-src/nds/keypad/README.md) supplies
all 4,096 digital combinations through the frontend input schedule and captures
ten-word snapshots: both CPUs' KEYINPUT halfword/byte reads, ARM7 EXTKEYIN
halfword/byte reads and a sequence counter. Unknown/reserved register bits are
captured but excluded from hardware scoring. All 4,096 snapshots matched with
zero mismatches.

The seven original ROMs use the word transport for register observations and the
byte transport for text framing. The four earlier corpora retain their instruction
domains, snapshot fields and independent grading. The integer builder retains `word_output=False`, which
reproduces the previously committed byte-output image exactly. Its reduced
347,136 observations match byte-for-byte between both transports, with both
streams independently graded. In that supplied scheduling profile, native word
output used 33,910,835 instruction attempts versus 152,399,921 for byte output;
these counts measure test-driver work, not physical DS cycles or latency.

`nds_machine_test` separately checks:

- 688,128 ALU cases with destination/source aliases and S set/clear. ARM9 Q
  remains set across every operation; every incoming NZCV is included.
- 1,179,648 multiply cases against a per-bit product oracle, four accumulator
  seeds, all NZCV and S set/clear, Rs/destination and accumulator aliases;
  PC/equal-long-destination diagnostics and failed-condition retirement.
  Rm/destination alias behavior on physical ARM7/ARM9 is not measured.
- 147,456 register/immediate shift cases against a per-bit oracle, plus 32,768
  rotated-immediate cases; every low-byte register amount and immediate amount.
- Condition truth tables on both architectures, signed branch/link boundaries,
  BX alignment, explicit unsupported coprocessor stops, and CPSR flags/Q.
- 393,216 Thumb ALU attempts over three destination/source arrangements,
  including 8,192 rejected unpredictable MUL aliases; independent arithmetic,
  per-bit shifts/product and all incoming NZCV with ARM9 Q set.
- 6,291,456 high-register attempts: every encoded register pair over boundaries
  and flags, including rejection of unpredictable low/low and CMP-PC operands.
  PC+4 reads, halfword PC writes and preserved ADD/MOV flags are checked.
- 131,072 PC/SP address cases and 8,192 SP adjustments, with every immediate
  and low destination, word-aligned PC calculation and 32-bit wrapping.
- All 114,688 conditional branch combinations (fourteen conditions, all flags
  and eight-bit offsets) and 4,096 unconditional branch offsets. ARM/Thumb
  BX/BLX target states, link addresses, ARM7 rejection, PC-load interworking,
  unsupported Thumb forms, halfword-aligned state replay and odd-PC rejection.
  Actual shared-plugin replay includes both CPUs in a Thumb loop.
- 20,971,515 defined adjacent BL/BLX encoding pairs, covering the combined
  high × low field matrix on both source positions and available architectures.
  Five combinations targeting their own suffix are unpredictable and excluded.
  Another 6,720 boundary/flag attempts include undefined/unavailable/self-suffix
  diagnostics. Actual shared-plugin frames end after a prefix on both CPUs;
  saving there and replaying the suffix produces byte-identical whole states,
  including ARM9 BLX interworking. Existing serialized LR/PC/CPSR preserve this
  continuation without changing the state format.
- 393,216 ARM halfword/signed transfer attempts: every eight-bit offset,
  six addressing forms, distinct immediate/register inputs, four byte lanes
  and four flag patterns, including
  147,456 odd-half diagnostics. Another 128 offset-form cases cover aliases.
- 524,288 Thumb register transfer cases over all 512 register triples and
  eight operations; 30,720 immediate/SP/PC cases cover every encoded offset
  and low destination. Byte references check signed extension, store width,
  neighboring bytes, preserved flags, PC alignment and exact writeback.
  Reserved/unmapped transfers and failed conditions check rejection atomicity.
- 3,327,744 defined Thumb stack/block transfers and 604,416 rejected
  unpredictable list attempts, all NZCV/ARM9 Q, sixteen operand seeds, exact
  register/memory ordering and guards. Another 24 unaligned model checks,
  two undefined ARM9 POP target checks and eight injected transaction failures
  verify alignment, rejection and the completed store prefix. Failed loads
  commit no registers; failed stores retain earlier writes and commit no
  registers/writeback. Those diagnostics do not model Data Abort behavior.
  Actual shared-plugin PUSH/POP-PC loops use distinct CPU stacks and reproduce
  whole states exactly after restore/replay.
- All 65,536 offsets on each CPU in each of four shared-WRAM modes, private
  fallback, bank mirroring, access rights, main-RAM coherence/mirrors, rejected
  cross-region write atomicity, byte loads and unaligned word-load lanes.
- Word output covers every byte value at each position on both CPU buses,
  legacy-byte equivalence, leading zeros/case/order, guest STR preservation,
  rejected widths/addresses, write-only reads and unbound sinks. Actual plugin
  replay emits distinct words from both CPUs and finishes only when both finish.
- All 4,096 digital masks with irrelevant upper ABI bits ignored, both CPU
  access rights, byte lanes, ignored read-only writes and invalid-width/range
  rejection. Input save/restore, malformed input rejection, neutral reset and
  legacy-state migration are checked. Actual plugin tests verify layout controls,
  held/released input and exact state restoration.
- Exact dual-CPU/RAM replay through machine and actual shared-plugin states;
  nine malformed state variants and eight malformed cartridge variants, with
  rejection atomicity. Plugin dimensions and unload behavior are also checked.

`VNDS0002` states contain explicit big-endian scalars for image identity/CRC,
frame count, completion and WRAM mode, both implemented CPU contexts/step counters/
diagnostic flags, followed by all three RAM banks and the normalized twelve-bit
pressed-button latch as one eight-byte scalar. Exact size/version/image,
mode, boolean, PC alignment (word for ARM, halfword for Thumb), and defined
CPSR/T and input-mask checks precede mutation. Exact legacy `VNDS0001` states
remain accepted with released keys; CPU/RAM scalar offsets are preserved.
New states use `VNDS0002` and are not accepted by older plugin builds.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce nds_plugin nds_machine_test -j
ctest --test-dir build -R '^nds_' --output-on-failure
```

Raw observations/reports remain in `build/artifacts/owned/nds_integer/`,
`build/artifacts/owned/nds_multiply/`, `build/artifacts/owned/nds_thumb/`,
`build/artifacts/owned/nds_memory/`, `build/artifacts/owned/nds_long_calls/`,
`build/artifacts/owned/nds_multiple/` and `build/artifacts/owned/nds_keypad/`,
and are uploaded by owned-hardware CI. Generator/image SHA256 are recorded
in the ROM manifest. Component evidence is separate from the original-ROM domain.

## Remaining hardware and features

ARM block transfers, ARM BLX immediate,
ARM9 DSP/saturation and other architecture extensions,
ARM doubleword transfers, SWP, privileged modes,
banked registers, SPSR, exceptions/interrupts, CP15/MPU/cache/TCM, firmware and
BIOS, gamecard/secure-area/save protocols, IPC, DMA, timers, KEYCNT/keypad IRQ and touchscreen,
SPI/RTC, Wi-Fi, graphics/VRAM/palette/OAM, 3D, sound and physical timing remain.
Valid unimplemented instructions and unmapped transactions set a serialized
`unsupported` flag rather than fabricate results or deliver hardware exceptions.
Unpredictable/reserved encoding behavior and PC-store implementation details are
not independently hardware verified. The declared arithmetic cases use ordinary
non-PC registers; the Thumb corpus records PC reads separately. The ARM integer
corpus uses an unshifted second operand; Thumb includes immediate/register shifts. Physical undefined BX alignment
and PC-store behavior are not hardware-scored.

Video is a blank 256×384 combined buffer and audio is empty. Digital input
reaches the keypad registers; touchscreen and lid input remain pending.
Dual-screen presentation, physical controllers/touchscreen, this core's netplay,
Windows/macOS builds and physical-hardware comparison remain unverified or
unimplemented. Other cores retain their existing controller/network behavior.
DSi enhanced mode requires additional CPU, RAM, boot/security and peripheral
work and remains a separate missing platform capability.
