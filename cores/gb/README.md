# Game Boy Emulator Core

Game Boy and Game Boy Color emulator for Veloce.

## Overview

The Game Boy core provides emulation for both the original Game Boy (DMG) and Game Boy Color (CGB). It features accurate Sharp LR35902 CPU emulation, PPU rendering, and four-channel audio processing. The core automatically detects ROM type and configures the appropriate mode.

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
- OAM corruption bug (DMG quirk) not implemented
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

### Test Suite

The Game Boy core uses [Blargg's gb-test-roms](https://github.com/retrio/gb-test-roms) for validation:

```bash
cd cores/gb/tests
python test_runner.py              # Run all tests
python test_runner.py --json       # JSON output for CI
python test_runner.py --category cpu  # CPU tests only
```

### Test Results Summary

#### CPU Instructions (Critical) - 11/11 Pass

| Test | Status |
|------|--------|
| 01-special.gb | Pass |
| 02-interrupts.gb | Pass |
| 03-op sp,hl.gb | Pass |
| 04-op r,imm.gb | Pass |
| 05-op rp.gb | Pass |
| 06-ld r,r.gb | Pass |
| 07-jr,jp,call,ret,rst.gb | Pass |
| 08-misc instrs.gb | Pass |
| 09-op r,r.gb | Pass |
| 10-bit ops.gb | Pass |
| 11-op a,(hl).gb | Pass |

#### Instruction Timing (High) - 1/1 Pass

| Test | Status |
|------|--------|
| instr_timing.gb | Pass |

#### Memory Timing (High) - 4/4 Pass

| Test | Status |
|------|--------|
| 01-read_timing.gb | Pass |
| 02-write_timing.gb | Pass |
| 03-modify_timing.gb | Pass |
| mem_timing.gb | Pass |

#### DMG Sound (Medium) - 12/12 Pass

| Test | Status |
|------|--------|
| 01-registers.gb | Pass |
| 02-len ctr.gb | Pass |
| 03-trigger.gb | Pass |
| 04-sweep.gb | Pass |
| 05-sweep details.gb | Pass |
| 06-overflow on trigger.gb | Pass |
| 07-len sweep period sync.gb | Pass |
| 08-len ctr during power.gb | Pass |
| 09-wave read while on.gb | Pass |
| 10-wave trigger while on.gb | Pass |
| 11-regs after power.gb | Pass |
| 12-wave write while on.gb | Pass |

#### CGB Sound (Medium) - 12/12 Pass

| Test | Status |
|------|--------|
| 01-registers.gb | Pass |
| 02-len ctr.gb | Pass |
| 03-trigger.gb | Pass |
| 04-sweep.gb | Pass |
| 05-sweep details.gb | Pass |
| 06-overflow on trigger.gb | Pass |
| 07-len sweep period sync.gb | Pass |
| 08-len ctr during power.gb | Pass |
| 09-wave read while on.gb | Pass |
| 10-wave trigger while on.gb | Pass |
| 11-regs after power.gb | Pass |
| 12-wave write while on.gb | Pass |

#### Interrupt Timing (High) - 1/1 Pass

| Test | Status |
|------|--------|
| interrupt_time.gb | Pass |

#### HALT Bug (Medium) - 1/1 Pass

| Test | Status |
|------|--------|
| halt_bug.gb | Pass |

#### OAM Bug (Low) - 0/1 Pass

| Test | Status | Notes |
|------|--------|-------|
| oam_bug.gb | Known Fail | DMG OAM corruption quirk |

### Known Test Failures

| Test | Issue | Impact |
|------|-------|--------|
| oam_bug.gb | DMG OAM corruption bug not emulated | Quirky DMG-only behavior, no game compatibility issues |

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
