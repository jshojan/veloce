# Original Switch AArch64 conditional-compare flag guest

Original MIT assembler and archived uncompressed NSO0. No SDK, firmware, keys,
commercial bytes or expected answers in the guest. Actual CCMP/CCMN instructions
execute; MRS reads each result, UBFM extracts its NZCV nibble, and ORR packs
sixteen fallback results into X4. STR stores X0 (left), X1 (right/immediate),
X6 (incoming flags), X4 (actual packed results) and context X5 to emulator-only
`vnxdebug`, graded independently from the output file.

## Domain

**786,432 rows / 3,932,160 actual captured words / 12,582,912 individual flag
observations**. Each row packs the results for all fallback NZCV values 0–15.

- CCMP/CCMN W/X register operands: all ordered pairs of sixteen distinct
  boundary/mixed-bit values × sixteen incoming flags × sixteen conditions ×
  four forms = 262,144 rows / 4,194,304 flag observations.
- Immediate operands: sixteen left values × all 32 five-bit immediates ×
  sixteen incoming flags × sixteen conditions × four forms = 524,288 rows /
  8,388,608 flag observations.

Both true arithmetic and false fallback branches are checked. AL and NV are
always true. Incoming C does not feed CCMP/CCMN arithmetic. All GPRs/SP stay
unchanged; register operands use XZR. W arithmetic uses low 32 bits. Independent
unbounded signed/unsigned arithmetic and a complete boolean condition table
compute each expected nibble. Fixed references check overflow/carry and true/
false packed results. This is finite input coverage, not every 64-bit pair.

Identity, domain, every captured field, exact rows/completion/trailer and EOF
are strict. The executed result-bit mutant fails all **12,288** short rows.
Five field controls and sixteen separate packed-lane controls fail once each;
seven malformed-stream controls error. Every lane is graded independently.

The separate truth-mask/bit-serial full-adder component passes **14,155,776
snapshots**: 4,194,304 register, 8,388,608 immediate and 1,572,864 alias/XZR cases.
It covers every condition, incoming/fallback flag value, all register numbers,
coincident sources and every five-bit immediate. Reserved S/bit-4/bit-10 forms
reject with unchanged architectural state. The old core fails this component
and emits no guest register row. Production ABI replay matches actual packed
output and complete `VNXS0001` states. [Pinned LLVM opcode fields](../../../reference/switch-ccmp/README.md)
match all 2,112 compare base words; physical execution remains unverified.

```bash
python3 tests/roms-src/build.py switch/ccmp_registers --original-only --check
cmake -S . -B build
cmake --build build --target veloce switch_plugin switch_ccmp_test switch_plugin_state_test -j
ctest --test-dir build -R '^switch_ccmp' --output-on-failure
```

Artifacts: `build/artifacts/owned/switch_ccmp/`. Native timing, full CPU/extensions/
exceptions, memory/MMU/devices, HOS/boot/retail support, GPU/audio/controllers,
saves/netplay and complete physical/native corpus remain incomplete. Switch
stays 25% delivery completion. CI/CD remains disabled.
