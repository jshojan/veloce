# Original Nintendo DS Thumb stack/block corpus

This MIT-licensed original cartridge executes Thumb PUSH, POP, STMIA and LDMIA
on both DS CPUs. Its standalone Python builder encodes its own drivers and
static instruction thunks. It uses no SDK, firmware, BIOS, Nintendo logo,
commercial ROM or third-party emulator implementation. The
[ARM architecture manual](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a)
defines these instructions (A7.1.27, A7.1.49–50 and A7.1.57).

## Finite domain

The full domain contains **3,720,960 twenty-five-word snapshots**: 2,480,640
ARM9 and 1,240,320 ARM7. All 3,720,960 snapshots matched, with zero mismatches
and no unscored bits.

- Every defined nonempty PUSH/POP register list, including LR-only PUSH and
  PC-only POP; all eight low registers are independently selectable.
- Every STMIA/LDMIA low-register list and all eight encoded base registers.
  LDM base-in-list keeps the loaded value. STM base-in-list is graded only
  when the base is the lowest listed register, and stores the original base.
- Sixteen full-width seed patterns. Source registers and memory words use
  distinct ordinal XOR tags, so swapped register order is observable. These
  correlated seeds do not exhaust every independent 32-bit operand product.
- All sixteen NZCV inputs on ARM7; all thirty-two NZCV/Q inputs on ARM9.
- Each POP-PC list has three targets: an even word-aligned address, an odd
  word-position Thumb address and an odd halfword-position Thumb address.
  ARM9's even target executes ARM code; ARM7's even target executes Thumb code.
  Each target reads its actual PC before returning to the driver.

There are 4,845 groups per CPU: 511 PUSH, 1,023 POP including target variants,
1,271 STMIA and 2,040 LDMIA. Transfer addresses are aligned and mapped; empty
lists, STM's non-lowest base alias and ARM9 PC values ending in binary `10`
are outside this hardware grade. Static thunks avoid runtime code patching.
Both full drivers occupy disjoint main-RAM segments. Reduced ARM7 code fits in
private WRAM. Shared scratch/capture/memory regions lie outside both drivers;
ARM9 signals ARM7 only after finishing its output. This uses the supplied marked
`VDSHB001` profile, not physical boot/cache/CP15 conditions.

## Register capture and host oracle

The guest seeds registers, flags and eleven memory words, enters the static
Thumb instruction, then returns through a high register to ARM code. It saves
all low registers, SP/LR and the target-PC observation before printing alters
working registers. CPSR is captured immediately after returning to ARM; its
T bit is therefore clear in the snapshot. Executing the selected target and
capturing its PC distinguishes POP interworking; component tests check T
explicitly immediately after POP.

| Words | Captured values |
| --- | --- |
| 0–7 | R0–R7 after the transfer |
| 8–9 | SP and LR after the transfer |
| 10 | CPSR after returning to ARM |
| 11 | Target's actual PC read for POP-PC, or unchanged thunk-entry address |
| 12–13 | Initial seed and encoded instruction |
| 14–24 | Eleven observed memory words: lower guard, nine possible transfer slots and upper guard |

`run.py` independently selects registers in numeric order, constructs expected
register/memory changes and writeback, and derives target PC from the archived
code's mapped location. It checks every bit of all twenty-five words. The ROM
contains inputs and captures, with no expected answers or pass verdict.
Word STR to emulator-only `$04FF0008` serializes observations; byte STR to
`$04FF0000` supplies framing and `$04FF0004` marks CPU completion. The frontend
owns the register file identity/trailer. These addresses are transport hooks,
not DS peripherals, and establish no physical bus timing.

## Controls and component evidence

The reduced original ROM passes **19,200/19,200** observations. Executed
missing-store and missing-load mutants fail exactly 7,680 and 11,520 cases,
respectively, with all unaffected groups passing. Each of twenty-five
single-word mutations produces one FAIL. Seven damaged files produce ERROR:
missing/malformed row, reordered group, false completion, missing or
unterminated trailer, and trailing data. CPU/group order, counts, identity,
record syntax and final completion are strict.

`nds_machine_test` separately covers **3,327,744 defined transfer cases** and
604,416 unpredictable-list diagnostics over every encoded list/base, sixteen
operand seeds and all NZCV/ARM9 Q. Its direct byte/word references check every
register and all memory slots/guards. Another 24 unaligned model checks verify
word alignment without rotation and low base bits retained in writeback; these
are outside the aligned ROM grade. Two undefined ARM9 POP targets are rejected.
Eight injected transaction failures check ascending address attempts, staged
loads, no committed register/writeback changes and the completed store prefix.
These diagnostic stops do not implement physical Data Abort behavior; earlier
writes and bus read effects are not rolled back.

Actual shared-plugin PUSH/POP-PC loops run both CPUs on distinct stacks and
restore/replay identical whole CPU/RAM states. Transfer support required no ABI/state change. Later keypad support saves
`VNDS0002` and accepts legacy `VNDS0001` states with released keys.

```bash
python3 tests/roms-src/build.py nds/multiple_registers --original-only --check
cmake --build build --target veloce nds_plugin nds_machine_test -j
ctest --test-dir build -R '^nds_(machine_test|multiple_registers)$' --output-on-failure
```

Raw register files and JSON reports remain in `build/artifacts/owned/nds_multiple/`
and are uploaded by owned-hardware CI. The manifest records generator/image
SHA256. The full image SHA256 is
`a463f10277f1b83fd1ac7133e41369b21382d76b2c7c82f21e7fc50a6f23fb2b`.

Undefined/unpredictable hardware behavior, physical access time order,
abort/alignment handling, cache/TCM/MPU, ARM block/doubleword transfers,
firmware/retail boot, graphics/audio/input/peripherals, DS netplay and DSi enhanced
mode remain unverified or unimplemented. Finite instruction coverage establishes
no whole-platform completion percentage.
