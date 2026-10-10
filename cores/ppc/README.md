# Experimental GameCube and Wii PowerPC cores

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| GameCube | **25%** | 5/20 |
| Wii | **25%** | 5/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

GameCube delivered milestones: Native digital controller/keypad registers consume frontend input, Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Remaining CPU, firmware/retail boot, polling/timing/analog/multiplayer, graphics/audio and netplay remain.

Wii delivered milestones: Native digital controller/keypad registers consume frontend input, Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Remaining CPU, MEM2/IOS/retail boot, GameCube polling, Wii Remote, graphics/audio and netplay remain.
<!-- completion:end -->

`gc_plugin` and `wii_plugin` are ABI v2 plugins sharing a supplied PowerPC
integer execution profile. They execute repository-owned programs; retail
GameCube/Wii software cannot boot. Full platform completion is unknown.

## Implemented subset

- ADD/ADDC/ADDE and SUBF/SUBFC/SUBFE, all OE/Rc forms, wrapping results,
  XER carry/overflow/sticky summary overflow and CR0 recording.
- AND/ANDC/OR/ORC/XOR/NAND/NOR/EQV with both Rc forms, RA destinations,
  alias-safe source reads, ordinary GPR0 and unchanged XER. ORI/ORIS,
  XORI/XORIS, ANDI./ANDIS. with unsigned immediates and CR0 recording.
- RLWIMI/RLWINM/RLWNM with all immediate shifts, low-five-bit register
  counts, inclusive/wrapped masks, old-destination insertion and both Rc forms.
- SLW/SRW, SRAW/SRAWI with six-bit register/five-bit immediate counts,
  sign fill and arithmetic-shift CA updates; EXTSB/EXTSH and CNTLZW with
  reserved-field validation. All accept both Rc forms and ordinary GPR0.
- NEG, ADDZE/ADDME and SUBFZE/SUBFME with all OE/Rc forms, carry-in/out,
  overflow/sticky SO and reserved-field checks. ADDIC/ADDIC./SUBFIC use signed
  immediates and ordinary GPR0, update CA and preserve OV/SO.
- MULHW/MULHWU, MULLW and MULLI; DIVW/DIVWU, all defined OE/Rc
  forms, ordinary GPR0 and alias-safe operands. Invalid division chooses zero
  in this profile; undefined quotient/CR0 ordering bits are excluded from
  hardware result scoring.
- ADDI/ADDIS. LWZ/LHZ/LBZ/LHA and STW/STH/STB with signed displacement,
  indexed and base-update forms; LWBRX/LHBRX/STWBRX/STHBRX byte reversal.
  Old source/index/base values supply transfers before base updates. Invalid
  update fields, reserved indexed Rc and invalid accesses diagnose atomically.
  RA=0 supplies zero for ordinary addressing; indexed RB=0 reads GPR0.
- B/BL and BC, signed absolute/relative displacement; BCLR/BCCTR including
  linked forms, CTR decrement/testing, CR bit conditions and aligned targets.
  LK updates LR even on untaken paths; old LR supplies BCLR targets.
- CMP/CMPL and CMPI/CMPLI (32-bit L=0), signed/unsigned ordering, any CR
  field and copied XER.SO; all GPR fields, including ordinary GPR0 reads.
- CRAND/CRANDC/CREQV/CRNAND/CRNOR/CROR/CRORC/CRXOR, aliased source/destination
  bits, MCRF field copies and MCRXR field capture/flag clearing.
- XER/LR/CTR MFSpr/MTSpr,
  MFCR and MTCRF field masks.
- Reset, ROM CRC/identity, complete register/RAM states and exact replay.
  Unknown instructions, invalid accesses and unsupported SPRs stop with an
  internal diagnostic. Hardware exceptions are not modeled.

Each original corpus matches **49,152/49,152 six-word snapshots**: six
arithmetic operations × sixteen operand patterns squared × all eight incoming
SO/OV/CA combinations × both OE × both Rc. This is a finite boundary domain;
32-bit operand pairs are not exhaustive. Host arithmetic grades every captured
bit of result, CR, XER, both sources and an untouched sentinel. Programs contain
no expected answers or pass verdicts. The component gate matches 2,752,512 arithmetic/register snapshots across both
profiles and passes with undefined-behavior sanitization. Checks cover every arithmetic
GPR field encoding and destination/source aliases, loader/state validation,
memory, branches and both dynamically loaded plugins.

