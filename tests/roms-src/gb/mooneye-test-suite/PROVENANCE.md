# mooneye-test-suite (GB) — provenance

- Upstream: https://github.com/Gekkio/mooneye-test-suite, pinned at
  `8d742b9d55055f6878a2f3017e0ccf2234cd692c` (MIT).
- Built with `make clean all` (the suite's own Makefile), `WLA`/`WLALINK`
  defaulting to `wla-gb`/`wlalink` on `PATH`, both from the toolchain
  image's `wla-dx` stage (`vhelin/wla-dx@89a90a56...`, the commit Mooneye's
  own CI pins — see `tools/rom-toolchain/Dockerfile`).
- `rom_variant: upstream` (no Veloce patch applied yet — GB-5 lands the
  `VT_*` shell patch to `common/lib/quit.s`/`macros.s` per the plan,
  section 6.3).

## Golden gate: build.py's internal gate has nothing to compare against

Mooneye's `build/` is the Makefile's own output directory, not tracked in
the repo (verified: `git ls-files build/` at the pinned SHA is empty), so
`build.py`'s shipped-then-delete-then-rebuild gate finds no shipped file at
any of the 115 artifact paths and skips it for all of them (`golden_gate:
false` in `recipe.json`, `null` per-file in `rom_manifest.json`).

## Golden gate against the c-sp v7.0 bundle instead

Compared the 115 ROMs `build.py` produced (`build/roms/gb/mooneye-test-suite/`)
against the same 115 relative paths already vendored locally at
`cores/gb/tests/roms/mooneye-test-suite/` (the c-sp/gameboy-test-roms v7.0
release bundle, gitignored, downloaded before this item):

```
match=104 mismatch=11
```

The 11 mismatches, all the same file size (32768 bytes) as their bundle
counterpart, differing by 4–18 bytes each (`cmp -l`):

```
acceptance/add_sp_e_timing.gb          acceptance/boot_regs-sgb2.gb
acceptance/boot_regs-dmg0.gb           acceptance/oam_dma_start.gb
acceptance/boot_regs-dmgABC.gb         acceptance/ppu/vblank_stat_intr-GS.gb
acceptance/boot_regs-mgb.gb            misc/boot_regs-A.gb
acceptance/boot_regs-sgb.gb            misc/boot_regs-cgb.gb
                                        misc/ppu/vblank_stat_intr-C.gb
```

**Root cause: WLA-DX version, not source content.** c-sp's own v7.0 release
script (`src/assemble.sh`, `build_mooneye_test_suite()`, fetched from the
`gameboy-test-roms` repo at tag `v7.0`) builds this exact suite at this
exact SHA with `vhelin/wla-dx@d8b51a99ff1d8c9cb64ba4db90191718508f9c98`.
Veloce's toolchain instead pins `@89a90a56be5c2b8cf19a9afa3e1b32384ddb1a97`
(the plan's choice, section 5: "Mooneye's pinned CI commit"). Both are
real, current WLA-DX commits; same source, same macros, a handful of files
whose exact byte layout depends on assembler-internal choices (e.g. operand
encoding for an otherwise-equivalent instruction, or table/constant
placement) differ between the two builds. This is a toolchain-version
artifact, not a suite or include problem — no source or macro in Mooneye
needed a compatibility shim, unlike SameSuite (see
`tests/roms-src/gb/same-suite/PROVENANCE.md`). Not chased further byte by
byte; flagging it here is what this item asked for.

**Not a regression risk for scoring**: none of these 11 are behavioural
changes — they're the same test, same expected outcome. When GB-5 patches
the shell to emit `VT_*` lines, the rebuilt ROM becomes `rom_variant:
veloce` (Tier A) and is judged by its own emitted `CHECK`/`END` lines, not
by byte-matching either WLA-DX build.

## Tool versions used for this build

Recorded per-ROM in `rom_manifest.json` (`tool_versions.wla-dx`); image id
also recorded (`toolchain_image_id`).
