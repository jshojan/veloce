# Experimental GameCube and Wii PowerPC cores

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
- ADDI/ADDIS; big endian LWZ/LHZ/LBZ and STW/STH/STB with signed
  displacement, RA=0 address semantics and alignment diagnostics.
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

`VPPC0001` states contain profile, full image identity/CRC, frame/completion
counters, all GPRs, PC/CR/XER/LR/CTR, instruction count/diagnostic and all RAM.
Malformed states and states from another ROM/profile are rejected before any
mutation. Output files/sink callbacks are external to state; replaying a state
can emit observations again.

## Pending hardware

Most CPU instructions, exceptions/interrupts, privileged
state, MMU/caches, floating point and paired-single operations remain absent.
So do IPL/IOS and firmware startup, disc loading, Flipper/Hollywood graphics,
DSP/audio, EXI/SI controllers, Wii Bluetooth/Wiimote accessories, memory cards,
network hardware and netplay. Input is ignored, video is blank and audio is
empty. The 100,000-instruction scheduling quantum and 60 Hz frontend metadata
are not measured physical instruction or video timing; the ABI cycle counter
reports attempted instructions and CPU frequency is unspecified.

## Build and verify

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target veloce gc_plugin wii_plugin ppc_machine_test -j4
ctest --test-dir build -R '^(ppc_machine_test|(gc|wii)_(integer|branch|condition|logical|rotate|shift|carry)_registers)$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```