The [GameCube](../../tests/roms-src/gc/branch_registers/README.md) and
[Wii](../../tests/roms-src/wii/branch_registers/README.md) branch ROMs each match
74,752 seven-word snapshots: BC/BCLR canonical BO and every BI, four CR seeds,
eight CTR patterns and both LK states; BCCTR uses only defined options and
all target low bits. The component gate additionally matches 4,227,072 branch
snapshots across both profiles, covers every BD field and AA/LK states, and
checks exact replay of a 100,001-iteration original loop. Invalid XL forms and
BCCTR decrement options diagnose without changing architectural registers.
Noncanonical BO z bits are ignored in the supplied profile, separately tested
but excluded from the ROM hardware score. Original XL fields require all five
reserved bits zero; newer BH encodings are unsupported. Branch prediction,
physical timing and specific Gekko/Broadway quirks remain unverified.

The [GameCube](../../tests/roms-src/gc/condition_registers/README.md) and
[Wii](../../tests/roms-src/wii/condition_registers/README.md) condition corpora
each match 42,108,928 eight-word snapshots. All 16-bit compare immediates are
covered on sixteen operand patterns, all CR fields and both SO states. CR
logical operations cover every destination/source field encoding on four CR
patterns and all XER flag states; MCRF/MCRXR capture preserved fields and
cleared flags, including every seven-bit XER byte count. Component checks
match 101,941,248 additional comparison/condition snapshots across both
profiles, force every defined logical truth input and verify GPR0, field
aliases, reserved-bit diagnostics and exact state replay through both plugins.
Runtime-generated instructions use the supplied flat mapping; physical cache
coherency and timing remain unverified. CMP L=1 and invalid forms diagnose;
physical program exceptions are still pending.

The [GameCube](../../tests/roms-src/gc/logical_registers/README.md) and
[Wii](../../tests/roms-src/wii/logical_registers/README.md) integer logical ROMs
each match 6,324,224 nine-word snapshots: 32,768 register operations on sixteen
operand patterns/all XER flags/both Rc states, and 6,291,456 immediate cases
covering all immediate values on eight operands/both SO states. Independent
truth-table grading checks result, CR, XER, sources, encoding, seeds and sentinel.
The component gate matches 21,856,256 additional integer logical snapshots
across both profiles, including every register field encoding/alias, all bit
truth inputs, immediate fields and exact state replay through both plugins.
X-form logical operations have no OE bit; unsupported high-XO variants diagnose
without mutating architectural registers. Full operand words and physical
console behavior are not exhaustive; [domain details](../../tests/roms-src/gc/logical_registers/README.md).

The [GameCube](../../tests/roms-src/gc/rotate_registers/README.md) and
[Wii](../../tests/roms-src/wii/rotate_registers/README.md) rotate/mask ROMs each
match 9,437,184 ten-word snapshots: every shift and mask encoding on eight
source patterns, four insert destinations, four RLWNM upper-count patterns,
both SO and both Rc states. Host grading independently checks final result,
CR/XER, sources, initial destination, instruction/seeds and a sentinel. The
component gate matches 113,258,496 additional rotate snapshots across both
profiles, including all XER flag combinations, every GPR encoding/alias on
boundary masks, walking source bits at every shift/mask and all XER byte counts
at a wrapped mask. Known guest captures and exact replay through both plugins
pass, including state restoration before an RA=RB rotate. Full 32-bit operand
pairs, CR/XER words and physical console behavior are not exhaustive;
[declared domain](../../tests/roms-src/gc/rotate_registers/README.md).

The [GameCube](../../tests/roms-src/gc/shift_registers/README.md) and
[Wii](../../tests/roms-src/wii/shift_registers/README.md) shift/unary ROMs each
match 4,416,528 nine-word snapshots: 196,608 register-shift, 8,192 SRAWI,
16,384 EXTSB, 4,194,304 EXTSH and 1,040 CNTLZW observations. All extension
input bits and shift counts are covered within declared upper/source patterns,
all XER flags and both Rc states. Independent arithmetic/bit-length grading
checks result, CR/XER, sources, encoding/seeds and sentinel. Component checks
match 37,042,208 additional snapshots across both profiles, including every
GPR/count field encoding/alias at boundaries, walking source bits, all byte
counts and exact state replay through both plugins before CA changes. Reserved
unary RB fields diagnose without register mutation. Full 32-bit operand pairs,
CR/XER words and physical console behavior remain unverified;
[finite domain details](../../tests/roms-src/gc/shift_registers/README.md).

