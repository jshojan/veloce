# gbmicrotest (GB) — provenance

- Upstream: https://github.com/aappleby/GBMicrotest, pinned at
  `f3b55497c1d1202c784b8201dafc888c838b7302` (MIT).
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
