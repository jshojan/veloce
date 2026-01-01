# Veloce NES Plugin

NES (Nintendo Entertainment System) emulator plugin for the Veloce emulation platform.

## Building

The NES plugin is built as part of the main Veloce build, but can also be built independently:

```bash
# From the emulatorplatform root directory
mkdir -p build && cd build
cmake ..
cmake --build . --target nes_plugin

# Or build just the plugin from the plugins/nes directory
cd plugins/nes
mkdir -p build && cd build
cmake ..
cmake --build .
```

The plugin will be output as `nes.so` (Linux), `nes.dylib` (macOS), or `nes.dll` (Windows).

## Debug Mode

Debug logging can be enabled by setting the `DEBUG` environment variable or using the `--debug` flag:

```bash
# Using environment variable
DEBUG=1 ./veloce rom.nes

# Using command line flag
./veloce --debug rom.nes
```

When debug mode is enabled, the following information is logged to stderr:

### CPU State Debugging
- Periodic CPU state dumps (PC, A, X, Y, SP, P registers)
- Detection of CPU stuck in tight loops

### Mapper Debugging
- Bank switching operations
- IRQ counter state changes
- CHR/PRG bank configuration

## Supported Mappers

| Mapper | Name | Description |
|--------|------|-------------|
| 000 | NROM | No mapper, basic games (Super Mario Bros, Donkey Kong) |
| 001 | MMC1 | Nintendo MMC1 (Legend of Zelda, Metroid) |
| 002 | UxROM | PRG bank switching (Mega Man, Castlevania) |
| 003 | CNROM | CHR bank switching (Solomon's Key) |
| 004 | MMC3 | Nintendo MMC3 with scanline counter (Super Mario Bros 3, Kirby's Adventure) |
| 007 | AxROM | Single-screen mirroring (Battletoads) |
| 009 | MMC2 | Nintendo MMC2 with CHR latch (Punch-Out!!) |
| 010 | MMC4 | Nintendo MMC4 with CHR latch (Fire Emblem) |
| 011 | Color Dreams | Unlicensed mapper (Crystal Mines) |
| 034 | BNROM/NINA-001 | Discrete logic (Deadly Towers) |
| 066 | GxROM | PRG/CHR bank switching (Super Mario Bros + Duck Hunt) |
| 071 | Camerica | Codemasters games (Fire Hawk) |
| 079 | NINA-003/006 | American Video Entertainment (Krazy Kreatures) |
| 206 | DxROM/Namco 108 | Early Namco games (Babel no Tou) |

## Testing

The NES plugin is validated against the [nes-test-roms](https://github.com/christopherpow/nes-test-roms) test suite. To run tests with debug output:

```bash
DEBUG=1 ./veloce path/to/test.nes
```

### Test Results

| Test Suite | Status | Notes |
|------------|--------|-------|
| MMC3 1-clocking | PASS | A12 clocking via PPUADDR |
| MMC3 3-A12_clocking | PASS | A12 edge detection |
| MMC3 5-MMC3 | PASS | MMC3-specific behavior |
| MMC3 2-details | Partial | PPU frame timing (241 clocks) needs work |

Test ROM output is displayed when `DEBUG=1` is set. The emulator automatically detects test ROMs using the blargg signature (`0xDE 0xB0 0x61` at $6001-$6003) and reports results from $6000.

## Architecture

The NES plugin implements:
- **6502 CPU** - Full instruction set with cycle-accurate timing
- **PPU** - Picture Processing Unit with background/sprite rendering
- **APU** - Audio Processing Unit with pulse, triangle, noise, and DMC channels
- **Cartridge** - iNES/NES 2.0 ROM loading with mapper support
