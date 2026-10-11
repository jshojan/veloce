# veloce_smoke (GB) — provenance

Not an upstream suite: this is a first-party Veloce fixture that exercises
`tests/roms-src/include/gb/veloce-test.inc` (wla-gb dialect) and
`tests/roms-src/include/gb/veloce-test-rgbds.inc` (RGBDS dialect) end to
end, one smoke ROM per dialect. Source, license: same as the repo
(`emulatorplatform`); no upstream repo or SHA to pin.

Built by `build.sh` in this directory (wraps the pinned `tools/rom-toolchain`
image; `wla-gb`/`wlalink` for `smoke_wla.s`, RGBDS 0.6.1 for
`smoke_rgbds.asm`) into `build/roms/gb/veloce_smoke/`. Both `.gb` files are
gitignored (`*.gb`). Repository-owned archived original images elsewhere are
explicit exceptions to the ignore rule.

## What it proves

Both dialects' includes assemble and link without modification, and the
resulting ROM's serial output, read back through `VELOCE_TEST_OUT`, matches
the `VELOCE-RESULT/1` grammar exactly:

```
#VELOCE 1 core=GB rom_crc32=<hex> channels=serial,mooneye,a000,hram,blarggtext
VELOCE 1 gb veloce_smoke
TEST 1 putc
REG a=1f p=30
CHECK 1 PASS putc
TEST 2 fail_demo
CHECK 2 FAIL fail_demo exp=01a0 got=00a0
CHECK 3 FAIL fail_demo_masked exp=30 got=31 mask=c3
LOG smoke transcript complete
END 1/3 code=2
#VELOCE end reason=terminator frames=14 cycles=<n> status=2
```

(the RGBDS build's transcript is byte-identical apart from the suite name
`veloce_smoke_rgbds` and a `rom_crc32`/`cycles` that differ because the two
ROMs aren't the same bytes — RGBDS and wla-gb produce different binaries
for equivalent source, as expected).

Verified manually (not yet wired into a CI selftest — that is SH-5's job
once it lands, see the plan's ordering constraints in
`_audit-2026-09/testrom-plan.md` section 4):

- Both ROMs assemble/link cleanly against the pinned toolchain image
  (`veloce/rom-toolchain:dev`, `wla-dx` at `89a90a56...`, `rgbds-0.6.1` at
  `69a57392...`).
- Running each ROM twice under `VELOCE_TEST_OUT` produces byte-identical
  result files, including the `#VELOCE end ...` trailer (the determinism
  gate SH-5 will automate).
- `TEST`/`CHECK PASS`/`CHECK FAIL`/`CHECK FAIL ... mask=`/`REG`/`LOG`/
  `END ... code=` are all exercised by the one fixture.

## Why RGBDS 0.6.1, not 0.7.0, for the RGBDS smoke ROM

Either pin builds this fixture (it has no dependency on the syntax that
changed between them). 0.6.1 was picked because it is the older of the two
pins and so is the more conservative default for new RGBDS-dialect sources;
`veloce-test-rgbds.inc` itself has no version-specific construct and was
also spot-checked against 0.7.0 during development.
