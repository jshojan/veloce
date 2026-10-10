# Original Nintendo DS Thumb long-call corpus

This MIT-licensed original cartridge executes adjacent Thumb BL pairs on ARM7
and ARM9, and Thumb BLX immediate pairs on ARM9. It contains its own reset-entry
drivers, instruction encoders and target trampolines; it uses no SDK, firmware,
BIOS, Nintendo logo, commercial ROM or third-party emulator implementation.
Instruction semantics follow the
[ARM architecture manual](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a)
(Thumb BL/BLX, A7.1.10). This is the core's marked `VDSHB001` emulator test
profile, not physical firmware boot.

## Measured domain

The actual frontend/plugin run matched **9,437,184/9,437,184 eight-word
snapshots**, zero mismatches and no unscored bits. ARM9 contributes 7,340,032;
ARM7 contributes 2,097,152.

- Every 11-bit signed high displacement field, paired with a fixed valid low
  field; every valid unsigned low field, paired with a fixed high field.
- Both source instruction halfword positions within a word. Thumb BL targets
  exercise both halfword positions; ARM9 BLX targets are word aligned.
- Sixteen initial LR seeds, every incoming NZCV combination, and both ARM9 Q
  states. ARM7 reserved Q is never supplied or graded as hardware behavior.

These are always valid adjacent pairs. A high-field group fixes BL low to zero
or BLX low to two. A low-field group fixes BL high to zero or BLX high to one
and varies its source address to keep the destination mapped. No pair branches
into its own suffix or crosses the 32-bit address-space boundary. This ROM does
**not** cover every high × low combination together.

Separately, `nds_machine_test` grades 20,971,515 defined combined encoding pairs
across both source positions and supported architectures. Five combinations
that target their own second half are architecturally unpredictable and excluded
from that defined grade. Another 6,720 boundary/flag attempts include diagnostic
checks. Odd ARM9 BLX suffix encodings are undefined; ARM7 BLX is unavailable.
The core's stops for those encodings and self-suffix targets are diagnostic
choices, not modeled hardware exceptions. Other invalid instruction-pair
streams are outside this grade.

## Guest capture and independent host grade

The guest installs the two halfwords with STRH, seeds LR and CPSR, then enters
the prefix in Thumb state. Mapped target code captures LR and an actual PC read,
returns to the ARM driver, and the driver captures CPSR. Each `REG r=` row holds
eight observed 32-bit words in this order:

| Word | Observation |
| --- | --- |
| 0 | Link register captured at the target |
| 1 | CPSR after returning to the ARM driver |
| 2 | Target instruction's PC read |
| 3 | Initial LR seed |
| 4 | Encoded high field |
| 5 | Encoded low field |
| 6 | Source pair address |
| 7 | Memory word written with LR by an ARM BLX target, or unchanged sentinel for BL |

The ROM supplies inputs and captured values, without expected answers or a pass
verdict. `run.py` computes signed displacement, target alignment, tagged return
address, PC pipeline offset, preserved NZCV/Q and expected memory independently
with Python integer arithmetic. Every bit in all eight words is checked.

A sparse 4 MiB main segment supplies static target trampolines at mapped pages;
the ARM9 driver occupies a separate region and ARM7 runs from private WRAM.
Each case backs up its two source halfwords and restores them before printing.
The loaded RAM is writable; the archived cartridge stays immutable. This dynamic
code patching assumes the supplied **cache-free profile**. It is not evidence of
physical instruction-cache coherence, CPU synchronization or timing.

Word STR to emulator-only `$04FF0008` serializes each captured value as eight
lowercase hex digits; byte STR to `$04FF0000` supplies text framing. ARM9 finishes
its output before signaling ARM7 through shared RAM. Both finish through
`$04FF0004`; the frontend supplies the file identity and unknown-status trailer.
These addresses are transport hooks, not DS peripherals.

## Grader controls and replay

The reduced original cartridge passes all 9,472 observations. Two executed
mutants check failure detection: a flipped high sign bit fails all 9,472 cases;
a wrong captured ARM-target link fails exactly the 3,328 BLX cases. Eight
single-word mutations each produce one FAIL. Seven damaged streams produce
ERROR: missing/malformed row, reordered group, false completion, missing or
unterminated trailer, and trailing data. Counts, CPU/group order, file identity,
record width and final completion are checked strictly.

Component tests save after a prefix and resume the suffix. Actual shared-plugin
frames also end after a prefix on each CPU; restore/replay produces identical
whole states for BL and ARM9 BLX. Existing serialized LR, PC and CPSR carry the
intermediate state; later keypad support saves `VNDS0002` states and accepts legacy `VNDS0001`
states with released keys.

```bash
python3 tests/roms-src/build.py nds/long_calls --original-only --check
cmake --build build --target veloce nds_plugin nds_machine_test -j
ctest --test-dir build -R '^nds_(machine_test|long_calls)$' --output-on-failure
```

Raw files and JSON reports remain in `build/artifacts/owned/nds_long_calls/` and. The manifest records generator/image SHA256.
The full archived image SHA256 is
`f968b7b2c19910fb2af4cf76879a2f27f3ecf4baa07f9140d0b81592780e4892`.

Physical-hardware comparison, cache/TCM/MPU, exceptions/interrupts, ARM BLX
immediate, ARM block transfers, firmware/retail boot, graphics/audio,
controllers/peripherals, DS wireless multiplayer and DSi enhanced mode remain unverified or
unimplemented. Finite arithmetic coverage establishes no whole-platform
completion percentage.

Thumb PUSH/POP/STMIA/LDMIA have a separate original
[stack/block corpus](../multiple_registers/README.md), with defined list/base
encodings and diagnostic component checks reported separately.
