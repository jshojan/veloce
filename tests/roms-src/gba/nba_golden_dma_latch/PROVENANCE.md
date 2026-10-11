# Provenance: gba/nba_golden_dma_latch

Upstream: [`nba-emu/hw-test`](https://github.com/nba-emu/hw-test), pinned at
`3932a0e54dbb5d94b84798d2d5e462e080b95599` (BSD-3-Clause, copyright 2021
fleroviux). GitHub still serves this exact commit (verified with `git
ls-remote`) even though the plan's protocol notes the project archived on
GitHub and moved to Codeberg (`https://codeberg.org/nba-emu/hw-test.git`,
same commit) -- either remote clones the same tree.
`cores/gba/tests/nba-hw-test` is the same repository at the same commit
(the test harness's `repositories.nba-hw-test` in
`cores/gba/tests/test_config.json`); this recipe clones it again
independently under `upstream/`, same reasoning as
`gba/jsmolka_golden_arm/PROVENANCE.md`.

## What this proves (GBA-2 scope)

The devkitARM half of GBA-2's golden gate. `dma/latch` is one of nba's 12
hw-test ROMs, built by devkitPro's standard `gba_rules` + `libgba`
(`-lgba`), sourced from `dma/latch/source/main.c` plus the shared
`lib/source/{test.c,ui.c}` every nba-hw-test ROM links. `LIBGBA` isn't set
by `tools/rom-toolchain`'s image `ENV` (only `DEVKITARM`/`DEVKITPRO` are),
so this recipe's build command exports it from `$DEVKITPRO/libgba` first --
GBA-3, which rebuilds the other 11 ROMs, will need the same export.

Verified directly (not just via `build.py --check`):

```
$ sha256sum dma/latch/latch.gba                            # upstream's shipped binary
4e3a6510d30a67afe1b0e8bc8de4676bbbabeef5c0f0f3e960ff25c4c082373b
$ export LIBGBA=$DEVKITPRO/libgba && make                   # rebuilt inside veloce/rom-toolchain:dev
$ sha256sum dma/latch/latch.gba                             # identical
4e3a6510d30a67afe1b0e8bc8de4676bbbabeef5c0f0f3e960ff25c4c082373b
```

`build.py gba/nba_golden_dma_latch` reproduces the same result
(`golden_gate: {"match": true}` in `rom_manifest.json`).

## Recommendation for whoever picks up GBA-3

The other 11 ROMs share the same `lib/source/{test.c,ui.c}` reporting
helper (`test_expect_hex`/`test_expect_str` printing through libgba's
on-screen console + `test_print_metrics()`) that GBA-3 needs to redirect
into `mgba_console_open()` (per `docs/testing/testrom-protocol.md`'s note on
`nba-emu/hw-test`) rather than patch per-ROM; this recipe deliberately
didn't touch that shared file so GBA-3's golden gate for the other 11 has
the same clean starting point this one had. `haltcnt`, `bus/*`, `irq/*`,
`timer/*`, and `ppu/*` each have their own `Makefile` following the same
shape as `dma/latch`'s (all `include $(DEVKITARM)/gba_rules`,
`SOURCES := source ../../lib/source`), so the same `LIBGBA` export and
build shape should carry over directly.
