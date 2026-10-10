# Original N64 integer register corpus

This repository-owned ROM executes actual MIPS III instructions through the
loadable N64 core and ordinary headless frontend. It contains no Nintendo boot
code, SDK assets or third-party ROM bytes. Source and binary are MIT licensed.
Its generator needs only Python; committed binary and generator SHA256 values
are recorded in `rom_manifest.json`.

## Declared domain

| Instructions | Inputs | Observations |
|---|---|---|
| ADDU, SUBU | 16 canonical signed word patterns × 16 | 512 |
| DADDU, DSUBU, AND, OR, XOR, NOR, SLT, SLTU | 16 doubleword patterns × 16 | 2,048 |
| SLLV, SRLV, SRAV | 16 canonical word patterns × amounts 0–255 | 12,288 |
| DSLLV, DSRLV, DSRAV | 16 doubleword patterns × amounts 0–255 | 12,288 |
| MULT, MULTU, DIV, DIVU | 16 canonical word patterns × 16 | 1,024 LO/HI pairs |

Total: **28,160 observations**. This includes every low-byte shift amount,
so masking to 5/6 bits is checked across repeated effective amounts. Patterns
include zero, small values, sign boundaries, all ones, alternating bits and
selected irregular doublewords; see `WORD`/`WIDE` in `build.py` for exact values.
Word operands are sign-extended to avoid unspecified noncanonical input cases.
Immediate shifts and other integer opcodes are implemented in the core but are
not scored by this ROM. Wide operand domains are not exhaustively enumerated.

The ROM loads input tables into actual GPRs, executes the instruction, and
captures the result in GPR 21. HI/LO operations use MFLO/MFHI to capture both.
Output routines preserve these registers and print their actual values, not
expected values or a pass verdict. `REG r=` holds one 16-digit doubleword or a
32-digit LO-then-HI pair. Groups and input order are fixed. The host uses Python
unbounded integers, modular word arithmetic, signed interpretation, arithmetic
shift via floor division, and quotient truncation toward zero. Hand references
exercise carry/wrap, sign extension, shifts at 0/31/63/64, signed comparison,
negative products/division, and unsigned multiply high halves. No emulator code
is imported. The architecture reference is the
[NEC VR4300 manual](https://hack64.net/docs/VR43XX.pdf), instruction details in
chapter 16.

All 32 division-by-zero cases and the signed minimum/-1 overflow case are
observed but **not counted as architectural accuracy**. The manual does not
provide defined portable results for these inputs. The core has deterministic
fallback values; this suite does not present those conventions as hardware proof.
Thus **28,127 defined observations** are graded, with 33 unscored observations.

## Entry, channel and limits

The ROM uses the core's explicit `V64HB001` homebrew entry profile: payload at
ROM `$1000` is copied to physical RAM `$400`, with entry `$80000400`. Boot
regions are zero. This bypasses PIF/IPL/CIC, and is not a bootable retail image
or a claim of physical-console execution. See the [core documentation](../../../../cores/n64/README.md)
for loader details and missing hardware.

Guest `SB` to `$BFF00000` writes each character to the emulator-only debug
channel; `SW` to `$BFF00004` finishes without a verdict. These are observation
hooks, not emulated Nintendo registers. The application records actual guest
register values in `VELOCE_TEST_OUT`. No display, screenshot, or stdout PASS
is graded. `LOG complete cases=28160`, `END observations_complete`, and the
unknown-status (-1) sink trailer are mandatory. The parser verifies raw-file
CRC, core/channel, metadata, every group/row/count and absence of trailing data.

This suite verifies selected integer results. Pipeline/caches/bus timing,
privilege modes, TLB, interrupts/FPU, video/audio, peripherals, controller/link
and retail boot remain separate domains. Its matched fraction is not full-core
completion. `cycles` in the trailer is this core's instruction-step counter,
not measured hardware cycles; `frames` is a frontend scheduling quantum.

## Run and retained evidence

```bash
python3 tests/roms-src/build.py n64/integer_registers
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_machine_test
ctest --test-dir build -R '^n64_' --output-on-failure
```

The runner retains `integer_registers.result/.json` in
`build/artifacts/owned/n64_integer/`, with group counts, undefined/graded counts,
mismatch examples, ROM hash, instruction steps and frame count. It also runs
byte-swapped `.v64` and `.n64` variants through actual frontend discovery and
stores `v64.result/.json` and `n64.result/.json`; their raw-file CRCs differ.
They must match the same defined domain, not add new unique hardware coverage.

A separate mutant changes the first ADDU to SUBU. Only that group may fail.
Missing/malformed rows, reordered groups, false-PASS completion, missing or
unterminated trailers and trailing data must be measurement errors. Raw files
and JSON reports for all controls are retained.

Measured 2026-10-09: **28,127/28,127 defined results matched** (100% of this
selected domain), all 28,160 observations present, 33 unscored undefined inputs,
zero mismatches across 20 groups. The big-endian run finished after 66 scheduling
frames and 6,592,139 instruction steps. The executed mutation failed only its
changed group; all seven damaged-file controls were rejected.
