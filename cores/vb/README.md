# Experimental Virtual Boy core plugin

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Virtual Boy | **25%** | 5/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Virtual Boy delivered milestones: Native digital controller/keypad registers consume frontend input, Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: The standard pad software-clock path now consumes frontend input through native SCR/SDLR/SDHR, with all 16,384 button combinations independently graded. Remaining CPU/NVC, hardware controller polling/interrupts/timing, VIP/VSU, native boot, persistent media and netplay remain.
<!-- completion:end -->

`vb_plugin` builds `vb.so`/`vb.dll`/`vb.dylib` and implements emulator ABI v2.
The frontend discovers it alongside the other cores and selects it for `.vb`
images. It executes original V810 integer homebrew with 32-bit registers,
little-endian instruction/data memory and an explicit marked cartridge profile.
**It does not run retail Virtual Boy games.** Full platform completion is unknown.

## Implemented subset

- MOV register/immediate, ADD/SUB/CMP, AND/OR/XOR/NOT, and SHL/SHR/SAR
  register/immediate forms; Z/S/OV/CY flags and immutable r0.
- MUL/MULU full 64-bit products and DIV/DIVU quotient/remainder, signed overflow,
  carry preservation and r30/destination aliasing.
- TRAP (all 32 parameters), divide-zero and three reserved major opcode
  families (`$1B`, `$32`, `$36`): normal/nested context, fatal bus writes/halt and
  RETI. TRAP saves the next PC; illegal/divide faults save the instruction PC.
  Address traps compare the full PC against ADTRE before instruction fetch when
  AE is set; board/RAM aliases do not compare equal. Other exception producers
  and external interrupts remain pending.
- MOVEA, MOVHI, ADDI, ORI, ANDI and XORI; sign/zero extension and source aliasing.
- All 16 branch/SETF conditions, relative JR/JAL, register JMP and link r31.
  There are no MIPS-style branch delay slots.
- LD.B/LD.H sign extension, LD.W, ST.B/ST.H/ST.W, architectural alignment
  masking, 64 KiB WRAM storage/aliases and read-only cartridge storage.
- CAXI compare/exchange through one word transaction, comparison flags,
  unconditional mismatch write-back, word alignment and captured r30 operands.
- IN.B/IN.H zero extension, IN.W and OUT.B/H/W; separate port callbacks,
  source truncation, alignment and unchanged PSW. The marked test profile
  shares its storage/debug decoder with ports; physical port wiring is pending.
- Eight arithmetic bit-string operations (OR/AND/XOR/MOV and complemented
  forms), low-to-high transfers, preserved PSW and working r26–r30. Execution
  is bounded at word boundaries with resumable source/destination buffers.
- XB/XH low-byte/halfword exchanges, source-register REV, signed 17-bit MPYHW,
  and CLI/SEI ID manipulation; whole PSW/r30 preservation where applicable.
- Basic LDSR/STSR for saved PC/PSW, ECR, PSW, ADTRE and cache-enable storage.
  ECR is read-only; reserved PSW bits and system-PC low bits are masked.
  Cache commands beyond enable storage stop as unsupported; no cache executes.
- Reset-vector execution from `$FFFFFFF0`, reset PSW `$8000`, ECR `$FFF0`,
  HALT, full CPU/WRAM save states and streaming test output.

