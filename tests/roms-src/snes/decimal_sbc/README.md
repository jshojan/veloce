# Original SNES decimal SBC register captures

This original MIT 4 MiB LoROM executes native 16-bit immediate SBC and writes
actual A and P to `$21FE`. It emits no expectations or verdicts. A host oracle
uses complement addition with decimal correction and checks N/V/Z/C plus the
preserved D/I/X/M bits. Valid BCD inputs also have an independent integer-decimal
cross-check. The host imports no emulator implementation.

```bash
cmake -S . -B build
cmake --build build --target snes_plugin veloce
ctest --test-dir build -R '^snes_decimal_sbc$' --output-on-failure
python3 tests/roms-src/build.py --original-only --check
```

Coverage is **8,193 cases / 16,386 actual register observations**:
four background words ($0000/$9999/$FFFF/$5555) × four nibble positions × all
16 left digits × all 16 right digits × both incoming carries, plus the pinned
catalog regression $AB1D − $F1E2 with C=1. That case must yield $59DB. The
fixture preserves I/X, so its expected P=$1C corresponds to the catalog's
P=$08 after excluding those preserved control bits.

There are **2,400 valid-BCD cases** and **5,793 invalid-digit cases**, including
repeated stimuli. This enumerates every digit pair and carry at each position
under the stated backgrounds. It does **not** exhaust the 16-bit operand-pair
domain, instruction addressing modes, 8-bit/emulation modes, or timing.
The invalid-digit model agrees with the pinned regression; the other generated
invalid-digit observations have not been independently captured on physical
hardware. Passing them establishes model consistency, not full hardware accuracy.

References: [WDC W65C816S datasheet](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf)
for decimal mode and flags, and the [pinned gilyon catalog](../gilyon_cputest/PROVENANCE.md)
for case 061a's expected accumulator/status. The manufacturer document does not
supply exhaustive expected outputs for invalid decimal digits.

The original ROM exposes **433 accumulator mismatches** on main 49a19fc,
including 061a. The repaired plugin passes all pairs. Positive captures repeat
byte for byte. An executed original EOR mutant flips the regression's captured
low accumulator byte and produces exactly one mismatch; missing, reordered and
truncated streams are rejected as measurement errors.

Artifacts and JSON reports remain under `build/artifacts/owned/snes_decimal_sbc/`;
private/retail ROMs are not involved. The generator is standalone and archived
with its image/source hashes in `rom_manifest.json`.
