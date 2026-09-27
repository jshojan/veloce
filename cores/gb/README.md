# Game Boy Emulator Core

M-cycle accurate Game Boy and Game Boy Color emulator for Veloce.

## Overview

The Game Boy core provides high-accuracy emulation for both the original Game Boy (DMG) and Game Boy Color (CGB). It features M-cycle accurate Sharp LR35902 CPU emulation, scanline-accurate PPU timing, and four-channel audio processing, and automatically detects ROM type to configure the appropriate mode. For the current, measured accuracy status (Blargg/Mooneye/Mealybug/SameSuite coverage and known gaps) see the [Testing](#testing) section -- the previous "100%" claim has been retired.

## Specifications

| Component | Details |
|-----------|---------|
| CPU | Sharp LR35902 (SM83 core) |
| Resolution | 160 x 144 |
| Frame Rate | 59.7275 Hz |
| Clock Speed | 4.194304 MHz (DMG), 8.388608 MHz (CGB double-speed) |
| Colors | 4 shades (DMG), 32768 colors (CGB) |

## Completion Status

### CPU (Fully Implemented)

| Feature | Status | Notes |
|---------|--------|-------|
| Main opcodes (256) | Complete | All standard instructions |
| CB-prefix opcodes (256) | Complete | All bit manipulation and rotate/shift |
| Cycle timing | Complete | M-cycle accurate |
| HALT mode | Complete | Proper wake-on-interrupt |
| STOP mode | Complete | Speed switching (CGB) |
| HALT bug | Complete | IME=0 with pending interrupt quirk |
| OAM bug | Complete | DMG OAM corruption quirk |
| Interrupts | Complete | VBlank, LCD STAT, Timer, Serial, Joypad |

### PPU (Fully Implemented)

| Feature | Status | Notes |
|---------|--------|-------|
| Background rendering | Complete | Tile maps, scrolling |
| Window rendering | Complete | WX/WY positioning |
| Sprite rendering | Complete | 8x8 and 8x16 modes, priority |
| OAM DMA | Complete | 160-cycle transfer |
| DMG palettes | Complete | 4-shade grayscale |
| CGB palettes | Complete | 8 BG + 8 OBJ palettes, 32768 colors |
| CGB VRAM banking | Complete | Bank 0/1 switching |
| LCD STAT interrupts | Complete | Mode 0/1/2, LYC=LY |
| Pixel FIFO | Partial | Scanline-based (accurate enough for most games) |

### APU (Fully Implemented)

| Feature | Status | Notes |
|---------|--------|-------|
| Channel 1 (Pulse) | Complete | Sweep, envelope, duty cycle |
| Channel 2 (Pulse) | Complete | Envelope, duty cycle |
| Channel 3 (Wave) | Complete | 16-byte wave RAM |
| Channel 4 (Noise) | Complete | LFSR, envelope |
| Frame sequencer | Complete | 512 Hz, 8 steps |
| Length counters | Complete | All 4 channels |
| Stereo panning | Complete | NR51 register |
| Master volume | Complete | NR50 register |

### CGB Features

| Feature | Status | Notes |
|---------|--------|-------|
| Double-speed mode | Complete | KEY1 register |
| HDMA transfers | Complete | General-purpose and HBlank DMA |
| Color palettes | Complete | BCPS/BCPD, OCPS/OCPD |
| VRAM banking | Complete | VBK register |
| WRAM banking | Complete | SVBK register |

### Known Limitations

- Pixel FIFO is scanline-based rather than dot-by-dot (sufficient for game compatibility)
- Some CGB-only timing edge cases may not be hardware-perfect

## MBC Support

| MBC | Features | Common Games | Status |
|-----|----------|--------------|--------|
| None | 32KB ROM only | Tetris | Complete |
| MBC1 | Up to 2MB ROM, 32KB RAM | Pokemon Red/Blue, Zelda | Complete |
| MBC3 | RTC, up to 2MB ROM | Pokemon Gold/Silver/Crystal | Complete |
| MBC5 | Up to 8MB ROM, 128KB RAM | Pokemon Crystal, GBC games | Complete |

## Building

### As Part of Veloce

The Game Boy core is built automatically with the main Veloce project:

```bash
cmake -B build
cmake --build build
```

### Standalone Build

```bash
cd cores/gb
cmake -B build
cmake --build build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| CMAKE_BUILD_TYPE | Release | Debug, Release, RelWithDebInfo, MinSizeRel |

### Output

- Linux: `build/bin/cores/gb.so`
- Windows: `build/bin/cores/gb.dll`
- macOS: `build/bin/cores/gb.dylib`

### Dependencies

- C++17 compatible compiler
- CMake 3.16+
- No external dependencies (header-only integration with Veloce)

## Testing

For the testing methodology and the platform-wide picture see the top-level
[TESTING.md](../../TESTING.md) and [COMPLETENESS.md](../../COMPLETENESS.md).

**Verified accuracy:** every non-visual test now gets its verdict from the
VELOCE-RESULT/1 file the core writes (see *Result Detection* below). Measured
2026-09-27: 65 PASS, 35 FAIL, 63 known_fail, 2 RUNS, 58 visual/skip (63.9%).
Blargg `cpu_instrs` 11/11, `instr_timing` and `mem_timing` 3/3 PASS; Mooneye
`timer` 13/13, `mbc1` 12/13, `mbc5` 8/8 PASS; Mooneye `timing` (13), `ppu` (11),
`oam_dma` (5) and `interrupts` (5) have real failures. The headline fell from
the earlier "Blargg serial subset" number because those Mooneye failures were
previously unscored (RUNS), not because anything regressed. The previous
"111/111, 100%" claim was not reproducible and has been retired. Full
justification is in [COMPLETENESS.md](../../COMPLETENESS.md#game-boy).

### Test Suite

The Game Boy core uses the shared Veloce testkit (`tests/veloce_testkit`) so its
detection, scoring, and JSON scorecard are identical to the other consoles. The
authoritative test ROM set is the prebuilt
[c-sp gameboy-test-roms v7.0 bundle](https://github.com/c-sp/gameboy-test-roms)
(the upstream Mooneye/Mealybug/SameSuite/acid2 repos ship only RGBDS source, not
runnable `.gb` ROMs). The runner downloads and unpacks it into
`cores/gb/tests/roms/` on first run.

`cores/gb/tests/test_config.json` is schema-v2 and enumerates **225 tests across
31 suites**, organized by hardware subsystem with a per-test `result_detection`,
`accuracy_type`, and `priority`:

| Source | Subsystems exercised |
|--------|----------------------|
| Blargg `cpu_instrs`, `instr_timing`, `mem_timing(-2)`, `halt_bug`, `interrupt_time`, `oam_bug`, `dmg_sound`, `cgb_sound` | CPU instr + instruction/memory timing, HALT bug, OAM bug, APU |
| Mooneye acceptance (`timer`, `ppu`, `interrupts`, `oam_dma`, `timing`, `bits`, `serial`, boot/div) | Cycle-accurate timer/PPU/interrupt/IME/HALT/DMA/control-flow timing |
| Mooneye `emulator-only` (MBC1/MBC2/MBC5) | Mappers / banking |
| Mealybug Tearoom (DMG + CGB) | Mid-scanline (mode-3) PPU register changes, pixel-perfect |
| Wilbertpol | PPU sub-scanline STAT/mode timing |
| SameSuite (APU ch1-4 + DIV/registers) | Cycle-accurate APU |
| dmg-acid2 / cgb-acid2 | Pixel-perfect PPU rendering |

```bash
cd cores/gb/tests
./run_tests.sh                 # run all, human scorecard
./run_tests.sh cpu ppu apu     # filter by subsystem key or suite id
./run_tests.sh --json          # scorecard JSON (for tests/run_all.py)
./run_tests.sh -v              # per-test verdict lines
./run_tests.sh --generate-refs # print measured screenshot CRC32 hashes
```

### Result Detection

Verdicts come only from the result file the application writes when
`VELOCE_TEST_OUT` is set ([VELOCE-RESULT/1](../../docs/testing/VELOCE-RESULT.md));
the core prints nothing to stdout. `gb::TestChannel`
([src/test_channel.hpp](src/test_channel.hpp)) implements these drivers:

| Driver | Tier | Suites | What it writes |
|--------|------|--------|----------------|
| `serial` | A | ROMs built with the Veloce emitter | every byte sent with `SC=$81`, raw; the ROM's own `CHECK`/`END` lines |
| `blarggtext` | B | Blargg `cpu_instrs`, `instr_timing`, `mem_timing` | the ROM's serial text, then `END 1/1` for a `Passed` line or `END 0/1 [code=n]` for `Failed [#n]` |
| `a000` | B | Blargg `oam_bug`, `dmg_sound`, `cgb_sound`, `mem_timing-2` | the `$A000` text as `LOG` lines (when not mirrored to serial), `END` from the status byte, reset request on `$81` |
| `hram` | B | GBMicrotest (cartridge title `microtest`) | `CHECK 1 PASS/FAIL hram exp=<$FF81> got=<$FF80>` + `END` once `$FF82` is `$01`/`$FF` |
| `mooneye` | C | Mooneye, SameSuite, Wilbertpol | `REG b= c= d= e= h= l=` + `END 1/1` (Fibonacci fingerprint at `LD B,B`) or `END 0/1 code=66` (all `$42`) |
| `screenshot-crc` | n/a | Mealybug, acid2, Blargg-sound, halt_bug, interrupt_time, mem_timing-2 | visual only; `reference_hash` values not populated, never scored |

Any adapter stands down once the ROM has sent a `VELOCE 1` header or an `END`
line over serial, and only one terminator is ever written. With a sink attached
the cartridge RAM is zeroed first, so a battery save from an earlier run can
never be read as this run's result.

```bash
VELOCE_TEST_OUT=/tmp/t.result ./build/bin/veloce cores/gb/tests/roms/blargg/cpu_instrs/individual/01-special.gb
cat /tmp/t.result
# #VELOCE 1 core=GBC rom_crc32=fdf8598c channels=serial,mooneye,a000,hram,blarggtext
# 01-special
# ...
# Passed
# #VELOCE adapter=blarggtext
# END 1/1
# #VELOCE end reason=terminator frames=140 cycles=2457919 status=0
```

The measured per-suite results are tabulated with their justification in
[COMPLETENESS.md](../../COMPLETENESS.md#game-boy).

### Rebuilding test ROMs from source

`tests/roms-src/include/gb/veloce-test.inc` (wla-gb dialect) and
`veloce-test-rgbds.inc` (RGBDS dialect) are the `VT_BEGIN`/`VT_TEST`/
`VT_PASS`/`VT_FAIL`/`VT_REG`/`VT_LOG`/`VT_END` emitters future patches use
to make a suite emit `VELOCE-RESULT/1` directly (Tier A) instead of relying
on an adapter. Both are exercised end to end by
`tests/roms-src/gb/veloce_smoke/` (one fixture ROM per dialect, built via
`tests/roms-src/build.py`'s toolchain image; running it twice under
`VELOCE_TEST_OUT` is byte-for-byte deterministic, including the trailer).

Rebuilding the three GB suites that are pure source (Mooneye, SameSuite,
GBMicrotest) against the pinned toolchain and comparing the result to the
c-sp v7.0 bundle this core actually runs against today (`tests/roms-src/gb/
<suite>/PROVENANCE.md` has the full detail per suite):

| Suite | Rebuilds against pinned toolchain | Matches the c-sp v7.0 bundle |
|-------|-----------------------------------|------------------------------|
| GBMicrotest | yes (wla-gb) | 513/513 byte-identical |
| Mooneye | yes (wla-gb) | 104/115 byte-identical; 11 differ by a handful of bytes each, traced to a WLA-DX *version* difference (Mooneye's own CI pin vs. c-sp's release pin), not a source or macro problem |
| SameSuite | **no** | blocked: its vendored `include/hardware.inc` uses pre-RGBDS-0.5 `SET` syntax that neither of Veloce's two pinned RGBDS builds (0.6.1, 0.7.0) accepts; c-sp's own release builds it with RGBDS v0.5.2, which Veloce does not carry |
| Blargg (`cpu_instrs`/`instr_timing`/`mem_timing(-2)`/sound/etc.) | not attempted | ships prebuilt upstream; already scored via the `blarggtext`/`a000` adapters above, no rebuild needed yet |

None of this changes today's scored numbers — GB-1's adapters already read
every suite's verdict from the vendored, unmodified c-sp bundle. It is the
groundwork for GB-4 through GB-7 (source patches that make each suite emit
`CHECK`/`END` directly, promoting it to Tier A).

### Populating visual reference hashes

`screenshot-crc` tests ship with empty `reference_hash` (so they SKIP / are
excluded from the score rather than scoring the emulator against its own output).
To validate them, capture the frame on a known-good reference emulator (or real
hardware), then:

```bash
./run_tests.sh dmg_acid2 cgb_acid2 mealybug_dmg --generate-refs
# paste the printed hashes into reference_hash, then flip expected to "pass"
```

### Measurement path for the full suite

```bash
# build the GB plugin (cmake lives under pyenv on this machine)
PATH=~/.pyenv/versions/3.12.10/bin:$PATH cmake --build /home/jsho/repos/emulatorplatform/build --target gb_plugin
cd cores/gb/tests && ./run_tests.sh --json > /tmp/gb_scorecard.json
```

## Architecture

### Components

- **LR35902** (lr35902.cpp/hpp) - CPU with all opcodes including CB prefix
- **PPU** (ppu.cpp/hpp) - Scanline renderer with CGB color support
- **APU** (apu.cpp/hpp) - Four-channel audio (pulse, wave, noise)
- **Bus** (bus.cpp/hpp) - Memory mapping, timer, serial, interrupts
- **Cartridge** (cartridge.cpp/hpp) - ROM loading and MBC dispatch
- **MBC** (mbc/) - Memory bank controller implementations

### Memory Map

| Address | Size | Description |
|---------|------|-------------|
| $0000-$3FFF | 16KB | ROM Bank 0 (fixed) |
| $4000-$7FFF | 16KB | ROM Bank 1-N (switchable) |
| $8000-$9FFF | 8KB | VRAM (banked on CGB) |
| $A000-$BFFF | 8KB | External RAM (cartridge) |
| $C000-$DFFF | 8KB | Work RAM (banked on CGB) |
| $E000-$FDFF | - | Echo RAM (mirrors WRAM) |
| $FE00-$FE9F | 160B | OAM (sprite attributes) |
| $FF00-$FF7F | 128B | I/O Registers |
| $FF80-$FFFE | 127B | High RAM |
| $FFFF | 1B | Interrupt Enable |

## Battery Saves

Games with battery-backed RAM automatically save:
- Pokemon save files
- Zelda save files
- RTC data (MBC3)

Save files stored in Veloce saves directory: `~/.config/veloce/saves/`

## Source Files

| File | Purpose |
|------|---------|
| src/plugin.cpp | Plugin interface implementation |
| src/lr35902.cpp/hpp | Sharp LR35902 CPU emulation |
| src/ppu.cpp/hpp | PPU emulation |
| src/apu.cpp/hpp | APU emulation |
| src/bus.cpp/hpp | Memory bus, timer, interrupts |
| src/cartridge.cpp/hpp | ROM loading and MBC dispatch |
| src/mbc/ | MBC implementations (MBC1, MBC3, MBC5) |
