# Veloce

A cross-platform, plugin-based emulator framework built for speedrunners and TAS creators. Supports Windows, Linux, and macOS (x86/ARM).

*Veloce* - Italian for "fast"

## Features

### Core Features
- **Plugin Architecture** - Emulator cores loaded as dynamic libraries (.dll/.so/.dylib)
- **Cycle-Accurate NES Emulation** - Dot-by-dot PPU rendering, accurate CPU timing
- **Save States** - 10 quick-save slots with F1-F10 hotkeys
- **Visual Input Configuration** - Interactive controller display for button mapping
- **Controller Support** - Keyboard and USB gamepad with hot-plugging
- **Per-Platform Bindings** - Separate input configs for each system
- **Debug Tools** - Memory viewer, CPU/PPU state inspection

### Speedrun Features
- **Live Timer** - Millisecond-precision timer with splits
- **Split Tracking** - Track segment times and cumulative time
- **Personal Best** - Stores and compares against your best times
- **Gold Splits** - Tracks best individual segment times
- **Sum of Best** - Shows your theoretical best time
- **Delta Display** - Color-coded comparison to PB

### TAS Features (Backend Complete, GUI Planned)
- **Movie Recording** - Record input frame-by-frame
- **Movie Playback** - Play back recorded inputs
- **Greenzone** - Automatic savestate snapshots for seeking
- **Undo/Redo** - Full edit history (100 levels)
- **Frame Editing** - Insert, delete, modify frames
- **FM2 Import** - Import movies from FCEUX
- **Selection & Clipboard** - Copy/cut/paste frame ranges
- **Markers** - Mark important frames with descriptions

> **Note:** TAS backend is fully functional but currently lacks a GUI. A piano roll editor is planned.

## Supported Systems

| System | Status | Mappers | Notes |
|--------|--------|---------|-------|
| NES | Playable | 0, 1, 2, 3, 4, 7, 9, 10, 11, 34, 66, 71, 79, 206 | Covers ~90% of NES library |
| Game Boy | Planned | - | Next priority |
| SNES | Planned | - | After GB |

### NES Mapper Coverage

**Core Mappers:**
- **Mapper 0 (NROM)** - Super Mario Bros., Donkey Kong
- **Mapper 1 (MMC1)** - Zelda, Metroid, Final Fantasy (~25% of library)
- **Mapper 2 (UxROM)** - Mega Man, Castlevania, Contra, Duck Tales
- **Mapper 3 (CNROM)** - Arkanoid, Gradius, Solomon's Key
- **Mapper 4 (MMC3)** - SMB3, Kirby, Mega Man 3-6 (scanline IRQ)
- **Mapper 7 (AxROM)** - Battletoads, Marble Madness, RC Pro-Am

**Extended Mappers:**
- **Mapper 9 (MMC2)** - Punch-Out!!, Mike Tyson's Punch-Out!! (CHR latching)
- **Mapper 10 (MMC4)** - Fire Emblem (Japan)
- **Mapper 11 (Color Dreams)** - Bible Adventures, Crystal Mines (unlicensed)
- **Mapper 34 (BNROM/NINA-001)** - Deadly Towers, Impossible Mission II
- **Mapper 66 (GxROM)** - Super Mario Bros. + Duck Hunt multicart
- **Mapper 71 (Camerica)** - Micro Machines, Fire Hawk, Quattro games
- **Mapper 79 (NINA-03/06)** - Various AVE games
- **Mapper 206 (DxROM/Namcot)** - Various Namco games, Tengen releases

## Building

### Prerequisites

All dependencies (SDL2, Dear ImGui, nlohmann/json) are automatically downloaded via CMake FetchContent.

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install -y build-essential cmake libgl-dev libglu1-mesa-dev
```

#### macOS
```bash
xcode-select --install
brew install cmake
```

#### Windows
- [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload
- CMake is included with Visual Studio

### Build Instructions

#### Linux / macOS
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/veloce
```

#### Windows (Command Prompt)
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\veloce.exe
```

#### Windows (Visual Studio IDE)
1. File -> Open -> CMake... -> Select `CMakeLists.txt`
2. Build -> Build All (F7)
3. Select "veloce.exe" as startup item and run (F5)

### Build Output
```
build/bin/
├── veloce(.exe)           # Main executable
└── plugins/
    ├── nes.so/.dll        # NES emulator
    ├── libaudio_default   # Audio plugin
    ├── libinput_default   # Input plugin
    └── libtas_default     # TAS plugin
```

## Usage

```bash
veloce [OPTIONS] [ROM_FILE]

Options:
  -h, --help       Show help and exit
  -v, --version    Show version and exit
  -d, --debug      Enable debug panel

Examples:
  veloce game.nes           # Load a NES ROM
  veloce --debug game.nes   # Debug mode
