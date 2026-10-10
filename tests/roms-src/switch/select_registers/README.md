# Original Switch AArch64 conditional-select register guest

MIT repository-owned NSO0, original Python assembler, no SDK, firmware, keys or
commercial bytes. The guest contains no expected answers. Real STR instructions
publish X0, X1, result X2, MRS NZCV X3 and context X5 through emulator-only
`vnxdebug`; the host strictly grades these actual values from a file.

## Domain

**524,288 rows / 2,621,440 captured 64-bit words**: 16 unique full-width boundary
values in every ordered pair × all 16 initial NZCV values × all 16 condition
codes × CSEL/CSINC/CSINV/CSNEG in both W and X widths. Values include zero,
all-ones, signed minima/maxima, W truncation boundaries and mixed bit patterns.
AL and NV both select the first source. False conditions select the second
source unchanged, incremented, inverted or negated respectively; W writes
zero-extend. Every form preserves flags. This is a finite operand domain,
not exhaustive 64-bit pairs, physical timing or full Switch compatibility.

The separate oracle uses a complete boolean condition table and unbounded
Python integer arithmetic, with fixed overflow/condition assertions. It checks
identity, exact domain, each field, row count, completion, terminator and EOF.
An executed EOR mutant flips a captured result bit and fails all **8,192** short
rows. Five individual field mutations fail once each; seven malformed stream
controls error. Source operands are graded too: a wrong boundary constant in
the initial builder was detected before shipment and corrected.

The component independently checks **151,846,912 snapshots** using complete
condition truth masks and bit-level increment/inversion/negation:

- 151,519,232 repeated-byte/boundary pair, form, NZCV and condition cases.
- 327,680 register routing cases: every register number, both source aliases,
  XZR destination, alternate destination, all conditions/flags and eight forms.
- Reserved S/bit-11 encodings reject with preserved result/flags and diagnostics.

A production ABI gate checkpoints pending guest execution and verifies identical
actual register streams and full `VNXS0001` states after restore. The decoder
failed the new component before implementation; the previous production core
stopped the guest without any register rows. Portable unsigned operations avoid
host signed-overflow behavior. Encoding fields are separately compared with
[pinned LLVM tests](../../../reference/switch-select/README.md); this does not
claim execution on physical hardware.

```bash
python3 tests/roms-src/build.py switch/select_registers --original-only --check
cmake -S . -B build
cmake --build build --target veloce switch_plugin switch_select_test switch_plugin_state_test -j
ctest --test-dir build -R '^switch_select' --output-on-failure
```

Artifacts: `build/artifacts/owned/switch_select/`. Switch stays 25% delivery
completion; full CPU, hardware, native boot, graphics/audio/controllers/netplay
and physical corpus remain pending. CI/CD remains disabled.
