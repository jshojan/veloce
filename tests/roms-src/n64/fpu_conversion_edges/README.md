# Original N64 conversion and rounding edge corpus

This MIT licensed assembler emits an original cartridge without SDK, firmware or
commercial bytes. It executes CVT.S/D/W/L and ROUND/TRUNC/CEIL/FLOOR W/L forms;
its own exception handler recovers and streams actual register observations.

**289,536 rows / 1,447,680 captured words** cover:

- All 256 single and 2,048 double exponent fields, both signs, four fraction
  patterns, the three valid CVT destinations and all four rounding modes: 221,184 rows.
- Sixteen selected single values × all twelve conversion functions × four
  rounding controls × all 32 enable masks × both FS settings, with preexisting
  sticky flags and condition bit: 49,152 rows.
- Every 64-bit integer bit position with three power/boundary/sign patterns,
  both W/L source formats, all functions and rounding controls: 18,432 rows.
- All sixteen source format encodings in the arithmetic range, all functions
  and rounding controls: 768 rows.

Each row captures destination, FCSR, exception-handler marker, CPU exception code
and unchanged source. Defined destination bits and all other words are graded by
an independent exact-rational Python oracle; undefined upper bits of successful
word results are recorded and ungraded. It imports no emulator code and uses no
native floating-point rounding. The guest never computes a pass verdict.

An executed capture mutation fails all 1,536 reduced rows; five field mutations
fail individually and seven malformed/incomplete streams are ERROR.
Reports and raw observations live in `build/artifacts/owned/n64_fpu_conversion_edges/`.
The committed ledger records image/generator hashes.

The component adds **4,194,304 exact signed-16-bit/index cases**, **229,376
rounding/enable/sticky/FS/condition combinations** over fourteen boundary profiles,
all **98,304 register-pair/FR/format/function combinations**, fixed rounding,
CU1 precedence, long-target privilege, delay/nested/BEV context, cause replacement
and deterministic pending-conversion state replay.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce n64_plugin n64_fpu_conversion_test -j
ctest --test-dir build -R '^n64_fpu_(conversion_edges|conversion_test)$' --output-on-failure
```

Rules follow the NEC [VR4300 manual](https://hack64.net/docs/VR43XX.pdf), chapters
7–8, conversion entries and Appendix D, supplemented by the hardware-test author's
[conversion cases](https://github.com/lemmy-64/n64-systemtest/blob/main/src/tests/cop1/mod.rs)
and [raw constants](https://github.com/lemmy-64/n64-systemtest/blob/main/src/cop1.rs).
The published cases clarify manual ambiguities: floating infinity survives
precision conversion; NaNs with top payload bit 1 raise invalid with a canonical
legacy result, while top-bit-0 NaNs need software assist. Integer destinations
reject both NaN types and infinity. Long-source conversion accepts [-2^55, 2^55);
long destinations require magnitude below 2^53. Subnormal inputs always require
assist. Tiny results use FS/enable/rounding rules; trapped conversions update cause,
preserve prior sticky flags and suppress destination writes.

These are finite selected domains, not the full independent operand product or
physical hardware certification. Undefined odd FR=0 operands use the core's RI
convention. Nonzero unary `ft` fields use an assist convention; physical behavior
of that encoding is ungraded. Pipeline hazards, precise CE timing, reverse-endian
behavior and other FPU arithmetic/comparison remain pending. No physical console
has validated these cartridges. N64 remains 40% on the delivery checklist.
