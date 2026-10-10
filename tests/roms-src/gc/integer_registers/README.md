# Original GameCube PowerPC arithmetic register corpus

MIT-licensed repository-owned Python assembler; no SDK, firmware or retail
assets. The marked image runs on the [supplied PowerPC profile](../../../../cores/ppc/README.md).

All 49,152 cases execute ADD/ADDC/ADDE/SUBF/SUBFC/SUBFE with sixteen
operand patterns squared, eight incoming SO/OV/CA combinations, both OE and
both Rc. Patterns include signed/unsigned endpoints and full-width mixed bits.
Every case streams result, CR, XER, source A, source B and a sentinel: six
32-bit words captured after the operation. The ROM has no expected values or
pass logic. The host independently grades widened mathematical addition and
subtraction, carry/no-borrow, signed overflow, sticky SO and CR0 fields.

`run.py` requires exact file identity/CRC/channel, suite/domain, row count,
completion and an ungraded terminator. It also runs a 768-case positive ROM
and an executed result mutation (768 failures required), flips every captured
field independently, and rejects seven malformed/truncated/false-verdict
streams. Raw `.result` files and JSON reports are retained under
`build/artifacts/owned/gc_integer/`. Archived image and generator SHA256
are recorded by the original-ROM ledger.

This covers the declared boundary domain, not all 32-bit operands or all
hardware. Timing, physical Gekko/Broadway differences, graphics/audio,
firmware, controllers and netplay remain unverified or unimplemented.

```sh
python3 tests/roms-src/build.py gc/integer_registers --check
python3 tests/roms-src/gc/integer_registers/run.py --emulator build/bin/veloce
```
