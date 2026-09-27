# gbmicrotest (GB) — provenance

- Upstream: https://github.com/aappleby/GBMicrotest, pinned at
  `f3b55497c1d1202c784b8201dafc888c838b7302`.
- Licence: the pinned checkout carries no LICENSE file and its README does
  not name one (checked 2026-09-27; an earlier draft here said "MIT" without
  a source). Treated as **patch-only**, like the other unlicensed suites:
  only this file and `recipe.json` are committed, never a built or modified
  ROM, and nothing from upstream is vendored.
- Built with the upstream `build.sh` (loops `wla-gb -Itests -DDMG` +
  `wlalink` over every `tests/*.s`, into `bin/`), `wla-gb`/`wlalink` from
  the toolchain image's `wla-dx` stage.
- `rom_variant: upstream` (no Veloce patch — GB-7 lands the `hram`-adjacent
  `VT_*` patch to `tests/macros.inc:test_end2` per the plan, section 6.3).

## Golden gate: build.py's own gate ran (upstream ships bin/*.gb)

Unlike Mooneye/SameSuite, GBMicrotest's `bin/` directory *is* tracked at
this pinned SHA — `build.sh`'s first line, `rm -f bin/*`, is there because
the repo ships (slightly stale, see below) prebuilt binaries there.
`build.py`'s shipped-then-delete-then-rebuild gate therefore ran for real
against all 513 artifacts: **509/513 match**. `recipe.json` still sets
`golden_gate: false` rather than `true`, because a `true` gate raises on
*any* mismatch and would permanently fail this suite's build over 4 files
that are upstream drift, not a Veloce or toolchain problem:

```
line_153_lyc_a.gb   golden(shipped)=a43c59db… rebuilt=92015561…
line_153_lyc_b.gb   golden(shipped)=8acdbfdc… rebuilt=85e14c37…
line_153_lyc_c.gb   golden(shipped)=a43c59db… rebuilt=3b937554…  (same golden as _a — the shipped
                                                                   copies of _a and _c are byte-identical
                                                                   to each other, a pre-existing upstream
                                                                   oddity, unrelated to this rebuild)
ppu_spritex_vs_scx.gb golden(shipped)=7a0c9e45… rebuilt=25d4568d…
```

Full hashes are in `rom_manifest.json` under each ROM's
`golden_gate.{golden_sha256,rebuilt_sha256}`.

## Golden gate against the c-sp v7.0 bundle

Compared all 513 rebuilt ROMs (`build/roms/gb/gbmicrotest/`) against the
same 513 filenames already vendored locally at
`cores/gb/tests/roms/gbmicrotest/` (c-sp/gameboy-test-roms v7.0, gitignored):

```
match=513 mismatch=0
```

**Including the 4 files that differ from upstream's own committed `bin/`.**
c-sp's release script (`build_gbmicrotest()`) rebuilds from source the same
way this recipe does (`rm -f bin/*; ./build.sh`), so its v7.0 release ROMs
reflect the *current* source tree at this SHA, same as our rebuild — both
disagree with the SHA's own stale committed binary in the same 4 files, by
the same bytes. This is upstream's committed-artifact drift, already
present at the pinned commit, not something this rebuild introduced.

## Tool versions used for this build

Recorded per-ROM in `rom_manifest.json` (`tool_versions.wla-dx`); image id
also recorded (`toolchain_image_id`).

## Scored entries (GB-3): what is enumerated and where the verdict comes from

`cores/gb/tests/test_config.json` scores 212 upstream ROMs in six
`gbmicrotest_*` suites (`rom_variant: upstream`, the bundle binaries, which
match this recipe's rebuild byte for byte). The selection is the plan's
category list (timer/div, dma, hblank_int_scx, halt, lcdon_to_stat,
VRAM/OAM lock), taken from the ROMs that report a result. It was not chosen
by pass/fail.

**Verdict.** Every scored ROM ends in a `tests/macros.inc` `test_end2`-family
macro (`test_finish_*`, `test_pass`, `test_fail`, `test_assert_a`). These write
`$FF80` = observed, `$FF81` = expected and `$FF82` = `$01` pass / `$FF` fail.
The expected constants are compiled into each test from upstream's
real-hardware measurements (README: "All tests in this repo have been
checked on real hardware (version DMG-CPU-08 I believe)"). The core's `hram`
adapter (`cores/gb/src/test_channel.cpp`, armed only for the cart title
`microtest`) reads the triple at frame end. It writes
`CHECK 1 PASS|FAIL hram exp=<$FF81> got=<$FF80>` + `END` to the result file.
`$FF82` alone decides the verdict. The c-sp howto warns that `$FF80`/`$FF81`
are not always consistent, and one ROM (`mbc1_rom_banks`) fails with exp == got.

**Not scorable (31).** These sources contain no result macro (checked with
`grep -E 'test_(end2|pass|fail|finish|assert)' tests/*.s`): the numbered
`000-`..`803-` display probes, the testbenches (`audio_testbench`,
`ppu_sprite_testbench`, ...), `minimal`, `poweron`, `flood_vram`,
`toggle_lcdc`, etc., and `temp.s`, an empty stub. The list is in
`known_issues.gbmicrotest_scope`.

**Left for GB-7/GB-8 (270).** poweron_*, win*, sprite*, line_153*,
stat_write_glitch*, lyc/oam/vblank interrupt edges, int_* nops/incs and
mbc1_* all report through the same triple and can be added without any
core change.