```

### Default Controls (NES)

| Button | Keyboard | Gamepad |
|--------|----------|---------|
| D-Pad | Arrow Keys | D-Pad / Left Stick |
| A | Z | A / Cross |
| B | X | B / Circle |
| Start | Enter | Start |
| Select | Right Shift | Back/Select |

### Hotkeys

| Action | Key |
|--------|-----|
| Pause/Resume | Escape |
| Frame Advance | F (when paused) |
| Reset | Ctrl+R |
| Fullscreen | F11 |
| Debug Panel | F12 |
| Quick Save (Slot 1-10) | F1-F10 |
| Quick Load (Slot 1-10) | Shift+F1-F10 |

## Save States

10 slots per ROM with hotkey access:
- **Save**: F1-F10
- **Load**: Shift+F1-F10

Files stored in:
- Linux/macOS: `~/.config/veloce/savestates/`
- Windows: `%APPDATA%\veloce\savestates\`

## Input Configuration

**Settings -> Input** provides:

- **Visual Mode** - Click buttons on an interactive controller graphic
- **Table Mode** - Traditional list with Set/Clear buttons
- **Per-Platform** - Separate bindings for NES, GB, SNES, etc.

## NES Emulation Details

### CPU (6502)
- All 56 official opcodes
- Cycle-accurate timing
- NMI and IRQ handling

### PPU (2C02)
- Dot-by-dot rendering
- Sprite 0 hit detection
- 8 sprites per scanline limit
- VBlank at scanline 241, cycle 1

### APU (2A03)
- 2 pulse channels with sweep
- Triangle and noise channels
- DMC (basic support)
- Low-pass filtering

## Project Structure

```
veloce/
├── include/emu/           # Plugin interfaces
│   ├── emulator_plugin.hpp
│   ├── audio_plugin.hpp
│   ├── input_plugin.hpp
│   ├── tas_plugin.hpp
│   └── speedrun_plugin.hpp
├── src/
│   ├── core/              # Application core
│   └── gui/               # Dear ImGui interface
├── plugins/
│   ├── nes/               # NES emulator
│   ├── audio_default/     # Audio backend
│   ├── input_default/     # Input backend
│   └── tas_default/       # TAS engine
└── speedrun_plugins/      # Auto-splitters (planned)
```

## Roadmap

### Completed
- [x] Core framework (window, audio, input, plugins)
- [x] NES emulator (CPU, PPU, APU)
- [x] Mappers 0, 1, 2, 3, 4, 7, 9, 10, 11, 34, 66, 71, 79, 206
- [x] Save states (10 slots, hotkeys)
- [x] Visual input configuration
- [x] Per-platform controller bindings
- [x] USB controller with hot-plug
- [x] Debug panel with memory viewer
- [x] Speedrun timer with splits, PB, golds
- [x] TAS backend (recording, playback, greenzone, undo)

### In Progress
- [ ] TAS Editor GUI (piano roll)

### Next Up: Game Boy
- [ ] Game Boy emulator plugin
- [ ] GB CPU (Sharp LR35902)
- [ ] GB PPU with proper timing
- [ ] GB APU (4 channels)
- [ ] MBC1, MBC3, MBC5 mappers
- [ ] Game Boy Color support

### Then: More NES + SNES
- [ ] Additional NES mappers (5, 16, 18, 19, 21, 22, 23, 24, 25, 69, etc.)
- [ ] RAM watch panel
- [ ] Game-specific auto-split plugins
- [ ] SNES emulator plugin
- [ ] SNES CPU (65C816)
- [ ] SNES PPU (Mode 7, etc.)
- [ ] SNES APU (SPC700 + DSP)

### Future
- [ ] Netplay (race mode)
- [ ] Rewind functionality
- [ ] Shader support (CRT, scanlines)
- [ ] LiveSplit .lss import/export
- [ ] TAS movie verification/sync

## Configuration

Config stored in:
- Linux/macOS: `~/.config/veloce/`
- Windows: `%APPDATA%\veloce\`

### Files
```
config/
├── input_nes.json      # NES bindings
├── input_gb.json       # GB bindings (future)
└── input_snes.json     # SNES bindings (future)
splits/
└── *.json              # Personal bests
savestates/
└── <CRC32>_slot*.sav   # Save state files
```

## Troubleshooting

### Duplicate Controllers (Windows)
Windows may show the same controller twice (XInput + DirectInput). Veloce filters by GUID but some may still appear. Either works.

### Input Blocked During Config
Intentional. Game input is blocked while Settings -> Input is open.

### Save States Not Working
- Ensure ROM is loaded
- Check config directory is writable
- States are per-ROM (by CRC32)

## License

MIT License - See LICENSE file.

## Acknowledgments

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [SDL2](https://www.libsdl.org/) - Cross-platform multimedia
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library
- [nes-test-roms](https://github.com/christopherpow/nes-test-roms) - NES emulator test ROMs for accuracy validation
