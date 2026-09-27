# blargg (GB): provenance

- Upstream: https://github.com/retrio/gb-test-roms, pinned at
  `c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15`. This is the source mirror
  c-sp/gameboy-test-roms builds from. c-sp's v7.0 release script
  (`src/assemble.sh`, `build_blargg()`) pins the same SHA and copies the
  repo's checked-in `.gb` files. No assembler is invoked, because
  `retrio/gb-test-roms` ships built binaries.
- Licence: none in the repo. The readmes carry no restriction. This suite is
  **patch-only**: only this file and `recipe.json` are committed, never a
  built or modified ROM.

## What is built here (GB-3): halt_bug, interrupt_time

`recipe.json` produces two `rom_variant: veloce` ROMs into
`build/roms/gb/blargg/`. The GB test config addresses them as
`veloce/blargg/<rom>`, through the `cores/gb/tests/roms/veloce` link that
`cores/gb/tests/run_tests.py` maintains. Their sha256 values, tool versions
and image id are recorded in `tests/roms-src/rom_manifest.json`. Rebuild
them with `tests/roms-src/build.py gb/blargg`, and verify with `--check`.

**Why.** GB-3 first ran both upstream ROMs with the `a000` adapter, as the
plan asked: "probe at runtime; rebuild on the newer shell if no
signature". Both reached 2500 frames with no verdict. The cause is not an
old shell. Both ROMs already run blargg's newer shell and write the full
`$A000` block (status, `$DE $B0 $61`, text). Their header, however,
declares MBC1+RAM (`$0147 = $02`) with RAM size 0 (`$0149 = $00`). A core
that honours the header maps no cart RAM, so every write to `$A000-$BFFF`
is lost. The other blargg suites that use the `$A000` block declare 8 KiB
(`common/build_rom.s`: `.ramsize 02`).

**The change.** `rgbfix -r 2 -f hg` (RGBDS 0.7.0 from the pinned toolchain
image) sets `$0149 = $02` and recomputes the header checksum. No code or data
byte changes, and the global checksum stays valid. `cmp -l` of upstream
against the rebuild:

```
halt_bug.gb        offset $0149: 00 -> 02   offset $014D: 65 -> 63
interrupt_time.gb  offset $0149: 00 -> 02   offset $014D: 25 -> 23
```

There is no source rebuild. halt_bug has no source anywhere upstream.
interrupt_time's single `.s` needs `shell.inc`, `cpu_speed.s`, `timer.s` and
`apu.s`, which live in three different suites' `common/` directories at
different shell versions. A mixed rebuild would differ from the shipped
binary in code, not just in the header, so there would be nothing to
golden-gate it against. `golden_gate` is `false`: the recipe's input *is*
upstream's shipped binary, which is sha256-identical to the c-sp v7.0
bundle copy.

**Result with the header fix** (core at this commit, `VELOCE_TEST_OUT` file):
- halt_bug prints its IE/IF table and `Passed`, then `END 1/1` (106 frames).
- interrupt_time prints `00 00 00` / `00 08 0A` twice, then `Failed` and
  `END 0/1 code=1` (32 frames). The ROM's own CRC check fails. The columns
  are CPU speed, the IF value written, and the measured cycles. A serviced
  interrupt takes `$0A` = 10 cycles where the source says 13. The second pass,
  after `cpu_fast`, still reports speed 0 (worth checking the KEY1
  speed switch). These are real core findings, not harness artefacts.

## Expected-value sources

No expected value is written into the Veloce config. Every verdict is the
ROM's own:
- blargg's shell compares a CRC of everything it printed against a constant
  compiled into the ROM (`check_crc`), or tests a condition directly. It
  then writes the result code to `$A000`: 0 pass, n = failing subtest.
  The core's `a000` adapter (`cores/gb/src/test_channel.cpp`) streams that
  as `END 1/1` / `END 0/1 code=n`.
- `expected: pass` in `test_config.json` records the hardware result from the
  c-sp v7.0 howto (`cores/gb/tests/roms/blargg/game-boy-test-roms-howto.md`):
  - halt_bug, mem_timing-2: pass on DMG-C and CGB-B/C/E.
  - interrupt_time, cgb_sound: CGB-C/E. interrupt_time is CGB-only.
  - dmg_sound: DMG-C.
  - oam_bug is verified on DMG only. Its ROMs set `$0143 = $80`, so this
    core runs them as CGB. They stay `known_fail` until the core can be told
    to run a DMG model (`known_issues.model_selection`).

## The rest of the suite

- `cpu_instrs`, `instr_timing` and `mem_timing` have no `$A000` block. The
  `blarggtext` adapter reads their final serial line (`Passed` /
  `Failed [#n]`).
- `mem_timing-2`, `dmg_sound`, `cgb_sound` and `oam_bug` write the `$A000`
  block into an 8 KiB cart RAM they declare. The `a000` adapter reads them
  unmodified (`rom_variant: upstream`).
- GB-6 (P1) patches blargg's shell to emit `CHECK`/`END` over serial
  directly, which makes these Tier A.

sha256 of the vendored upstream binaries (first 8 hex characters; full
digests are reproducible from `cores/gb/tests/roms/blargg/`):

```
8c5e12f4  blargg/cpu_instrs/cpu_instrs.gb
646067b3  blargg/instr_timing/instr_timing.gb
791cb418  blargg/mem_timing/mem_timing.gb
ab9daf4c  blargg/mem_timing-2/mem_timing.gb
ddebfb81  blargg/halt_bug.gb                        (input to recipe.json)
1a8f8f3b  blargg/interrupt_time/interrupt_time.gb   (input to recipe.json)
c34e7406  blargg/dmg_sound/dmg_sound.gb
de2101eb  blargg/cgb_sound/cgb_sound.gb
21f0e7af  blargg/oam_bug/oam_bug.gb
```
