# NES Emulator Core

Cycle-accurate NES/Famicom emulator for Veloce with dot-by-dot PPU rendering.

## Overview

The NES core provides high-accuracy NES emulation with support for 20+ mappers covering approximately 90% of the NES library. The implementation features dot-by-dot PPU rendering, cycle-accurate CPU timing, and netplay support via the INetplayCapable interface.

## Specifications

| Component | Details |
|-----------|---------|
| CPU | Ricoh 2A03 (6502 variant), all 56 official opcodes |
| PPU | 2C02, dot-by-dot rendering |
| APU | 2 pulse, triangle, noise, DMC channels |
| Resolution | 256 x 240 |
| Frame Rate | 60.0988 Hz (NTSC) |
| Clock Speed | 1.789773 MHz (CPU) |

## Completion Status

### CPU (Fully Implemented)

| Feature | Status | Notes |
|---------|--------|-------|
| Official opcodes (56) | Complete | All addressing modes implemented |
| Unofficial opcodes | Complete | LAX, SAX, DCP, ISB, SLO, RLA, SRE, RRA, ANC, ALR, ARR, SBX |
| Cycle timing | Complete | Per-instruction cycle accuracy with page crossing penalties |
| NMI handling | Complete | Proper timing including delayed NMI |
| IRQ handling | Complete | Both edge-triggered and level-triggered support |
| BRK instruction | Complete | Correct B flag behavior |
| RMW dummy writes | Complete | Read-modify-write instructions perform dummy write |

### PPU (Fully Implemented)

| Feature | Status | Notes |
|---------|--------|-------|
| Dot-by-dot rendering | Complete | Cycle-accurate pixel output |
| Background rendering | Complete | Scrolling, attribute tables, shifters |
| Sprite rendering | Complete | 8 sprites per scanline, priority handling |
| Sprite 0 hit | Complete | Cycle-accurate hit detection |
| Sprite overflow | Complete | Proper overflow flag with quirky behavior |
| VBlank timing | Complete | NMI at scanline 241, cycle 1 |
| Open bus | Partial | Basic implementation, decay not fully emulated |
| NTSC/PAL regions | Complete | Configurable scanline counts |
| Vs. System palettes | Complete | RP2C03, RP2C04 variants |

### APU (Fully Implemented)

| Feature | Status | Notes |
|---------|--------|-------|
| Pulse channels (2) | Complete | Duty cycle, sweep unit, envelope |
| Triangle channel | Complete | Linear counter, frequency timer |
| Noise channel | Complete | Mode flag, period lookup |
| DMC channel | Complete | Sample playback, DMA, IRQ |
| Frame counter | Complete | 4-step and 5-step modes |
| Length counters | Complete | Halt flag, table lookup |
| Envelope units | Complete | Decay, looping |
| IRQ generation | Complete | Frame and DMC IRQs |
| Region timing | Complete | NTSC/PAL/Dendy rates |
| Audio filtering | Complete | High-pass/low-pass, anti-aliasing |

### Known Limitations

- PPU open bus decay timing is not fully accurate (very few games affected)
- Some edge cases in MMC3 A12 clocking may not match hardware perfectly
- Vs. System coin insertion not implemented

## Mapper Support

### Core Mappers

| Mapper | Name | Games | Status |
|--------|------|-------|--------|
| 0 | NROM | Super Mario Bros., Donkey Kong | Complete |
| 1 | MMC1 | Zelda, Metroid, Final Fantasy | Complete |
| 2 | UxROM | Mega Man, Castlevania, Contra | Complete |
| 3 | CNROM | Arkanoid, Gradius | Complete |
| 4 | MMC3 | SMB3, Kirby, Mega Man 3-6 | Complete |
| 7 | AxROM | Battletoads, Marble Madness | Complete |

### Extended Mappers

