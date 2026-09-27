# blargg (GB) — provenance and GB-2 decision: stays prebuilt

- Upstream: https://github.com/retrio/gb-test-roms (the source mirror
  c-sp/gameboy-test-roms itself builds from). c-sp's v7.0 release script
  (`src/assemble.sh`, `build_blargg()`) pins
  `c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15` and just copies the repo's own
  checked-in `.gb` files (no assembler is invoked — `retrio/gb-test-roms`
  ships built binaries directly, there is no build step to golden-gate).
- No `recipe.json` in this directory on purpose: an empty-commands recipe
  is rejected by `build.py` (`"recipe has no build commands"`), and there
  is nothing to rebuild — see above.

## Why this item doesn't rebuild Blargg from source

GB-2's brief allows it explicitly ("Blargg text suites may stay prebuilt
since they already emit"). Concretely, today, unmodified:

- `cpu_instrs`, `instr_timing`, `mem_timing` (`rom_singles`) have no
  `$A000` block; their final serial line (`Passed` / `Failed [#n]`) is
  read by the `blarggtext` adapter GB-1 added
  (`cores/gb/src/test_channel.cpp`), giving a real `END` verdict already.
- `mem_timing-2`, `dmg_sound`, `cgb_sound`, `oam_bug`, `halt_bug`,
  `interrupt_time` do have blargg's `$A000` result block, read by the
  `a000` adapter GB-1 also added.

Both adapters are Tier B (`channel: a000|blarggtext`, `rom_variant:
upstream`) per `docs/testing/VELOCE-RESULT.md` section 4 — scored today,
without any source rebuild. Patching Blargg's shell (`testing.s`, per-suite
`$A004` text) to emit `VT_*`/`CHECK` lines directly and promoting these to
Tier A is GB-6 (P1), which also needs the source mirror actually cloned
and built (unlike this repo, which ships binaries only) — out of scope for
GB-2's golden-gate/toolchain-proving brief.

## sha256 of what's currently vendored (informational only)

Not written into `rom_manifest.json`: these ROMs are, and remain,
`rom_variant: upstream` — the manifest only gates `rom_variant: veloce`
entries (`tests/veloce_testkit/rom_manifest.py` docstring), so an unbuilt,
unpatched suite needs no manifest row. Recorded here instead, so "what did
GB-2 actually look at" is on record:

```
8c5e12f4  blargg/cpu_instrs/cpu_instrs.gb
646067b3  blargg/instr_timing/instr_timing.gb
791cb418  blargg/mem_timing/mem_timing.gb
ab9daf4c  blargg/mem_timing-2/mem_timing.gb
ddebfb81  blargg/halt_bug.gb
1a8f8f3b  blargg/interrupt_time/interrupt_time.gb
c34e7406  blargg/dmg_sound/dmg_sound.gb
de2101eb  blargg/cgb_sound/cgb_sound.gb
21f0e7af  blargg/oam_bug/oam_bug.gb
```
(first 8 hex chars of each sha256; full digests are reproducible from the
files themselves, already vendored at `cores/gb/tests/roms/blargg/`.)
