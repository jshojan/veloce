# Provenance: nes/dmc_dma_during_read4

Upstream: `christopherpow/nes-test-roms`, pinned at `95d8f621ae55cee0d09b91519a8989ae0e64753b`
(sub-tree `dmc_dma_during_read4/`). Test code by Shay Green (blargg). `source/readme.txt`
documents the framework but has no licence statement, and there is no `LICENSE`
file. The plan (section 8) treats blargg's NES test ROMs as freely usable. Only our patch is committed
here; the sources are fetched at the pinned SHA and the ROMs are built by
`tests/roms-src/build.py nes/dmc_dma_during_read4` (never committed).

## Why a rebuild (NES-3)

The test config listed these five ROMs as `memory` tests, but upstream's shipped
binaries are built on the suite's own older shell (`source/common/`: PPU console +
`serial.s` bit-bang on `$4016`). They never write the `$6000` status block or the
`DE B0 61` signature, so no verdict could ever be read from them: every run ended as
`RUNS` ("no result signal") in the baseline scorecard.

## What the build does

1. **Shell swap.** The recipe stages `vcommon/` = `instr_test-v5/source/common/*`
   (blargg's modern shell: `shell.inc` -> `build_rom.s`/`shell.s`, `text_out.s`,
   `testing.s`, `crc.s`) plus this suite's own `common/sync_dmc.s` and
   `common/common.inc`, and links with `instr_test-v5/source/nes.cfg`.
   The suite's `sync_dmc.s` is kept on purpose. It is not the same file as
   `apu_test/source/common/sync_dmc.s`, which the inventory suggested copying: the
   suite's copy syncs at DMC rate 0 (3424-clock period) and provides
   `time_code_begin`, and the test constants (`time = 14`,
   `delay 3309+33+time-iter`) are calibrated against it.
   The unmodified sources assemble against the modern shell with no missing symbols.
   The test bodies use only `shell.inc`, `print_*`, `for_loop`, `delay`,
   `check_crc` and `tests_passed`, all of which the modern shell has.
2. **`patches/0001-veloce-result-channel.patch`** (applied from the clone root):
   - defines `CUSTOM_EXIT = 1` before `shell.inc` and includes
     `tests/roms-src/include/veloce-blargg.inc` after it. This is done in
     `common/common.inc` for dma_2007_read, dma_2007_write and dma_4016_read, and
     directly in double_2007_read and read_write_2007;
   - makes `VB_BEGIN "dmc_dma_during_read4.<rom>"` the first statement of `main`;
   - replaces the final `check_crc X` / `jmp tests_passed` (or the bare
     `jsr print_crc` / `rts`) with `VB_CRC #1,"<name>",<expected CRC(s)>`, followed by `rts`.

   The shell's `exit` then writes `END`, and the shell carries on as before
   (`$6000` status, on-screen text, beeps).

## Expected values: where every number comes from

All CRCs are blargg's running CRC-32 over the bytes the test prints: the same value
`print_crc` shows and the same value `check_crc` compares. The old and modern shells
compute it differently. The old shell starts at `$FFFFFFFF` and prints the complement.
The modern shell keeps the complemented state and needs no inversion. Both equal
`zlib.crc32` of the printed bytes, and we checked this by simulating both 6502
routines on 2000 random inputs. So the old shell's documented values carry over
unchanged.

| ROM | `CHECK` | Accepted CRC(s) | Source |
|---|---|---|---|
| dma_2007_read | `dma_during_2007_read_output` | `159A7A8F`, `5E3DF9C4` | Source header: "11 22 / 11 22 / 33 44 **or** 44 55 / 11 22 / 11 22 / 159A7A8F **or** 5E3DF9C4". The CPU-PPU alignment at reset decides which one appears. Both are consistent with the listed output: we recomputed each from the printed rows and they match. Upstream only printed the CRC and never checked it, so the patch adds the check. |
| dma_2007_write | `dma_during_2007_write_output` | `28F53CA4` | Upstream's own `check_crc $28F53CA4`. The source header's output comment ("22 11 22 AA ...") is stale: it hashes to `346D1277`. The code's `check_crc` value is `11 11 AA 33 44 55 66 77` × 5, which is what the code produces (buffer byte, then VRAM with `$AA` written at `$0001`). The `check_crc` value is the one used. |
| dma_4016_read | `dma_during_4016_read_output` | `F0AB808C` | Upstream's own `check_crc`. It equals the header's hardware output `08 08 07 08 08`: exactly one of the five DMA timings clocks the controller an extra time. |
| double_2007_read | `double_2007_read_output` | `85CFD627`, `F018C287`, `440EF923`, `E52F41A5` | Source header: "Output (depends on CPU-PPU synchronization)". Four second-row variants are listed, and each CRC matches its variant. Upstream only printed the CRC, so the patch adds the check. |
| read_write_2007 | `read_write_2007_output` | `0F877C4B` | Upstream's own `check_crc`. It equals the header's `33 11 22 33 09 55 66 77` × 2. |

A failing `CHECK` carries `exp=<first accepted CRC> got=<CRC the run produced>`.

## Golden gate: behavioural-only (not byte-for-byte)

`golden_gate: false`. The rebuild deliberately uses a different shell from the one the
shipped binaries were built with, so it cannot be byte-identical by construction. For
the record, we also built the unmodified sources against their own `common/` with the
same toolchain (`ca65 --feature force_range`). The result is the same size as
upstream's shipped ROMs (32784 bytes) but differs in 1242-1250 bytes per ROM, first
at file offset 0x6072-0x6472. This is the `force_range` / longbranch situation
`tests/roms-src/nes/cpu_dummy_reads/PROVENANCE.md` describes.

Verified behaviourally instead:

- **Reproducible.** `build.py nes/dmc_dma_during_read4 --check` rebuilds all five ROMs
  byte-identical to the sha256s in `rom_manifest.json`.
- **The patch does not move timed code.** We built the unpatched and patched sources
  on the modern shell with `ca65 -g`, then diffed them. Every byte before `main` is
  identical, except the high byte of `jsr`/`jmp` operands into the shell's `CODE2`
  segment. That segment is `$100`-aligned and starts one page later because `main`
  grew, so the shell routines keep their in-page offsets, and every branch keeps its
  page-crossing behaviour and cycle count. `veloce-blargg.inc` puts all of its code in
  `CODE2`, after the shell's routines, for this reason.
- **Same observations as the unpatched build.** On this emulator the unpatched
  modern-shell build prints CRCs `498C5C5F`, `FBF7C7B1` and `D84F6815`. The patched
  build reports exactly these as `got=` for dma_2007_read, dma_4016_read and
  double_2007_read.