| Mapper | Name | Notable Games | Status |
|--------|------|---------------|--------|
| 9 | MMC2 | Punch-Out!! | Complete |
| 10 | MMC4 | Fire Emblem (Japan) | Complete |
| 11 | Color Dreams | Bible Adventures | Complete |
| 24 | VRC6 | Castlevania III (Japan) | Complete |
| 34 | BNROM | Deadly Towers | Complete |
| 66 | GxROM | SMB + Duck Hunt | Complete |
| 69 | FME-7 | Gimmick!, Batman: RotJ | Complete |
| 71 | Camerica | Micro Machines | Complete |
| 79 | NINA | Various AVE games | Complete |
| 206 | DxROM | Namco games | Complete |

### Expansion Audio Support

| Chip | Mapper | Channels | Status |
|------|--------|----------|--------|
| VRC6 | 24 | 2 pulse + 1 sawtooth | Complete |
| Sunsoft 5B | 69 | 3 square wave | Complete |

## Building

### As Part of Veloce

The NES core is built automatically with the main Veloce project:

```bash
cmake -B build
cmake --build build
```

### Standalone Build

```bash
cd cores/nes
cmake -B build
cmake --build build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| CMAKE_BUILD_TYPE | Release | Debug, Release, RelWithDebInfo, MinSizeRel |

### Output

- Linux: `build/bin/cores/nes.so`
- Windows: `build/bin/cores/nes.dll`
- macOS: `build/bin/cores/nes.dylib`

### Dependencies

- C++17 compatible compiler
- CMake 3.16+
- No external dependencies (header-only integration with Veloce)

## Architecture

### Components

- **CPU** (cpu.cpp/hpp) - 6502 interpreter with cycle-accurate timing
- **PPU** (ppu.cpp/hpp) - Dot-by-dot rendering, sprite evaluation
- **APU** (apu.cpp/hpp) - Audio synthesis with expansion audio support
- **Bus** (bus.cpp/hpp) - Memory mapping and component interconnection
- **Cartridge** (cartridge.cpp/hpp) - iNES/NES 2.0 loading, mapper dispatch

### Memory Map

| Address | Size | Description |
|---------|------|-------------|
| $0000-$07FF | 2KB | Internal RAM |
| $2000-$2007 | 8 | PPU registers |
| $4000-$4017 | 24 | APU and I/O |
| $4020-$FFFF | ~48KB | Cartridge space |

## Netplay Support

The NES core implements INetplayCapable for rollback netplay:

- is_deterministic() - Returns true (fully deterministic)
- run_frame_netplay(p1, p2) - Two-player frame execution
- save_state_fast/load_state_fast - Optimized for rollback
- get_state_hash() - FNV-1a hash for desync detection

## Source Files

| File | Purpose |
|------|---------|
| src/nes_plugin.cpp | Plugin interface implementation |
| src/cpu.cpp/hpp | 6502 CPU emulation |
| src/ppu.cpp/hpp | PPU emulation |
| src/apu.cpp/hpp | APU emulation |
| src/bus.cpp/hpp | Memory bus |
| src/cartridge.cpp/hpp | ROM loading and mapper dispatch |
| src/mappers/ | Mapper implementations |

## Testing

The NES core ships an authoritative, subsystem-organized accuracy suite in
`tests/test_config.json` (schema v2), driven by the shared `veloce_testkit`
harness so detection and scoring are identical across all consoles. For the
methodology and the platform-wide picture see the top-level
[TESTING.md](../../TESTING.md) and [COMPLETENESS.md](../../COMPLETENESS.md).

### Verified accuracy (evidence-based)

Headline accuracy is **approximately 90-94%**, dominated by CPU and PPU. This is
the most thoroughly verified Veloce core because most critical tests use the
Blargg `$6000` `memory` protocol, which the core's `blargg6000` adapter
streams into the `VELOCE_TEST_OUT` result file (no reference hashes needed). Measured 2026-06-02 against
`build/bin/veloce`.

| Subsystem | Verified score | Notes |
|-----------|----------------|-------|
| CPU | ~96% | All 16 `instr_test-v5` official singles + `official_only` PASS; lone scored fail `exec_space_apu`; `03-immediate` known_fail (unstable opcodes) |
| PPU | ~92% | `ppu_vbl_nmi` 9/10 PASS; OAM/open-bus PASS; sprite-hit/overflow/2005 palette suites visual and unverified |
| Mapper | ~88% | MMC3 core verified; MMC6 + `4-scanline_timing` known_fail; MMC1/MMC5 visual/unverified |
| APU | ~70% | Functional/length/IRQ gates pass; cycle-accurate frame-counter cluster (`apu_test` 4/5/6) fails |
| Timing | ~40% (weakest) | `cpu_interrupts_v2` 2/3/4 fail; DMA collisions expected-fail |

Full per-test justification, the verified-versus-unverified breakdown, and the
honest coverage caveats (nestest trace, screenshot-crc reference hashes, reset
injection, mapper breadth, PAL) are documented in
[COMPLETENESS.md](../../COMPLETENESS.md#nes). The headline reflects the verified
`memory`-detected subset; the 62 `screenshot-crc` tests and nestest contribute
zero until reference hashes or a `TRACE=1` emitter exist.

### Running

```bash
# from cores/nes/tests/ (requires a built build/bin/veloce and python3)
./run_tests.sh                 # all suites, human scorecard
./run_tests.sh cpu ppu apu     # filter by subsystem key or suite id
./run_tests.sh -v              # per-test PASS/FAIL/KNOWN/RUNS lines
./run_tests.sh --json          # scorecard JSON (consumed by tests/run_all.py)
./run_tests.sh --generate-refs # emit measured CRCs for screenshot-crc tests
```

`run_tests.sh` is a thin shim over `runner.py`, which calls
`veloce_testkit.runner.run_console_main("nes", ...)`. The ROM provider clones
`christopherpow/nes-test-roms` on first run into `tests/nes-test-roms/` and
resolves test paths through `tests/_roms/` (gitignored). That directory holds one
symlink per top-level entry of the checkout, plus `veloce/` -> `build/roms/nes/`
for the `rom_variant: veloce` ROMs that `tests/roms-src/build.py` builds (see
below). The link lives there, not inside `nes-test-roms/`, because worktrees
often share that one clone. A veloce test whose ROM is not built yet reports
SKIP ("ROM not found").

### Coverage (172 tests across 32 suites)

| Subsystem | Suites |
|-----------|--------|
| CPU | instr_test-v5 (official), illegal opcodes (all_instrs, nes_instr_test), blargg_nes_cpu_test5, nestest golden trace, instr/branch timing, dummy reads, dummy writes (RMW), exec-from-I/O space, misc edge cases, reset |
| PPU | ppu_vbl_nmi, vbl_nmi_timing, sprite-0 hit, sprite overflow, OAM (read/stress), open-bus + read-buffer, blargg_ppu_2005 (palette/VRAM/power-up), full_palette/scanline render |
| Timing | cpu_interrupts_v2 (NMI/IRQ/BRK hijacking), DMA collisions (sprdma_and_dmc_dma, dmc_dma_during_read4) |
| APU | blargg_apu_2005, apu_test, apu_mixer, apu_reset, DMC, PAL framing |
| Mapper | MMC3 (mmc3_test, mmc3_test_2, mmc3_irq_tests), MMC1 A12, MMC5 (mmc5test, exram) |
| Misc | controllers (read_joy3) |

### Result detection

- **result file** (`VELOCE_TEST_OUT`, format in
  [docs/testing/VELOCE-RESULT.md](../../docs/testing/VELOCE-RESULT.md)): the
  core's `nes::TestChannel` (`src/test_channel.hpp`) feeds two drivers into the
  file. The **port** takes `$401E` data bytes, `$401F` control writes and
  answers a `$401F` probe read with `$56`, for ROMs rebuilt with the Veloce
  emitter. The **`blargg6000` adapter** serves unmodified Blargg ROMs: it
  follows the ROM's writes to the `$6000` status byte, the `DE B0 61`
  signature and the `$6004` text, streams the text as `LOG` lines, and turns a
  final status into `END 1/1` (status `0`) or `END 0/1 code=N`. Status `$81`
  ("press reset") makes the application reset the console, which runs the
  `apu_reset`/`cpu_reset` ROMs. The deprecated `memory` detection alias reads
  the same file. Nothing is printed to stdout/stderr, and `DEBUG` plays no
  part. While a sink is attached, CPU RAM and PRG-RAM start from fixed
  contents (`$00`), so a stale battery save cannot change a run.
- **screenshot-crc** (visual): older 2005-era Blargg suites
  (`sprite_hit_tests_2005`, `sprite_overflow_tests`, `vbl_nmi_timing`,
  `blargg_ppu_tests_2005`, `mmc3_irq_tests`, `blargg_nes_cpu_test5`,
  `cpu_dummy_reads`) report only on-screen and **do not** write `$6000`; they are
  classified visual and require reference hashes generated via `--generate-refs`.
- **cpu-trace** (nestest): compares a `TRACE=1` nestest-format instruction stream
  against the golden `nestest.log`. Requires a `TRACE=1` emitter in the binary and
  the golden log (not shipped); currently reported as unverified.

### Rebuilding test ROMs from source (`tests/roms-src/`)

`tests/roms-src/build.py` rebuilds ROMs from pinned upstream commits inside
the shared `tools/rom-toolchain` Docker image and records each ROM's
`sha256` (plus every tool's version) in `tests/roms-src/rom_manifest.json`.
`tests/roms-src/include/veloce-test.inc` (ca65), `veloce-test-asm6f.inc`
(asm6f) and `veloce-test-nesasm.inc` (NESASM3) are the "one include per
assembler" `VT_*` emitter (`VT_BEGIN`/`VT_TEST`/`VT_PASS`/`VT_FAIL`/`VT_REG`/
`VT_LOG`/`VT_END`, all `STA $401E` underneath) a patched suite's shell calls
into to speak `VELOCE-RESULT/1` directly instead of through the
`blargg6000` adapter; `tests/roms-src/nes/veloce_smoke/` is a small
self-authored ROM (`local_source: true`, no upstream to golden-gate
against) that exercises every macro in the ca65 dialect against a fixed
transcript and is used as this channel's determinism check (build twice /
run twice, byte-diff). The asm6f and NESASM3 dialects are hand-verified
against the pinned toolchain (each has a documented, worked-around
assembler quirk in its own file's header comment) but do not yet have their
own smoke ROM; whoever wires the first real asm6f (NES-6/7) or NESASM3
(NES-4, AccuracyCoin) suite through the channel gets to be that dialect's
first real-ROM proof instead of a second synthetic one.

`build.py` also golden-gates (rebuilds unmodified, `cmp`s against upstream's
own shipped binary) 16 of the "modern" ca65 shell suites plus Bisqwit's
`cpu_dummy_reads` (17 suites, 85 ROMs: `instr_test-v5`, `nes_instr_test`,
`instr_timing`, `instr_misc`, `cpu_reset`, `cpu_interrupts_v2`,
`ppu_vbl_nmi`, `oam_read`, `oam_stress`, `ppu_open_bus`, `apu_test`,
`apu_mixer`, `apu_reset`, `mmc3_test`, `mmc3_test_2`, `read_joy3` (only
`test_buttons.nes`; see the correction below), `cpu_dummy_reads`). All 85 rebuild byte-for-byte the same size as upstream
but a few hundred to ~2000 bytes different (`golden_gate: false`, reason
recorded per suite; same unconfirmed `--feature force_range`/`longbranch`
root cause `cpu_dummy_reads/PROVENANCE.md` first wrote up), verified
behaviourally instead: two independent rebuilds of each are byte-identical
to each other. Three suites are documented, not built: the seven "combined"
ROMs (`official_only.nes`, `all_instrs.nes`, `instr_timing.nes`,
`instr_misc.nes`, `cpu_interrupts.nes`, `ppu_vbl_nmi.nes`, `apu_test.nes`)
can't be rebuilt from the shipped source tree at all (the suites' own
`readme.txt` says the multi-test build tooling "isn't included");
`ppu_read_buffer` needs a PHP/nasm/ImageMagick asset pipeline the toolchain
image doesn't have; `sprdma_and_dmc_dma` ships no source whatsoever in
`nes-test-roms`. None of the 88 real `$6000`-writing "blargg-modern/bisqwit"
ROMs this covers were patched -- the `channel`/`rom_variant: veloce` config
flip and the shell patch that emits `VELOCE-RESULT/1` `TEST`/`CHECK` lines
directly are NES-5's job; this only proves the rebuild pipeline honest
before that patching starts. (Correction, NES-3: `read_joy3/test_buttons.nes`
is not a `$6000` writer either. Like the rest of its suite, it uses an older
console+serial shell, so the count of unpatched real writers is 87. It is now
built as a patched ROM with the other three `read_joy3` ROMs, as described below.)

#### ROMs rebuilt to report over the port (NES-3)

Eleven config entries were labelled `memory`, but their shipped binaries never
write `$6000`, so they always ended as `RUNS`. Together with
`read_joy3/test_buttons`, they are now built as `rom_variant: veloce` ROMs that
write `VELOCE-RESULT/1` over `$401E`. The config addresses them as
`veloce/<suite>/<rom>.nes` with `channel`/`require_channel: port`. Each suite's
`PROVENANCE.md` says where every expected value comes from.

- `dmc_dma_during_read4` (5) and `read_joy3` (4): the ca65 sources are rebuilt on
  blargg's modern shell (`instr_test-v5/source/common`). Each suite keeps its own
  DMC-sync code, because its timing constants are calibrated against it. A small
  patch per suite reports through
  `tests/roms-src/include/veloce-blargg.inc`, which provides:
  - `VB_BEGIN`;
  - `VB_CRC`: the shell's CRC-32 against one or more documented hardware results;
  - `VB_FLAG` and `VB_REG`;
  - the shell's `exit`, which writes `END`.

  The patch keeps every cycle-timed routine at its original in-page address.
- `pal_apu_tests` 01-03: the 2005-era sources are translated to asm6f
  (`include/runtime2005/to_asm6f.sed`) and linked against `runtime2005`, a
  replacement for blargg's `validation.a` and ROM wrapper that reports the `$F8`
  result code as one `CHECK`. Tests 01-03 exercise behaviour the NTSC and PAL APUs
  share, so they are scored. 04-11 stay unwired until a PAL mode exists.

Results on this core (2026-09-27):

| ROM | Result | Why |
|---|---|---|
| pal_apu_tests 01/02/03 | PASS | |
| dma_2007_write, read_write_2007 | PASS | |
| read_joy3 test_buttons | PASS | uses a deterministic `INPUT` schedule |
| dma_2007_read | FAIL (known_fail) | got `498c5c5f`, no extra `$2007` reads |
| dma_4016_read | FAIL (known_fail) | got `fbf7c7b1` = `08 08 08 08 08`, no extra `$4016` clock |
| double_2007_read | FAIL (known_fail) | a fifth pattern, `d84f6815` |
| count_errors | 1/2 (known_fail) | `read_joy` stays correct, but 0 DMC conflicts are seen |
| count_errors_fast | FAIL (known_fail) | 0 corrupted reads |
| thorough_test | RUNS | the suite's `sync_dmc` never returns at about iteration 330 of 1000; the unmodified build behaves the same |

All of these failures point at one gap: the core's DMC DMA does not repeat the
CPU read that was in flight.

### Known measurement gaps

The nestest golden trace (needs a `TRACE=1` emitter plus the golden
`nestest.log`), screenshot-crc reference hashes, PAL framing, unstable
illegal immediates, and narrow mapper breadth are all documented with their
impact in [COMPLETENESS.md](../../COMPLETENESS.md#nes). Reset-injection tests
(`apu_reset`, `cpu_reset`) now run through the `blargg6000` adapter's `$81`
handshake. Optional mapper PCB stress ROMs
from `pinobatch/holy-mapperel` are referenced in the config but not auto-fetched;
build them from that GPL repo and drop the per-mapper `.nes` files in to extend
mapper coverage.
