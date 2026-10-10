# Original N64 integer-to-floating-point conversion corpus

This MIT licensed assembler emits an original cartridge with no SDK, firmware or
commercial bytes. It runs CVT.S.W and CVT.D.W on every repeated 16-bit word input
in all four FCSR rounding modes: **524,288 rows / 2,621,440 captured words**.
The five actual 64-bit captures are destination, FCSR, exception-handler marker,
CPU exception code and unchanged source. The host grades defined result bits and
all four other captures; successful single results have architecturally undefined
upper bits, which are recorded but ungraded.

The original guest copies its own recovery handler to the ordinary kernel vector
and patches an uncached instruction slot from its input table. FPE suppresses the
destination; the guest reports what happened without computing a verdict.
The independent exact-rational host oracle shares only input-case descriptions
with the builder, never expected values or emulator conversion code. It checks
rounding, sticky/cause/control fields and enabled exception suppression.

An executed capture mutation fails all 256 reduced rows. Five field mutations
fail individually, and seven malformed or incomplete streams are ERROR.
Identity, declared dimensions, rows, completion and trailer are checked.
Reports and raw observations are in `build/artifacts/owned/n64_fpu_integer_conversions/`;
the committed ledger records generator and archived-image hashes.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_fpu_conversion_test -j
ctest --test-dir build -R '^n64_fpu_(integer_conversions|conversion_test)$' --output-on-failure
```

This exhausts the declared repeated-word input domain, not all 2^32 independent
integer payloads or the whole FPU. Rules follow the NEC
[VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapters 7–8 and the conversion
instruction entries. See [edge-case coverage](../fpu_conversion_edges/README.md).
No physical console has validated this cartridge; pipeline hazards and timing
remain ungraded. N64 remains 40% on the delivery checklist.