The [GameCube](../../tests/roms-src/gc/carry_registers/README.md) and
[Wii](../../tests/roms-src/wii/carry_registers/README.md) carry arithmetic ROMs
each match 25,168,384 nine-word snapshots: 2,560 NEG/extended unary operations
and 25,165,824 carry immediate observations. Every immediate is covered on
sixteen source patterns/all XER flags; register forms include all OE/Rc states.
Independent mathematical/unsigned-carry grading checks result, CR/XER, sources,
encoding/seeds and sentinels. Component checks match 56,083,456 additional
snapshots across both profiles, including all GPR encodings/aliases, walking
sources, all byte counts, reserved-field diagnostics and exact state replay
through both plugins before NEGO. sets OV/SO. Full 32-bit operand and CR/XER
words, physical exceptions and console behavior remain unverified;
[declared domains](../../tests/roms-src/gc/carry_registers/README.md).

The [GameCube](../../tests/roms-src/gc/multiply_registers/README.md) and
[Wii](../../tests/roms-src/wii/multiply_registers/README.md) multiply/divide ROMs
each match 8,420,320 defined results plus 1,056 undefined division rows checked
for defined flags and preserved fields. MULLI covers every 16-bit immediate
on sixteen sources/all incoming XER flags; register forms use sixteen operand
patterns, every applicable OE/Rc and all incoming flags. The independent host
masks only undefined quotient and recorded CR0.LT/GT/EQ; CR0.SO and XER remain
graded. The supplied profile chooses quotient zero on divide-zero/MIN divided
by -1, without claiming those bits match physical hardware. Component checks
match 26,443,776 snapshots across both profiles, including 280,640 deterministic
undefined-result fixtures; register fields, aliases, independent limb products,
restoring division, reserved variants, guest captures and state replay pass.
The full component gate also passes undefined-behavior sanitization. See the
[NXP instruction set manual](https://www.nxp.com.cn/docs/en/reference-manual/MPC82XINSET.pdf)
for DIVW/DIVWU undefined-result rules. Console timing remains unverified.

The [GameCube](../../tests/roms-src/gc/memory_registers/README.md) and
[Wii](../../tests/roms-src/wii/memory_registers/README.md) memory ROMs each match
12,586,368 ten-word observations. Fourteen displacement forms cover every
aligned 16-bit displacement on eight data patterns/three RAM aliases; eighteen
indexed forms use eight index patterns. Independent byte-array grading checks
result, updated/preserved base, index, CR/XER, exact target bytes, surrounding
guards, original instruction and source. Incoming CR/XER seeds are selected
per group, not crossed with every input. Component coverage adds 27,560,000
snapshots across both profiles: 26,356,500 valid transfers and 1,203,500
invalid-form/access diagnostics. Complete register arrays, every field encoding,
source/base/index aliases, reserved Rc, neighboring memory bytes, a known
mixed-transfer guest and exact machine/plugin replay pass, including with
undefined-behavior sanitization. Physical alignment/bus exceptions and timing
remain unmodeled. See the linked domain tables for the finite scope.

Instruction references: [NXP MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf),
[IBM addc](https://www.ibm.com/docs/en/aix/7.1.0?topic=set-addc-add-carrying-instruction),
[IBM subfe](https://www.ibm.com/docs/en/aix/7.1.0?topic=set-subfe-sfe-subtract-from-extended-instruction),
[IBM mtcrf](https://www.ibm.com/docs/en/aix/7.2.0?topic=set-mtcrf-move-condition-register-fields-instruction).
Branch references: [NXP MPCxxx instruction set](https://www.nxp.com/docs/en/reference-manual/MPC82XINSET.pdf),
[NXP 32-bit programming-environment errata](https://www.nxp.com/docs/en/reference-manual/MPCFPE32BAD.pdf),
[IBM BCLR](https://www.ibm.com/docs/el/ssw_aix_72/assembler/idalangref_branch_conditional_link_register.html).

These instruction-level references do not establish Gekko/Broadway hardware
quirks, timing or console compatibility.

## Original image and state formats

Only `.vgc` (`VGCP0001`) and `.vwi` (`VWIP0001`) marked images load. The 32-byte
big endian header contains magic at 0, load address at 8, entry at 12, payload
length at 16 and twelve zero reserved bytes at 20. The word-aligned payload
starts at 32, fits supplied RAM and contains the aligned entry instruction.
The canonical load range is `$80000000–$817FFFFF`. These are original test
formats, not disc, DOL, ELF, firmware or commercial ROM formats.

The supplied 24 MiB RAM accepts byte/halfword/word accesses through physical
`$00000000`, cached `$80000000` and uncached `$C0000000` aliases. This flat
mapping does not emulate address translation, cache behavior or hardware bus
faults. The Wii profile has no MEM2. Reset zeroes RAM/registers and loads the
payload directly; this is not physical console startup.

The synthetic test transport accepts byte characters at `$FFF00000`, word
hexadecimal observations at `$FFF00008` and completion at `$FFF00004` through
`ppcdebug`. These addresses are test conventions, not Nintendo MMIO. The
application writes `VELOCE_TEST_OUT`; completion remains ungraded (`-1`).

`VPPC0002` states contain profile, full image identity/CRC, frame/completion
counters, all GPRs, PC/CR/XER/LR/CTR, instruction count/diagnostic, all RAM and 156 bytes of serial communication RAM,
input/connection masks, control and error latches. Old `VPPC0001` states load
with the serial device reset to its neutral default. New invalid peripheral
fields are validated before any CPU/RAM mutation.
Malformed states and states from another ROM/profile are rejected before any
mutation. Output files/sink callbacks are external to state; replaying a state
can emit observations again.

## Pending hardware

Most CPU instructions, exceptions/interrupts, privileged
state, MMU/caches, floating point and paired-single operations remain absent.
So do IPL/IOS and firmware startup, disc loading, Flipper/Hollywood graphics,
DSP/audio, EXI, most SI behavior, Wii Bluetooth/Wiimote accessories, memory cards,
network hardware and netplay. Port-one digital input drives the supplied SI direct-read profile; video is blank and audio is
empty. The 100,000-instruction scheduling quantum and 60 Hz frontend metadata
are not measured physical instruction or video timing; the ABI cycle counter
reports attempted instructions and CPU frequency is unspecified.

## Build and verify

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_(integer|branch|condition|logical|rotate|shift|carry|multiply|memory)_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

## Shared instruction module

`src/cpu.{hpp,cpp}` now exposes `ppc::execute(State&, Memory&)`, separating
instruction behavior from each console's bus and completion/scheduling policy.
The original GC/Wii `Machine` retains its maps and loader; its versioned states now also preserve SI.
The separate [Wii U plugin](../wiiu/README.md) supplies three CPU contexts and
a 32 MiB MEM1/2 GiB MEM2 physical profile. The extraction changes bus routing
and caller-owned completion only; GC/Wii component regression passes. Espresso
privileged state, MMU/cache/coherency, timing and remaining instructions are
pending. GC/Wii coverage counts are not evidence of all Wii U hardware.

## GameCube controller communication RAM

Version 0.10.0 adds settled direct SI transactions at `CC006400` (GameCube) or
`CD006400` (Wii), aligned 32-bit register/buffer access, SICOMCSR lengths/channel
selection/completion/masks, SISR per-channel errors and W1C acknowledgement.
Only standard identify/reset (`00`/`FF`) and mode-3 direct digital reads
(`40 03 00/01/02`) are supplied; unsupported commands/modes return a profile
no-response error. Requested lengths use zero for 128 bytes; replies preserve
bytes beyond the received payload and record short/long errors. TCINT indicates
completion independently of its mask; CPU interrupt delivery is pending.
Polling/output/input-buffer registers reject rather than simulate polling.

[GameCube](../../tests/roms-src/gc/controller_serial/README.md) and
[Wii](../../tests/roms-src/wii/controller_serial/README.md) original ROMs each
match 4,096/4,096 sixteen-word snapshots. The component gate adds 262,144
port/input/connection snapshots and 196,608 length snapshots. Exact state
replay, legacy state loading and malformed state atomicity pass for both cores.

The frontend routes one digital pad. Its twelve controls appear in the input
layout/configuration UI: A=Z, B=X, X=A, Y=S, Z=C, L=Q, R=W, Start=Enter,
D-pad=arrows. Custom bindings use the existing saved configuration/hotplug
system; the virtual-device integration gate verifies defaults and persistence.
Native analog axes, polling, origin calibration, physical rumble, timing,
accessories, four-port frontend routing and Wii Remotes remain pending. Wii
uses this legacy GameCube-controller path; this is not Wii Remote emulation.

Register and length definitions are grounded in Nintendo's
[external-interface patent](https://patents.google.com/patent/US6609977B1/en).
The Wii/GC effective addresses and zero-for-128 encoding are independently
confirmed by [libogc's SI implementation](https://github.com/devkitPro/libogc/blob/master/libogc/si.c).
Controller wire layout follows original research in
[YAGCD Joybus](https://www.gc-forever.com/yagcd/chap9.html); the origin marker
and mode-3 arrangement are cross-checked against
[Dolphin's authored controller implementation](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/SI/SI_DeviceGCController.cpp).
No upstream implementation was copied. The idle ID/status and unsupported-mode
responses are supplied conventions; physical hardware validation remains pending.
