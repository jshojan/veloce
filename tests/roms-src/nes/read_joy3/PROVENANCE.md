# Provenance: nes/read_joy3

Upstream: `christopherpow/nes-test-roms`, pinned at `95d8f621ae55cee0d09b91519a8989ae0e64753b`
(sub-tree `read_joy3/`). Test code by Shay Green (blargg). `source/readme.txt`
documents the framework but has no licence statement, and there is no `LICENSE`
file. The plan (section 8) treats blargg's NES test ROMs as freely usable. Only our patch is committed
here; the sources are fetched at the pinned SHA and the ROMs are built by
`tests/roms-src/build.py nes/read_joy3` (never committed).

## Why a rebuild (NES-3)

None of the four shipped binaries writes the `$6000` status block or the `DE B0 61`
signature. All four are built on the suite's own older shell (`source/common/`: PPU
console plus `serial.s` bit-bang on `$4016`).

This corrects NES-2's recipe, which described `test_buttons.nes` as a modern-shell
`$6000` writer. It is not: its shipped binary, and NES-2's rebuild against
`source/common/`, contain no `$6001`/`$6004` store and no `DE B0 61` constant. The
test config had already marked it `screenshot-crc`. The other three were mislabelled
`memory` tests that could only end as `RUNS` ("no result signal").

## What the build does

1. **Shell swap.** The recipe stages `vcommon/` = `instr_test-v5/source/common/*`
   (blargg's modern shell) plus this suite's own `common/sync_dmc.s`, and links with
   `instr_test-v5/source/nes.cfg`. thorough_test's delays are calibrated against the
   suite's `sync_dmc` (DMC rate 0, 3424-clock period), so it is kept rather than
   swapped for `apu_test`'s. `read_joy.inc` is the suite's own file and is used
   unchanged. The unmodified sources assemble against the modern shell with no
   missing symbols.
2. **`patches/0001-veloce-result-channel.patch`** makes each ROM report over the
   `$401E` port through `tests/roms-src/include/veloce-blargg.inc`. It sets
   `CUSTOM_EXIT = 1` before `shell.inc`, includes the file after it, and adds
   `VB_BEGIN "read_joy3.<rom>"` as the first statement of `main`. The per-ROM checks
   are below.

## Expected values: where every verdict comes from

| ROM | `CHECK` | Pass condition | Source |
|---|---|---|---|
| test_buttons | 1 `each_button_read_alone_in_order` | blargg exit code 0 | Upstream's own verdict. `read_joy` must return exactly the requested button, for A, B, Select, Start, Up, Down, Left and Right in turn (`jne test_failed`), while DMC plays. The config drives the presses with a deterministic `INPUT` schedule (`30:1,40:0,60:2,...,240:800,250:0`: one button every 30 frames, released after 10). A negative control (B pressed first) produces `CHECK 1 FAIL ... got=01`. |
| thorough_test | 1 `read_joy_reliable_at_every_dmc_timing` | blargg exit code 0 | Upstream's own verdict. It syncs to the DMC, then runs `read_joy` with the DMA at every one of 1000 timings. No more than one of the four internal reads may be corrupted, and the result must be 0. No buttons are held, so the test needs no input. |
| count_errors | 1 `read_joy_result_correct` | blargg exit code 0 | Upstream's own verdict: `read_joy` returned 0 (no buttons) in every one of 1000 reads with DMC playing (`jne test_failed`). |
| count_errors | 2 `dmc_dma_conflict_seen` | conflicts > 0 | The ROM's stated purpose (source header: "prints X when the DMC conflicted with one of the internal reads"). On an NTSC 2A03, a DMC DMA landing on a `$4016` read clocks the controller an extra time. blargg's own `check_crc`'d `dmc_dma_during_read4/dma_4016_read` shows this ("08 08 07 08 08"), and the nesdev wiki (APU DMC, conflicts with controller and PPU reads) documents it. With the DMC at rate 15 during 1000 reads (about 660 clocks each, a DMA every 432 clocks), a correct 2A03 cannot avoid every read cycle. This check is Veloce's addition: upstream only prints the count. |
| count_errors_fast | 1 `dmc_dma_corrupted_read_seen` | errors > 0 | Same source and reasoning. `read_joy_fast` makes no correction, so an extra `$4016` clock shifts in the controller's post-8-bit 1s and gives a non-zero read. Upstream only prints the count; it has no pass/fail of its own. |

Both counts are also emitted as `REG` lines (`conflicts_hi`/`conflicts_lo` and
`errors_hi`/`errors_lo`), so a regression report shows the measured number. Upstream
counts in one byte, which wraps at 256. The patch adds a high byte (`inc x_hi` on the
wrap) so that "> 0" cannot be fooled by exactly 256 conflicts.

## Golden gate: behavioural-only (not byte-for-byte)

`golden_gate: false`. The rebuild deliberately uses a different shell from the one the
shipped binaries were built with. For the record, the unmodified sources built against
their own `common/` with the same toolchain (`ca65 --feature force_range`) are the same
size as upstream's shipped ROMs (40976 bytes) but differ in 72-99 bytes, first at file
offset 0x6084-0x6370. This is the `force_range` / longbranch situation
`tests/roms-src/nes/cpu_dummy_reads/PROVENANCE.md` describes.

Verified behaviourally instead:

- **Reproducible.** `build.py nes/read_joy3 --check` rebuilds all four ROMs
  byte-identical to the sha256s in `rom_manifest.json`.
- **The patch does not move timed code.** We compared unpatched and patched
  modern-shell builds (`ca65 -g`). Before `main`, only the high bytes of
  `jsr`/`jmp` operands into the page-aligned `CODE2` shell segment change. This
  includes `read_joy.inc`'s `.align 32` routines and the suite's `sync_dmc`.
- **thorough_test hangs the same way either way.** On this emulator the suite's
  `sync_dmc` never returns at about iteration 330-354 of 1000. We saw this with
  markers written to `$401E` before and after the call, both in the patched
  modern-shell build and in an unmodified old-shell build. The hang comes from the
  core's DMC timing, not from the shell swap. blargg documents 17 ms as the maximum
  time for `sync_dmc`.
