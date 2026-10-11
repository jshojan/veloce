# Original Switch AArch64 bit-field/extract register guest

Original MIT assembler and archived uncompressed NSO0. No firmware, SDK, keys
or commercial bytes. There are no expected answers in the guest. Actual STR
stores emit X0 (source/high), X1 (complement source/low), X6 (prior destination),
X2 (result), MRS NZCV X3 and context X5 through emulator-only `vnxdebug`.
The strict host oracle grades these actual values from the output file.

## Domain

**494,592 rows / 2,967,552 actual 64-bit words**:
16 distinct boundary/mixed-bit input values × two prior destinations (zero and
all-ones, with NZCV seeds 9/F) × **15,456 instruction words**:

- SBFM, BFM and UBFM in both W/X widths: every legal rotate/end combination,
  3 × (32² + 64²) = 15,360 instructions.
- EXTR in both widths: every legal extraction amount, 32 + 64 = 96 instructions.

BFM preserves destination bits outside the selected field; signed fields extend
the source end bit; unsigned fields zero-fill. W writes zero-extend and NZCV is
unchanged. EXTR extracts from Rn:Rm (high:low), including amount zero. Source
and destination aliases, XZR and SP isolation are covered by the component.
The guest's far labels use original ADRP/ADD fixups; the large backwards loop
uses an unconditional branch after a short conditional branch, so no branch
immediate silently overflows. Code is read/execute, RO is read-only, as before.

The independent host oracle uses contiguous field removal/insertion and
unbounded double-width arithmetic. Fixed known references check sign extension,
wrapping insertion and full fields. Identity, domain, every field, count,
completion/trailer and EOF are strict. The executed EOR mutant fails all **240**
short rows; six field mutations fail once each and seven malformed streams error.

The independent per-bit component passes **247,414,784 snapshots**:

| Domain | Snapshots |
|---|---:|
| Every legal bit-field immediate, 272 repeated-byte/boundary inputs, two old destinations and all sixteen NZCV values | 133,693,440 |
| Every legal EXTR amount, all 272×272 input pairs and all sixteen NZCV values | 113,639,424 |
| Every register number, five alias/XZR destinations, all flags and four immediate boundary shapes across eight forms | 81,920 |

Reserved opcode/N/width/immediate encodings reject before architectural writes.
The previous core fails the new component and stops the guest before any row.
UBSAN passes. Production ABI restore/replay matches actual emitted registers
and full `VNXS0001` states. Every guest instruction word matches the
[pinned LLVM opcode fields](../../../reference/switch-bitfield/README.md).
These are finite domains, not every pair of full-width inputs or physical timing.

```bash
python3 tests/roms-src/build.py switch/bitfield_registers --original-only --check
cmake -S . -B build
cmake --build build --target veloce switch_plugin switch_bitfield_test switch_plugin_state_test -j
ctest --test-dir build -R '^switch_bitfield' --output-on-failure
```

Artifacts: `build/artifacts/owned/switch_bitfield/`. Switch remains 25% delivery
completion; full CPU/extensions/exceptions, native memory/MMU, firmware/HOS,
boot/retail compatibility, GPU/audio/controllers, saves/netplay and the complete
physical/native hardware corpus remain unfinished. CI/CD remains disabled.
