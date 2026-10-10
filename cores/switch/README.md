# Experimental Nintendo Switch core plugin

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Nintendo Switch | **25%** | 5/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Nintendo Switch delivered milestones: Parse a native cartridge/executable format (boot scored separately), Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Full AArch64 CPU/extensions/exceptions, native RAM/MMU/processes, compressed/hash-checked NSO and other formats, firmware/HOS/retail boot, graphics/audio, native controllers/devices, saves/netplay, physical timing and complete original hardware corpus remain. Current execution is an original uncompressed NSO0 integer process profile; retail playability is unverified.
<!-- completion:end -->

`switch_plugin` builds `switch.so` / `switch.dll` / `switch.dylib`, discovered by
emulator ABI v2 alongside the other cores. It executes original AArch64 code
from an **uncompressed NSO0 executable**. Firmware/native boot, HOS services,
retail games, graphics/audio and native peripherals are unimplemented.
This is a functional integer process profile, with no hardware timing claim.

## Execution and loader

Implemented: 32/64-bit ADD/SUB immediate and shifted-register forms, flag-setting
forms, ADC/ADCS/SBC/SBCS, AND/ORR/EOR/ANDS and inverted operands, LSL/LSR/ASR/ROR
operand shifts, MOVN/MOVZ/MOVK, ADR/ADRP, B/BL/B.cond, CBZ/CBNZ, BR/BLR/RET,
unsigned-offset byte/halfword/word/doubleword LDR/STR, NOP and MRS/MSR NZCV.
W-register results clear the upper half. SP and XZR uses follow the instruction
family. Unsupported/reserved encodings, fetch/alignment and memory faults stop
with a diagnostic; architectural exception entry and system registers remain
pending. Barriers and other hints are not treated as NOPs.

The loader checks the 0x100-byte NSO0 header, segment/file bounds and overlaps,
page-aligned destinations, text entry at offset zero, BSS extent and optional
module-name file bounds. Text is read/execute, RO is read-only, data/BSS and the
explicit stack are writable/non-executable. Rejected loads preserve the active
image/state. This profile uses a **4 MiB process map**, fixed base zero and
64 KiB test stack; full Switch RAM/MMU/process ownership is pending.
**Compression and segment-hash flags are explicitly rejected**. Relocations,
multiple modules, NRO/NSP/XCI loading, executable hash validation, IPC/services,
firmware, GPU/audio, controllers, saves and netplay remain pending.

Instruction semantics are grounded in the [Arm A64 instruction documentation](https://developer.arm.com/documentation/100069/0608/A64-General-Instructions/ADDS--shifted-register-).
The native container layout is checked against the [hactool NSO0 header](https://github.com/SciresM/hactool/blob/master/nso.h).
No firmware, keys, SDK or commercial executable bytes are included.

## Register verification

The [original MIT guest](../../tests/roms-src/switch/arithmetic_registers/README.md)
passes **1,572,864 cases / 7,864,320 actual 64-bit captured words**. It covers all
repeated-byte operand pairs, both widths, both incoming carry values with stated
NZV seeds, and six arithmetic forms. Guest STR instructions publish operands,
result, actual MRS NZCV value and context through emulator-only `vnxdebug`.
There are no expected answers or correctness verdicts in the image. Host grading
uses separate unbounded-integer arithmetic with fixed reference assertions.
The executed result-bit mutant fails all 384 short cases; five field mutations
and seven malformed streams reject. Counts describe this domain, not the full
AArch64 ISA or every pair of 64-bit operands.

The independent component passes **13,877,248 integer cases**:

| Domain | Cases |
|---|---:|
| Six arithmetic forms, repeated-byte pairs, both widths, all 16 incoming NZCV values | 12,582,912 |
| Every 12-bit add/sub immediate, both shifts/widths, four forms, all-ones operand | 65,536 |
| All legal logical shift/rotate amounts, four forms, inversion, both widths and 16 patterns | 49,152 |
| Every 16-bit MOVN/Z/K immediate, every legal halfword and both widths | 1,179,648 |

A bit-serial full-adder and independent bit-selection oracle grade component
arithmetic/shifts. Additional checks cover all 256 B.cond/NZCV combinations,
SP/XZR, unallocated encodings, loader rejection, map permissions and BSS.
This is finite result verification; physical-console validation remains pending.

## State and production integration

`VNXS0001` stores **4,194,648 bytes**: full process RAM, 31 X registers, SP, PC,
NZCV, step/frame counts, completion and terminal diagnostics, pinned to the
loaded image identity. Rejected states preserve the active state; immutable
text/RO must match the image. Pending guest execution replays byte-for-byte.
The production ABI gate also compares actual emitted register streams, complete
states and counters after restore, and checks reset/unload and protection of
executable bytes through the legacy UI write API.

```bash
cmake -S . -B build
cmake --build build --target veloce switch_plugin switch_machine_test switch_plugin_state_test -j
ctest --test-dir build -R '^switch_' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Original reports: `build/artifacts/owned/switch_arithmetic/`. Timing uses bounded
100,000-attempt scheduling quanta, not native display frames. CPU clock metadata
is unmeasured (zero); the debug panel reports unmeasured time without division. The full Nintendo
implementation goal remains unfinished. CI/CD remains disabled.

Verification: all three Switch gates pass; the full build and integer component
UBSAN pass. All 95 original ROM archives reproduce, completion blocks are
synchronized, and changed local documentation links/whitespace pass.