The original implementation follows the
[NEC V810 architecture manual](https://www.virtual-boy.com/downloads/978644/get/978650),
including the instruction definitions, flag tables and reset table. Board mapping
is a storage subset; physical hardware has not been tested.
No Nintendo SDK, commercial ROM, copyrighted logo or copied emulator source is
included.

## Original cartridge profile and result channel

Only power-of-two `.vb` images from 1 KiB through 16 MiB with `VVBHB001` at
`size - $200` load. This marker opts into the repository's experimental profile;
unmarked retail images and malformed lengths are rejected without changing the
active machine. The ROM maps to `$07000000–$07FFFFFF` with cartridge-size mirroring
and a 27-bit board address mask. The CPU starts at its architectural reset vector;
the original ROM supplies its own reset instructions to jump to `$07000000`.
WRAM maps at `$05000000` and mirrors its 64 KiB storage through region 5. RAM and
architecturally undefined registers are zeroed by the test profile on load/reset.

Emulator-only `ST.B` to `$04000000` emits one observed character; `ST.W` to
`$04000004` finishes with unknown status; `ST.W` to `$04000008` emits the
actual word as eight hexadecimal characters. These debug addresses are not Nintendo
hardware. The frontend owns the output file, including ROM CRC32 and channel
metadata. The host grader derives expected values independently; the ROM has no
expected results or pass verdict. The 16-bit RAM-watch API maps all WRAM.

## Measured coverage

The original [V810 integer corpus](../../tests/roms-src/vb/integer_registers/README.md)
matched **6,512,640/6,512,640 result/PSW pairs** across nine register operations:

- ADD/SUB/CMP/OR/AND/XOR: every pair of 256 repeated-byte words, plus 16×16
  full-width patterns, each with all 16 incoming Z/S/OV/CY states.
- SHL/SHR/SAR: 16 full-width patterns × all 256 low-byte shift amounts × all
  16 incoming flags, including zero-count carry clearing and masked amounts.

This is 100% of that declared finite domain, **not exhaustive over 32-bit
operands or a percentage of Virtual Boy hardware implemented**. CMP observations
include the unchanged destination and the actual resulting PSW. An executed
ADD-to-SUB mutation fails only the ADD groups; seven damaged files return ERROR.
The full run exercises actual shared-plugin discovery through the frontend.

The original [multiply/divide corpus](../../tests/roms-src/vb/muldiv/README.md)
matched **4,210,688/4,210,688 nine-word register snapshots**, zero mismatches,
from MUL/MULU/DIV/DIVU,
including **8,704 zero-divisor cases** with captured exception registers and
RETI return. Every signed/unsigned byte pair and full-width boundary pair runs
with all 16 incoming flag states. Arithmetic and exception-capture mutations
failed as required, and seven damaged files returned ERROR.

The original [exception corpus](../../tests/roms-src/vb/exceptions/README.md)
matched **4,685,824/4,685,824 thirteen-word snapshots**, zero mismatches: all 32 TRAP parameters and three
illegal opcode families across every writable PSW combination with NP clear,
plus every low-bit illegal encoding with all flags and both EP states. It
records entry and post-RETI context, both saved-register banks, persistent ECR
code halves, vector markers and unchanged GPRs.

The original [address-trap corpus](../../tests/roms-src/vb/address_traps/README.md)
matched **4,982,784/4,982,784 fourteen-word snapshots**, zero mismatches,
including 131,168 matching enabled
cases. It covers all writable nonfatal PSWs on six address relationships, all
single-bit PC flips and all low-half offsets. Raw ADTRE low-bit masking,
normal/nested entry, saved AE clearing, RETI retry and nonmatching execution
are graded from actual guest registers.

The original [memory transaction corpus](../../tests/roms-src/vb/memory_ops/README.md)
matched **17,055,744/17,055,744 nine-word snapshots**, zero mismatches: CAXI repeated-byte comparisons and
replacements/full-width triples, and OUT/IN pairs on every byte or repeated-half
value. All four address low-bit lanes and all incoming flags are included.
Loaded results, comparison/preserved PSW, stored memory, adjacent guards and
original source registers are graded independently.

The original [arithmetic bit-string corpus](../../tests/roms-src/vb/bitstrings/README.md)
matched **14,680,064/14,680,064 fourteen-word snapshots**, zero mismatches,
across all eight operations,
every repeated-byte pair, all source/destination offsets and twelve lengths
through 65 bits, with all incoming flags. It grades destination memory, unchanged
source/guard words, working registers and PSW. Overlap and physical bus timing
are outside its declared domain.

The original [immediate-instruction corpus](../../tests/roms-src/vb/immediates/README.md)
matched **100,712,448/100,712,448 three-word snapshots**, zero mismatches:
every 16-bit encoding for six
wide-immediate instructions and every 5-bit encoding for six short-immediate
forms, on 16 full-width patterns and all incoming low flags. The ROM executes
changing instructions from WRAM and records destination, PSW and source.
Full 32-bit operands and physical cache coherency remain outside this domain.

`vb_machine_test` separately checks reset entry, all branch conditions/flag
states on signed displacement boundaries, SETF, JR/JAL/JMP/link behavior, r0,
source aliasing, immediate/register decoding, wide-immediate reference vectors,
flag preservation, alignment/extension/byte order, WRAM mirrors, ROM protection,
system-register masks, HALT/unsupported stops, rejected ROM/state atomicity and
full state equality after continuation through the actual shared plugin.
It also checks multiply/divide aliases, nested exception/RETI priority, fatal
bus writes/halt and full saved exception continuation. An additional 3,178,496
TRAP/illegal-instruction component cases include fatal entry and RETI,
all listed encodings, condition/control combinations, and sticky-flag/interrupt
level boundaries.
Another 12,582,912 address-trap cases cover every writable PSW, six address
relationships at eight full-PC boundaries, fatal state/bus writes, pre-fetch
delivery and ordinary nonmatching execution. Armed-breakpoint and handler
save-state replay also match exactly.
CAXI/port checks also cover original operands through one atomic callback,
profile read/write order and mismatch write-back, r0/r30/base/destination aliases,
every writable PSW for all port widths, all signed displacements/address wrapping,
rejected callback CPU state and full saved transaction continuation.
Bit-string checks compare 3,145,728 transfers with a per-bit byte-array oracle,
preserve every writable PSW in 2,097,152 cases, bound maximum-length execution,
and replay pending source/destination buffers exactly through saved states.
Malformed/legacy buffered states and a diagnostic bus stop are also checked.
These component checks are separate from the original-ROM domain.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' --output-on-failure
```

Raw observations and reports are in `build/artifacts/owned/vb_integer/`,
`build/artifacts/owned/vb_muldiv/`, `build/artifacts/owned/vb_exceptions/`,
`build/artifacts/owned/vb_address_traps/`, `build/artifacts/owned/vb_memory_ops/`,
`build/artifacts/owned/vb_bitstrings/` and `build/artifacts/owned/vb_immediates/`.
They remain local while CI/CD is disabled. Source/image SHA256 values are recorded
in `tests/roms-src/rom_manifest.json`. The current `VVBS0003` states store explicit big-endian
scalars for the full CPU, frame/step counters, completion, ROM identity, WRAM
and pending bit-string buffers plus controller state. `VVBS0002` loads with neutral
controller state. Legacy `VVBS0001` states
are rejected; recreate them with the updated core.
Size, version, ROM identity and basic field checks precede any mutation.

## Remaining platform hardware and features

Bit-string searches, FPU/remaining NVC instructions, physical port/peripheral mapping, complete
system registers/PIR/TKCW, other exceptions/external interrupts, caches,
pipeline/wait-state timing, VIP drawing/display/scanner timing, VSU audio,
automatic controller polling/interrupts/timing, link communication, timers and save media remain.
Valid but unimplemented instructions and unmapped transactions stop execution
with an explicit serialized `unsupported` flag. Reserved major opcodes `$1B`,
`$32` and `$36` generate the implemented illegal-instruction exception; remaining
encoding validity and physical bus faults are not modeled. Register
execution and bus storage are functional; timing is not modeled. CAXI is
serialized within one bus callback in the single-CPU profile; external LOCK
signals and future DMA/multiple-owner arbitration remain pending. Arithmetic
bit-string overlap visibility, buffer bus order, invalid working-register fields
and interrupt abort/resume remain unverified; the measured ROM uses separate
source/destination banks with canonical aligned addresses and offsets.

Each frontend frame is a bounded 100,000-step scheduling quantum. The counter is
CPU step attempts, not verified hardware cycles; 20 MHz/50 Hz metadata is nominal.
Video is a blank 384×224 buffer, audio is empty and input reaches the software-clocked controller. Stereo
presentation, rendered UI usage, Windows/macOS builds, real hardware and netplay
for this core remain unverified or unimplemented. Other plugins retain their
existing networking/controller behavior. The external four-platform suite
matrix does not include VB; its owned original ROM and component gates run
locally with CTest while CI/CD is disabled.

### Pending bit-string search evidence

The English architecture manual's SCH0BS remarks refer to searching for a one
despite the zero-search definition; its no-match cursor wording also needs
clarification before an independent register oracle can be accepted. The
current core stops all four search forms diagnostically. Final cursor/length
semantics need stronger primary clarification or captured physical-hardware
registers; no search completion percentage is claimed.

## Native software controller input (2026-10-11)

Virtual Boy **0.2.0** consumes ordinary frontend input through native
software-clocked SCR/SDLR/SDHR transactions. The layout exposes both D-pads,
A/B, L/R, Start and Select; default keyboard/gamepad routes and remap persistence
are verified. The right D-pad uses W/A/S/D or a thresholded right stick. See the
[original cartridge and domain](../../tests/roms-src/vb/controller_serial/README.md), grounded in the
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
and CLI/SEI ID manipulation. The [original instruction cartridge](../../tests/roms-src/vb/nintendo_registers/README.md)
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
