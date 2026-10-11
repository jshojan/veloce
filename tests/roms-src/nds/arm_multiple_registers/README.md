# Original DS ARM block-transfer register/RAM corpus

The MIT generator builds an original marked native `.nds` image with separate
ARM9/ARM7 programs. It contains no SDK, firmware, Nintendo logo, commercial
code, expected guest answers or pass verdict. ARM9 emits first, then releases
ARM7 through a RAM mailbox; completion requires both CPUs to finish.

## Implemented data path and limits

Ordinary ARM LDM/STM (S=0) implement increment/decrement before/after (IA, IB,
DA, DB), all nonempty lists, write-back, defined base aliases, unchanged flags
and PC loads with ARM9/ARM11 interworking. Low register numbers map to low
addresses, regardless of addressing mode. Address arithmetic wraps modulo 2^32.
The supplied little-endian U=0 profile ignores low address bits for memory
transactions while retaining them in base write-back. Physical alignment
checking and timing are pending. See
[ARM DDI0100I A4.1.20/.97 and A5.4](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a).

Empty lists, PC bases, LDM write-back with base in list, and STM write-back
with a listed base above the lowest listed register stop diagnostically.
Unimplemented S=1 User-bank transfers and SPSR exception returns also stop;
banked registers, privilege/modes and exceptions remain required unfinished work.
ARM9/ARM11 PC targets with bits[1:0]=10 stop without destination commit; ARM7
ignores the low two target bits. STM with PC stores the existing supplied
pipeline profile value (instruction+12 on ARM7, +8 on ARM9/ARM11). This value is
implementation defined and has not been measured on hardware; its comparisons
establish model consistency, not physical correctness.

The scheduler executes a whole transfer within one CPU step. Bus calls occur
in increasing address order. Rejected loads commit no destination/base values;
earlier bus reads may already have effects. Rejected stores retain their
completed memory prefix, with registers/base unchanged. These diagnostic
choices are checked; physical data-abort semantics, IRQ boundaries, arbitration,
exclusive monitors, MMIO side effects and atomicity are not established.

## Original execution domain

The full original cartridge grades **2,881,632 snapshots**, each containing
**35 actual words** (**100,857,120 words**), zero mismatches across both DS CPUs:

| Domain | Snapshots |
|---|---:|
| Every legal nonempty 16-bit list in all modes and write-back settings, base R3, AL | 1,605,600 |
| Fourteen boundary lists, all fifteen non-PC bases, eight full-width seeds, all flags, all modes/write-back | 1,144,320 |
| Boundary lists/base R3, conditions 0–13, all flags, all modes/write-back; AL covered above | 131,712 |

List sweeps use one incoming flag assignment derived from the list (16 NZCV
settings on ARM7, 32 NZCV/Q on ARM9), not every list×flag combination. Their
source seed repeats the 16-bit list in both halves of a word. Boundary groups
use all flags and explicit full-width seeds. Invalid write-back/base cases are
omitted from the guest data path and checked separately by the decoder gate;
zero lists and unpredictable hardware effects are not awarded PASS credit.

For each case the driver initializes distinct register/memory patterns and
computes a base placing the transfer at `02001880`. A loaded PC targets an
aligned ARM capture block. The driver writes the tested instruction into one
fixed slot and loads stimulus registers using single-register instructions.
Immediately after transfer, single-register PC-relative STR captures R0–R14;
MRS captures CPSR before output changes flags. Single-register LDR restores the
driver. Capturing the result does not depend on the block instruction under test.

Each snapshot consists of one `REG r=` line (actual list, R0–R14 and CPSR) and
one `MEM m=` line (preceding guard, sixteen RAM words and following guard).
Both fit the test sink's 255-byte limit. The first oversized single-line
capture was correctly rejected as truncated; this protocol preserves every word.
The host independently models lists, modular address/write-back arithmetic,
conditions, initial stimuli and complete register/RAM output. It requires both
ordered lines, exact identities/counts and the strict frontend trailer.

The cartridge uses one transfer region and fixed instruction/capture sites;
all word values, all list/base/condition/seed combinations, arbitrary PC target
streams, physical cache coherence and complete memory/device coverage remain
outside this finite domain. PC loads into the capture block complement the
component check of architectural PC/T values; guest capture omits R15 itself.

## Decoder and negative controls

`arm_multiple_decode_test` performs **50,385,536** checks. Its sweep covers
all 65,536 lists (including diagnostic zero), all sixteen base fields, four
address modes, write-back and load/store, on ARM7/ARM9/ARM11. Each assignment
has one derived flag value and a low address lane derived from the list.
Separate checks combine all flags with seven base/wrap values, six lists,
four PC targets and all modes. User/exception-return diagnostic rejection,
failed ordinary conditions and every transaction position in a fifteen-word
transfer are also checked. Complete register arrays, CPSR, stored values,
transaction counts and failed-store prefixes are compared independently.

The original baseline emits zero observations before implementation. The corrected
**1,276,424-snapshot** reduced cartridge passes; an executed STM-IB-to-IA mutant
fails **153,986 snapshots**, with every unrelated group unchanged. Single
register/RAM bit corruptions fail exactly one row each. Missing/malformed register
or memory lines, false completion, missing/unterminated trailers and extra data
report ERROR (eight damaged-stream controls). The committed harness executes
all these controls by default. `--controls-only` and `--full-only` allow the
verified controls and full corpus to run separately without repeated grading.

DSi/3DS share the interpreter; the original runs both DS CPUs, with ARM11
checked separately. No state-format change is needed: existing register, CPSR
and RAM state fields preserve results. Captures/JSON remain local in
`build/artifacts/owned/nds_arm_multiple/`, with generator/image hashes in the
manifest. CPU/mode/exception/device milestones remain unfinished, so delivery
completion percentages do not rise.

```sh
python3 tests/roms-src/build.py nds/arm_multiple_registers --original-only --check
ctest --test-dir build -R '^(nds_arm_multiple_registers|arm_multiple_decode_test)$' --output-on-failure
```
