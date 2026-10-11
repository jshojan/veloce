# Game Boy Emulator Core

<!-- completion:start -->
## Completion percentage

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| Game Boy / Game Boy Color | **40%** | 8/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](../../docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Game Boy / Game Boy Color delivered milestones: Guest sound produces audio samples, Native digital controller/keypad registers consume frontend input, Guest graphics produce rendered video, Parse a native cartridge/executable format (boot scored separately), Repository-owned ROM source and reproducible archived image, Separate core plugin and ABI entry points, Actual guest register-file output independently graded on the host, with negative controls, Complete states for the implemented profile and deterministic replay.

Remaining: Mooneye result detection, interrupt/timing edges, link, additional media and full hardware corpus remain.
<!-- completion:end -->

M-cycle accurate Game Boy and Game Boy Color emulator for Veloce.

## Overview

The Game Boy plugin implements DMG/CGB CPU, display, audio and cartridge profiles. The fixed delivery checklist is reported above; whole-platform hardware accuracy and compatibility percentages have not been established.

## Specifications

| Component | Details |
|-----------|---------|
| CPU | Sharp LR35902 (SM83 core) |
| Resolution | 160 x 144 |
| Frame Rate | 59.7275 Hz |
| Clock Speed | 4.194304 MHz (DMG), 8.388608 MHz (CGB double-speed) |
| Colors | 4 shades (DMG), 32768 colors (CGB) |

## Completion Status

### Implemented CPU profile

| Feature | Status | Notes |
|---------|--------|-------|
| Main opcodes (256) | Implemented profile | All standard instructions |
| CB-prefix opcodes (256) | Implemented profile | All bit manipulation and rotate/shift |
| Cycle timing | Implemented profile | M-cycle accurate |
| HALT mode | Implemented profile | Proper wake-on-interrupt |
| STOP mode | Implemented profile | Speed switching (CGB) |
| HALT bug | Implemented profile | IME=0 with pending interrupt quirk |
| OAM bug | Implemented profile | DMG OAM corruption quirk |
| Interrupts | Implemented profile | VBlank, LCD STAT, Timer, Serial, Joypad |

### Implemented PPU profile

| Feature | Status | Notes |
|---------|--------|-------|
| Background rendering | Implemented profile | Tile maps, scrolling |
| Window rendering | Implemented profile | WX/WY positioning |
| Sprite rendering | Implemented profile | 8x8 and 8x16 modes, priority |
| OAM DMA | Implemented profile | 160-cycle transfer |
| DMG palettes | Implemented profile | 4-shade grayscale |
| CGB palettes | Implemented profile | 8 BG + 8 OBJ palettes, 32768 colors |
| CGB VRAM banking | Implemented profile | Bank 0/1 switching |
| LCD STAT interrupts | Implemented profile | Mode 0/1/2, LYC=LY |
| Pixel FIFO | Partial | Scanline-based (accurate enough for most games) |

### Implemented APU profile

| Feature | Status | Notes |
|---------|--------|-------|
| Channel 1 (Pulse) | Implemented profile | Sweep, envelope, duty cycle |
| Channel 2 (Pulse) | Implemented profile | Envelope, duty cycle |
| Channel 3 (Wave) | Implemented profile | 16-byte wave RAM |
| Channel 4 (Noise) | Implemented profile | LFSR, envelope |
| Frame sequencer | Implemented profile | 512 Hz, 8 steps |
| Length counters | Implemented profile | All 4 channels |
| Stereo panning | Implemented profile | NR51 register |
| Master volume | Implemented profile | NR50 register |

### CGB Features

| Feature | Status | Notes |
|---------|--------|-------|
| Double-speed mode | Implemented profile | KEY1 register |
| HDMA transfers | Implemented profile | General-purpose and HBlank DMA |
| Color palettes | Implemented profile | BCPS/BCPD, OCPS/OCPD |
| VRAM banking | Implemented profile | VBK register |
| WRAM banking | Implemented profile | SVBK register |

### Known Limitations

- Pixel FIFO is scanline-based rather than dot-by-dot (sufficient for game compatibility)
- Some CGB-only timing edge cases may not be hardware-perfect

## MBC Support

| MBC | Features | Common Games | Status |
|-----|----------|--------------|--------|
| None | 32KB ROM and optional unbanked RAM profile | Tetris | Functional profile |
| MBC1 | Up to 2MB ROM, 32KB RAM | Pokemon Red/Blue, Zelda | Implemented profile |
| MBC3 | RTC, up to 2MB ROM | Pokemon Gold/Silver/Crystal | Implemented profile |
| MBC5 | Up to 8MB ROM, 128KB RAM | Pokemon Crystal, GBC games | Implemented profile |

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

**Current evidence:** the repository-owned register probe, arithmetic corpus
and unary corpus grade actual guest output independently. Result-channel
adapters now support the supplemental catalog. Historical catalog pass counts
and scores require a new complete run before use as current compatibility
results. Visual-only entries remain unscored. No whole-core hardware accuracy
percentage is inferred from these bounded tests.

### Test Suite

The Game Boy core uses the shared Veloce testkit (`tests/veloce_testkit`) so its
detection, scoring, and JSON scorecard are identical to the other consoles. The
authoritative test ROM set is the prebuilt
[c-sp gameboy-test-roms v7.0 bundle](https://github.com/c-sp/gameboy-test-roms)
(the upstream Mooneye/Mealybug/SameSuite/acid2 repos ship only RGBDS source, not
runnable `.gb` ROMs). The runner downloads and unpacks it into
`cores/gb/tests/roms/` on first run.

`cores/gb/tests/test_config.json` is schema-v2 and enumerates **448 tests across
41 suites** (418 scored, 30 visual), organized by hardware subsystem with a per-test `result_detection`,
`accuracy_type`, and `priority`:

| Source | Subsystems exercised |
|--------|----------------------|
| Blargg `cpu_instrs`, `instr_timing`, `mem_timing(-2)`, `halt_bug`, `interrupt_time`, `oam_bug`, `dmg_sound`, `cgb_sound` | CPU instr + instruction/memory timing, HALT bug, OAM bug, APU |
| Mooneye acceptance (`timer`, `ppu`, `interrupts`, `oam_dma`, `timing`, `bits`, `serial`, boot/div) | Cycle-accurate timer/PPU/interrupt/IME/HALT/DMA/control-flow timing |
| Mooneye `misc` (CGB hardware: boot regs/HWIO/DIV, unused HWIO, VBlank STAT IRQ) | Post-boot CGB state (`known_fail`: DMG-header carts run as DMG, see `known_issues.model_selection`) |
| GBMicrotest (212 of 513: timer/DIV, OAM DMA, HBlank IRQ vs SCX, HALT wakeup, LCD-on STAT, VRAM/OAM lock) | Single-cycle timer/PPU/interrupt/access probes |
| Mooneye `emulator-only` (MBC1/MBC2/MBC5) | Mappers / banking |
| Mealybug Tearoom (DMG + CGB) | Mid-scanline (mode-3) PPU register changes, pixel-perfect |
| Wilbertpol | PPU sub-scanline STAT/mode timing |
| SameSuite (APU ch1-4 + DIV/registers, CGB DMA/HDMA, EI/HALT, BGPI lock) | Cycle-accurate APU, CGB DMA, interrupts, palette access |
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

| Method | Suites | Status |
|--------|--------|--------|
| `serial` (ASCII "Passed"/"Failed") | Blargg cpu_instrs, instr_timing, mem_timing | Works (verified) |
| `file` (SB/SC register stream) | Original register probe | 2/2 checks verified, including injected failure |
| `serial` (Mooneye Fibonacci reg + LD B,B) | All Mooneye, SameSuite, Wilbertpol | **Not yet detected** -- see below |
| `screenshot-crc` (CRC32 of framebuffer PNG) | Mealybug, acid2, Blargg-sound, halt_bug, interrupt_time, mem_timing-2 | Pipeline works; `reference_hash` values not yet populated |

| Driver | Tier | Suites | What it writes |
|--------|------|--------|----------------|
| `serial` | A | ROMs built with the Veloce emitter | every byte sent with `SC=$81`, raw; the ROM's own `CHECK`/`END` lines |
| `blarggtext` | B | Blargg `cpu_instrs`, `instr_timing`, `mem_timing` | the ROM's serial text, then `END 1/1` for a `Passed` line or `END 0/1 [code=n]` for `Failed [#n]` |
| `a000` | B | Blargg `oam_bug`, `dmg_sound`, `cgb_sound`, `mem_timing-2`, `halt_bug`, `interrupt_time` | the `$A000` text as `LOG` lines (when not mirrored to serial), `END` from the status byte, reset request on `$81` |
| `hram` | B | GBMicrotest (cartridge title `microtest`) | `CHECK 1 PASS/FAIL hram exp=<$FF81> got=<$FF80>` + `END` once `$FF82` is `$01`/`$FF` |
| `mooneye` | C | Mooneye, SameSuite, Wilbertpol | `REG b= c= d= e= h= l=` + `END 1/1` (Fibonacci fingerprint at `LD B,B`) or `END 0/1 code=66` (all `$42`) |
| `screenshot-crc` | n/a | Mealybug, acid2 | visual only (`visual_test_suites`); `reference_hash` values not populated, never scored |

Any adapter stands down once the ROM has sent a `VELOCE 1` header or an `END`
line over serial, and only one terminator is ever written. With a sink attached
the cartridge RAM is zeroed when attached before the first frame, so a battery save from an earlier run can
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

All **708 supplemental images** rebuild from the merged recipes: two Blargg,
513 GBMicrotest, 115 Mooneye and 78 SameSuite images. SameSuite's obsolete
include-guard `SET` directive is normalized to `EQU` with the same constant
value in a declared recipe command. Its source-only pin has no shipped-binary
golden gate. Older release-bundle comparisons in provenance are historical;
this integration does not infer physical hardware correctness from a rebuild.
Supplemental ROM binaries and upstream clones remain ignored. The original
WLA/RGBDS emitter fixtures are built with their own `build.sh` and both match
their literal source transcripts, including intentional failures.

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

## Result-channel branch integration (2026-10-11)

The older `gb/test-channel` history is reconciled with current main. One module
owns raw serial bytes plus Mooneye, A000, title-armed HRAM and Blargg-text
adapters. An explicit `VELOCE 1` serial stream suppresses inferred verdicts.
RAM is cleared only when the sink is attached before the first frame; late
attachment preserves guest state. The original probe and strict arithmetic/
unary graders now require the actual full channel list. Instrumentation adds
no machine state fields.

**10/10 selected GB/shared harness gates pass in 101.15 seconds**, including
**8,388,608 arithmetic** and **73,728 unary** actual register-pair observations,
executed corrupted-capture and malformed-stream controls, and the small original
register probe. Twelve original production-plugin fixtures execute real CPU
stores/register loads and check adapter status, fingerprint, title gating and
explicit serial precedence. Their actual output is written to
`build/artifacts/owned/gb_result_channel/*.capture`.

The fixture exposed the disabled RAM path for unbanked headers 08/09. Their
functional profile now decodes the fixed A000-BFFF RAM window and recognizes
09 battery persistence. **4,194,304 address/byte checks**, reset/state/save replay
and out-of-window guards pass; UBSAN passes. The final address pattern also
separates each 256-byte page. Two compiled/executed faults (disabled reads and
256-byte address aliasing) are rejected. State layout is unchanged.
[Pan Docs no-MBC mapping](https://github.com/gbdev/pandocs/blob/master/src/nombc.md)
describes optional unbanked RAM. Its [header documentation](https://github.com/gbdev/pandocs/blob/master/src/The_Cartridge_Header.md#0147--cartridge-type)
notes that licensed cartridges do not use 08/09 and their exact behavior is
unknown. This is a tested functional homebrew profile, not physical validation
of licensed hardware.

All **708 supplemental images** rebuild. Both original WLA/RGBDS emitter
fixtures match their source transcripts, repeat byte-for-byte, and their
intentional **1/3 failure** is correctly detected. The SameSuite recipe's
include-guard normalization fixes its assembler blocker; source-only/golden
exceptions remain explicit. All **117 archived original images reproduce**.
Build/testkit selftests, full build and completion rendering pass. Supplemental
ROM bytes and cloned source stay out of git.

Game Boy remains **40% (8/20 delivery milestones)**. Full CPU/timing/peripheral/
mapper corpus, hardware comparison, link multiplayer/netplay and broader retail
verification remain incomplete. The root and every core percentage stay in
sync; N64 remains 45%; CI/CD remains disabled. GBA, SNES, older documentation
and incomplete NES APU branches remain to reconcile.
