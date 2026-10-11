# Provenance: snes/sour_dma_irq_test

Upstream: the higan/byuu `snes-test-roms` GitLab mirror
(`https://gitlab.com/higan/snes-test-roms.git`), pinned at
`26e8aa91e0d3b90b901e7e693bc4a8244b391d7b` (the plan's SNES-2 acquisition
pin; this SHA is that mirror's current `master` HEAD). Sub-tree
`Sour/SnesTests/dma_irq_test/`, which the mirror's own `README.md` says is a
`git subtree` mirror of `github.com/SourMesen/SnesTests`, expected results
recorded from that repo's revision `71c50e3`.

No LICENSE file anywhere in `SourMesen/SnesTests` or its `Sour/` mirror
directory. Per the plan (section 8), this is one of the unlicensed
"patch-only" suites: this recipe/PROVENANCE are committed, the golden-gate
rebuild is done in CI from the pinned upstream, and no ROM binary (built or
otherwise) is ever committed (`*.sfc` is gitignored, and `upstream/` itself
is gitignored per `tests/roms-src/*/*/upstream/`).

## Golden gate: byte-identical

`golden_gate: true` in `recipe.json`, and it passes: `tests/roms-src/build.py
snes/sour_dma_irq_test` rebuilds `dma_irq_test.sfc` from the unmodified
source and its sha256 matches the sha256 of the file the same path had right
after checkout (upstream's own shipped binary), byte for byte. Unlike several
NES/SNES blargg-shell sources hit by cc65-version drift (see
`tests/roms-src/nes/cpu_dummy_reads/PROVENANCE.md`), this suite's own
`global.inc`/`snes.inc`/`bg.s`/etc. sources build cleanly with the pinned
cc65 commit and no special flags (`ca65 -g <file> -o <file>.o` exactly as
`Sour/SnesTests/dma_irq_test/build.cmd` specifies, translated from the
Windows-path `cc65\ca65 -g src\bg.s` form only by using forward slashes).

## Expected values

Unaffected by this recipe: `dma_irq_test` is currently scored via
`screenshot-crc` in `cores/snes/tests/test_config.json` (visual on-screen
result), which this recipe does not touch. Converting it to a
VELOCE-RESULT/1 `CHECK`/`END` emitter via `veloce-test.ca65.inc` (patching
`src/main.s`'s result-reporting path) is SNES-5 (Sour op_timing_test_v2 /
dma_irq_test: "expected tables ... + CHECK per row; keep on-screen table"),
not this item -- this recipe only proves the acquisition-and-golden-gate
half of that dependency chain end to end.

## Tool versions

Recorded in `tests/roms-src/rom_manifest.json` under
`snes/sour_dma_irq_test/dma_irq_test.sfc` (`tool_versions.cc65`, the
toolchain image id, and this recipe's `source_repo`/`source_sha`).
