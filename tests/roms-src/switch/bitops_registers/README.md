# Original Switch AArch64 variable-shift and unary register guest

Original MIT Python assembler and archived uncompressed NSO0, with no SDK,
firmware, keys or commercial bytes. There are no expected answers in the guest.
Real STR instructions capture X0 (source), X1 (shift count, zero for unary),
X2 (result), actual MRS NZCV X3 and context X5 through emulator-only `vnxdebug`.
A strict independent host oracle grades the actual output file.

## Domain

**1,120,096 rows / 5,600,480 actual 64-bit words**:

- 272 operands: every repeated-byte pattern and sixteen distinct full-width
  boundaries, including signed minima, all-ones and mixed patterns.
- For each operand, every count 0–255, two NZCV seeds (9/F), and LSLV, LSRV,
  ASRV, RORV in both W and X widths: 1,114,112 rows.
- RBIT, REV16, REV32, CLZ and CLS in W width; RBIT, REV16, REV32, REV64,
  CLZ and CLS in X width, both flag seeds: 5,984 rows.

Variable counts use the low five/six bits. Arithmetic shifts sign-fill;
zero rotations avoid shifts by the host word width. Unary operations reverse
bits/bytes within specified groups, or count leading zero/sign bits. CLS
excludes the sign bit. All instructions preserve NZCV, and W writes zero-extend.

The host uses separate unbounded arithmetic, binary strings and byte sequences
with fixed known references. Identity, domain, each field, row count, completion,
trailer and EOF are strict. An executed EOR mutant fails all **172** short rows;
five field mutations fail once each and seven malformed-stream controls error.

The independent component uses per-bit source mapping and checks **9,218,304
snapshots**: 9,121,792 shift cases (272 operands × 262 counts × eight forms × all
sixteen NZCV values), 47,872 unary cases, and 48,640 register/alias/XZR cases.
Extra counts exercise upper count bits and width boundaries. It verifies all
registers, SP, flags and PC/step preservation; invalid unary opcode/Rm/S and
variable-shift controls reject. The previous core failed the component and
emitted no guest register row, establishing the missing instruction path.

Production ABI checkpoint/replay compares real guest output and complete
`VNXS0001` states. The [pinned LLVM encoding comparison](../../../reference/switch-bitops/README.md)
checks all nineteen opcode forms. These are finite domains, not full-width
operand enumeration, complete ISA/hardware accuracy or physical timing.

```bash
python3 tests/roms-src/build.py switch/bitops_registers --original-only --check
cmake -S . -B build
cmake --build build --target veloce switch_plugin switch_bitops_test switch_plugin_state_test -j
ctest --test-dir build -R '^switch_bitops' --output-on-failure
```

Artifacts: `build/artifacts/owned/switch_bitops/`. Full native CPU, memory/MMU,
boot/HOS, graphics/audio/controllers/devices, saves/netplay and physical corpus
remain incomplete. Switch stays 25% delivery completion. CI/CD remains disabled.
