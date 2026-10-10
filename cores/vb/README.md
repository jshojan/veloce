# Experimental Virtual Boy core plugin

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
  Other exception producers and external interrupts remain pending.
- MOVEA, MOVHI, ADDI, ORI, ANDI and XORI; sign/zero extension and source aliasing.
- All 16 branch/SETF conditions, relative JR/JAL, register JMP and link r31.
  There are no MIPS-style branch delay slots.
- LD.B/LD.H sign extension, LD.W, ST.B/ST.H/ST.W, architectural alignment
  masking, 64 KiB WRAM storage/aliases and read-only cartridge storage.
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
`$04000004` finishes with unknown status. These debug addresses are not Nintendo
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
These component checks are separate from the original-ROM domain.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce vb_plugin vb_machine_test -j
ctest --test-dir build -R '^vb_' --output-on-failure
```

Raw observations and reports are in `build/artifacts/owned/vb_integer/`,
`build/artifacts/owned/vb_muldiv/` and `build/artifacts/owned/vb_exceptions/`.
They are uploaded by the owned-hardware CI job. Source/image SHA256 values are recorded
in `tests/roms-src/rom_manifest.json`. `VVBS0001` states store explicit big-endian
scalars for the full CPU, frame/step counters, completion, ROM identity and WRAM.
Size, version, ROM identity and basic field checks precede any mutation.

## Remaining platform hardware and features

Bit strings, FPU/custom NVC instructions, port I/O, complete
system registers/PIR/TKCW, other exceptions/external interrupts, address traps, caches,
pipeline/wait-state timing, VIP drawing/display/scanner timing, VSU audio,
controller serial hardware, link communication, timers and save media remain.
Valid but unimplemented instructions and unmapped transactions stop execution
with an explicit serialized `unsupported` flag. Reserved major opcodes `$1B`,
`$32` and `$36` generate the implemented illegal-instruction exception; remaining
encoding validity and physical bus faults are not modeled. Register
execution and bus storage are functional; timing is not modeled.

Each frontend frame is a bounded 100,000-step scheduling quantum. The counter is
CPU step attempts, not verified hardware cycles; 20 MHz/50 Hz metadata is nominal.
Video is a blank 384×224 buffer, audio is empty and input is ignored. Stereo
presentation, rendered UI usage, Windows/macOS builds, real hardware and netplay
for this core remain unverified or unimplemented. Other plugins retain their
existing networking/controller behavior. The external four-platform suite
matrix does not include VB; its owned original ROM and component gates run in CI.
