# Original SNES math register corpus

This original 32 KiB LoROM emits observed register values to the optional
`$21FE` host port. Python 3 builds it without an assembler, Docker, or network.
The committed image and generator hashes are recorded in `rom_manifest.json`.

```bash
python3 tests/roms-src/build.py --original-only --check
cmake --build build --target veloce snes_plugin
ctest --test-dir build -R snes_math_registers --output-on-failure
```

Measured 2026-10-09: **68,864/68,864 settled math cases matched**:

| Domain | Enumerated inputs | Observations | Coverage |
|---|---|---|---|
| Unsigned multiplication | Both operands 0–255 | `$4217:$4216` product and `$4215:$4214` = multiplier | 65,536/65,536 pairs, 100% of settled operand domain |
| Unsigned division | Dividends 0, 1, 2, 127, 128, 254, 255, 256, 257, 32767, 32768, 65534, 65535; divisors 0–255 | `$4215:$4214` quotient and `$4217:$4216` remainder | 3,328 boundary cases, 13/65,536 dividends; not exhaustive |

The ROM writes `$4202/$4203` for multiplication and `$4204/$4205/$4206` for
division. It waits 16 and 32 CPU cycles respectively, then captures every
result byte in WRAM before formatting output. The host uses Python integer
multiplication and `divmod`, with quotient `$FFFF` and remainder equal to the
dividend for divisor zero. These behaviors are documented in
[nocash's original fullsnes hardware specification](https://problemkaputt.de/fullsnes.htm#snesmathsmultiplydivide).

The ROM also records the host signature in bank `$80` and ordinary WRAM
roundtrips at `$7E:21FF` and `$7F:21FE`, checking that those WRAM offsets remain
memory while the sink is attached. This checks selected mappings, not the full map.

The ROM reports completion with `$21FF=1`, an unknown verdict. The host runner
requires the exact core, standard ROM CRC32, channel, group order, observation
counts, syntax, and completion trailer before grading. It reads raw files;
screenshots and stdout do not determine hardware verdicts.

The runner changes the multiplier trigger to the division trigger in a smaller
ROM and requires FAIL: 256 multiplication mismatches among 464 total cases;
division still passes. Missing, malformed, reordered, and truncated files must
be measurement errors. Raw output, control ROMs, and JSON reports remain in
`build/artifacts/owned/snes_math/`; CI uploads that directory.

The additional RDDIV side-effect observation exposed 65,280 mismatches before
the core fix; all 68,864 cases now match. The before-fix raw file and report
are retained locally alongside the passing artifacts.

## Remaining work

The current core calculates math results immediately. This suite checks settled
values, so it cannot verify the documented operation latency, intermediate
results, or overlapping operations. Those require timing corpora and core work.
It does not verify native CPU arithmetic, Mode 7 multiplication, DMA, interrupts,
PPU/APU, cartridge hardware, or whole-core accuracy. No whole-core completion
percentage follows from this domain's 100% multiplication coverage.
