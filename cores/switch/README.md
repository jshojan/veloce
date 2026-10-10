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
unsigned-offset byte/halfword/word/doubleword LDR/STR, NOP and MRS/MSR NZCV,
MADD/MSUB, signed/unsigned long and high multiply, SDIV/UDIV, CSEL/CSINC/CSINV/CSNEG, variable LSL/LSR/ASR/ROR,
RBIT, REV16/REV32/REV64, CLZ and CLS.
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

Initial integer-profile verification passed all three initial Switch gates and
95 original archives. Later verification is recorded in the instruction-family
sections below.


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

The [new original guest](../../tests/roms-src/switch/muldiv_registers/README.md)
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
the [pinned LLVM encoding reference](../../tests/reference/switch-muldiv/README.md).
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

The [original guest](../../tests/roms-src/switch/select_registers/README.md) passes **524,288 rows / 2,621,440 actual words**:
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
All eight guest forms match [pinned LLVM opcode fields](../../tests/reference/switch-select/README.md); physical
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

The [original guest](../../tests/roms-src/switch/bitops_registers/README.md) passes **1,120,096 rows / 5,600,480 actual words**:
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
reference](../../tests/reference/switch-bitops/README.md); this does not claim physical execution.

All **twelve Switch gates pass** in **24.92 seconds** after a warning-free full
build, including nine existing regressions. Component UBSAN passes with clean
compilation. All **98 original archives** reproduce; actual report identities,
completion renderer, changed local links and whitespace pass. Switch stays
**25% = 5/20 fixed delivery milestones**: full CPU/extensions/exceptions, native
memory/MMU, firmware/HOS/retail boot, GPU/audio/controllers/devices, persistent
saves/netplay, physical timing and the complete native hardware corpus remain
pending. These finite operand domains do not claim complete hardware accuracy.
CI/CD remains disabled. The full Nintendo implementation goal remains active.
