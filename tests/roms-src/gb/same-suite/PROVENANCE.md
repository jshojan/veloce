> Current status (2026-10-11): all 78 images rebuild after the recipe's
> declared `SET` to `EQU` include-guard normalization. The old blocked report
> below is historical. This source-only pin has no shipped golden binary;
> `rom_variant` is now `veloce`.

# same-suite (GB) — provenance

- Upstream: https://github.com/LIJI32/SameSuite, pinned at
  `f71b4b3c3773b771f94955f5081dd2b36aabe54d` (X11).
- Intended build: upstream `Makefile` (`rgbasm -h -i include/`, `rgblink`,
  `rgbfix -jv`), RGBDS on `PATH`.
- `rom_variant: upstream`; `status: "blocked"` in `recipe.json`.

## Golden gate: blocked — 2019-era RGBDS `SET` syntax, both toolchain pins fail

`tests/roms-src/build.py gb/same-suite` fails to assemble even a single
file, unmodified, at this pinned SHA:

```
$ export PATH=/opt/rgbds-0.6.1/bin:$PATH && make
error: ...include/hardware.inc(29):
    syntax error, unexpected set
```

Same failure (plus more cascading errors from the same root cause) under
`/opt/rgbds-0.7.0/bin`. `include/hardware.inc` at this SHA is the classic
gbdev `hardware.inc`, revision 2.7 (dated 19-Jan-19), whose header-guard is
written the pre-RGBDS-0.5 way:

```asm
    IF !DEF(HARDWARE_INC)
HARDWARE_INC SET 1              ; <- line 29: bare "SYMBOL SET value"

rev_Check_hardware_inc : MACRO  ; <- line 31: colon-before-MACRO (also
                                 ;    deprecated, but only a warning)
```

RGBDS's `SET` directive changed meaning across the 0.5→0.6 boundary (it
stopped parsing as a bare assignment keyword in this position), so every
RGBDS newer than what c-sp itself uses for this suite rejects it outright.
This is confirmed against the source, not inferred: c-sp's own v7.0 release
script (`src/assemble.sh`, `build_same_suite()`, fetched from
`c-sp/gameboy-test-roms` at tag `v7.0`) builds this exact suite at this
exact SHA with **RGBDS v0.5.2** — a version older than either of Veloce's
two pinned RGBDS builds (`tools/rom-toolchain/Dockerfile`: 0.6.1 at
`69a57392...`, 0.7.0 at `08f3e360...`; the plan's decision, section 1 #10).

## What this blocks, and what doesn't need it

This blocks only the **golden gate** (byte-comparing an unmodified rebuild
against the c-sp v7.0 bundle already vendored at
`cores/gb/tests/roms/same-suite/`) and any future *unmodified* rebuild of
this suite. It does not block SameSuite's current scoring: the suite's
`quit` macro shares Mooneye's fingerprint dump, so the existing `mooneye`
Tier-C adapter (`cores/gb/src/test_channel.cpp`, landed in GB-1) already
reads its verdict from the vendored bundle binaries — those are prebuilt
and never rebuilt by this item.

## Two ways to unblock (neither attempted here — out of GB-2's scope)

1. Add an RGBDS 0.5.x stage to `tools/rom-toolchain/Dockerfile` (SH-3a is
   not this item's file to change without a separate call).
2. Patch `include/hardware.inc`'s header guard when GB-4 patches
   `include/base.inc` anyway to add the `VT_*` emitter (a two-line, purely
   mechanical fix: `HARDWARE_INC SET 1` → `DEF HARDWARE_INC EQU 1` and
   `rev_Check_hardware_inc : MACRO` → `MACRO rev_Check_hardware_inc`, per
   RGBDS's own deprecation warnings above) — at which point the suite is
   `rom_variant: veloce` anyway and the golden gate (which only applies to
   *unmodified* rebuilds) is moot for it.

Recorded as `status: "blocked"` / `blocked_reason` in `recipe.json` rather
than silently omitted, so this is tracked and `build.py`'s "N suite(s)
failed" is the expected, documented outcome until one of the above lands.

## Scored entries added by GB-3

`samesuite_dma` (gbc_dma_cont, gdma_addr_mask, hdma_lcd_off, hdma_mode0),
`samesuite_interrupt` (ei_delay_halt) and `samesuite_ppu`
(blocking_bgpi_increase) run the c-sp v7.0 bundle binaries
(`rom_variant: upstream`; this recipe is still blocked, see above).
`sgb/` is not enumerated because the core does not emulate an SGB.

Verdict source: `include/base.inc` compares each test's stored results
against the test's own `CorrectResults` table: the hardware behaviour the
suite documents (upstream README). It then sets the Mooneye fingerprint (3/5/8/13/21/34 pass,
`$42` x6 fail) and executes `LD B,B`. The core's `mooneye` adapter turns that
into `REG` + `END 1/1` / `END 0/1 code=66` (`channel: mooneye`, Tier C). GB-4
patches `base.inc` to emit a `CHECK` per table index instead.


## Integration correction (2026-10-11)

The previous build blocker is fixed by a declared recipe command that changes
`HARDWARE_INC SET 1` to `HARDWARE_INC EQU 1` before assembly with RGBDS 0.6.1.
The include guard has one immutable value; no instruction or expected result
changes. This is a source-only suite with no shipped binary golden gate. The
modified recipe is marked `rom_variant: veloce`. Old release-bundle comparisons
and historical emulator results are not fresh hardware validation.
