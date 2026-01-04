# Veloce

A cross-platform, plugin-based emulator framework built for speedrunners and TAS creators.

*Veloce* - Italian for "fast"

| Platform | Status |
|----------|--------|
| Windows  | Supported |
| Linux    | Supported |
| macOS    | Supported (x86/ARM) |

## Overview

Veloce is a modular emulator platform that combines cycle-accurate console emulation with advanced features for speedrunning and tool-assisted speedruns (TAS). The architecture follows a Project64-style plugin system where emulator cores, audio backends, input handlers, and TAS tools are all swappable components.

The framework prioritizes accuracy where it matters for speedrunning (timing-sensitive operations, RNG behavior) while maintaining playable performance on modern hardware.

## Quick Start

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run
./build/bin/veloce game.nes
```

See [Build Instructions](#building) for platform-specific details.

## Supported Systems

| System | Core | Status | Notes |
|--------|------|--------|-------|
| NES/Famicom | [cores/nes](cores/nes/README.md) | Playable | 20+ mappers, ~90% library coverage |
| Game Boy / Color | [cores/gb](cores/gb/README.md) | In Progress | DMG and CGB, MBC1/3/5 |
| Game Boy Advance | [cores/gba](cores/gba/README.md) | In Progress | ARM7TDMI, all video modes |
| Super Nintendo | [cores/snes](cores/snes/README.md) | In Progress | 65C816, Mode 7, SPC700 |

## Architecture

Veloce uses a plugin architecture where functionality is divided into distinct, swappable components:

```
veloce (application)
    |
    +-- Plugin Manager
    |       |
    |       +-- Emulator Cores (IEmulatorPlugin)
    |       |       NES, GB, GBA, SNES
    |       |
    |       +-- Audio Plugin (IAudioPlugin)
    |       |       Volume, muting, processing
    |       |
    |       +-- Input Plugin (IInputPlugin)
    |       |       Keyboard, gamepad, recording
    |       |
    |       +-- TAS Plugin (ITASPlugin)
    |       |       Movie editing, greenzone, playback
    |       |
    |       +-- Game Plugins (IGamePlugin) [Multiple]
    |       |       Timer, splits, auto-splitters
    |       |       Each plugin renders its own GUI panel
    |       |       Self-contained with IGameHost interface
    |       |
    |       +-- Netplay Plugin (INetplayPlugin)
    |               Rollback netcode, delay-based
    |
    +-- Core Services
            Savestate Manager, GamePluginHost (IGameHost)
```

### Game Plugin Architecture

Game plugins are self-contained components that can:
- Render their own ImGui panels via `render_gui()`
- Access emulator memory for auto-split detection
- Track timer state, splits, and personal bests

**Multiple game plugins can be active simultaneously**, enabling:
- Built-in timer + game-specific auto-splitter
- Different timer layouts for different use cases
- Community-created auto-splitter plugins

Game plugin selection is configurable in Settings > Plugins.

### Plugin Documentation

**Auxiliary Plugins:**
- [Audio Plugin](plugins/audio_default/README.md) - Audio passthrough with volume control
- [Input Plugin](plugins/input_default/README.md) - Keyboard and gamepad input handling
- [TAS Plugin](plugins/tas_default/README.md) - TAS movie recording and editing
- [Game Plugin](plugins/speedrun_tools_default/README.md) - Built-in timer, splits, and PB tracking

**Emulator Cores:**
- [NES Core](cores/nes/README.md) - Nintendo Entertainment System
- [Game Boy Core](cores/gb/README.md) - Game Boy and Game Boy Color
- [GBA Core](cores/gba/README.md) - Game Boy Advance
- [SNES Core](cores/snes/README.md) - Super Nintendo Entertainment System

### Plugin Interfaces

All plugin interfaces are defined in include/emu/:

| Header | Interface | Description |
|--------|-----------|-------------|
| emulator_plugin.hpp | IEmulatorPlugin | ROM loading, frame execution, save states |
| audio_plugin.hpp | IAudioPlugin | Audio processing and output |
| input_plugin.hpp | IInputPlugin | Input polling and configuration |
| tas_plugin.hpp | ITASPlugin | Movie recording, playback, editing |
| game_plugin.hpp | IGamePlugin | Timer, splits, auto-splitters, RAM watch |
| netplay_plugin.hpp | INetplayPlugin | Network multiplayer support |

## Features

### Core Features

- 10 save state slots with F1-F10 hotkeys
- Visual input configuration with interactive controller display
- Per-platform controller bindings
- USB gamepad support with hot-plugging
- Debug tools (memory viewer, CPU/PPU state)

### Speedrun Features

- Millisecond-precision live timer with splits
- Personal best tracking and comparison
- Gold splits (best segment times)
- Sum of best calculation
- Color-coded delta display
- Per-game auto-splitter support via game plugins
- LiveSplit integration (planned)

### TAS Features

- Frame-by-frame movie recording and playback
- Greenzone (automatic savestate snapshots for seeking)
- Undo/redo with 100 levels of history
- Frame insertion, deletion, and modification
- FM2 movie import (FCEUX format)
- Selection, copy/cut/paste operations
- Frame markers with descriptions

### Netplay Features

- GGPO-style rollback netcode
- Delay-based mode for stable connections
- Session codes for easy joining
- In-game chat with timestamps
- Desync detection and recovery

## Building

### Prerequisites

Dependencies (SDL2, Dear ImGui, nlohmann/json) are automatically downloaded via CMake FetchContent.

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install -y build-essential cmake libgl-dev libglu1-mesa-dev
```

**macOS:**
```bash
xcode-select --install
brew install cmake
```

**Windows:**
- Visual Studio 2022 with "Desktop development with C++" workload
- CMake is included with Visual Studio

### Build Commands

**Linux / macOS:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/veloce
```

**Windows (Command Prompt):**
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\veloce.exe
```

## Usage

```bash
veloce [OPTIONS] [ROM_FILE]

Options:
  -h, --help       Show help and exit
  -v, --version    Show version and exit
  -d, --debug      Enable debug panel

Environment Variables:
  DEBUG=1          Enable debug output
  HEADLESS=1       Run without GUI (for automated testing)
  FRAMES=N         Run for N frames then exit (requires HEADLESS=1)
```

### Hotkeys

| Action | Key |
|--------|-----|
| Pause/Resume | Escape |
| Frame Advance | F (when paused) |
| Reset | Ctrl+R |
| Fullscreen | F11 |
| Debug Panel | F12 |
| Netplay Panel | F8 |
| Quick Save (Slot 1-10) | F1-F10 |
| Quick Load (Slot 1-10) | Shift+F1-F10 |

### Default Controls

| Button | Keyboard | Gamepad |
|--------|----------|---------|
| D-Pad | Arrow Keys | D-Pad / Left Stick |
| A | Z | A / Cross |
| B | X | B / Circle |
| Start | Enter | Start |
| Select | Right Shift | Back/Select |

## Configuration

Configuration files are stored in:
- Linux/macOS: ~/.config/veloce/
- Windows: %APPDATA%\veloce\

## Testing

Each emulator core includes test suites for accuracy validation. See the individual core documentation:

- [NES Testing](cores/nes/README.md#testing)
- [Game Boy Testing](cores/gb/README.md#testing)
- [GBA Testing](cores/gba/README.md#testing)
- [SNES Testing](cores/snes/README.md#testing)

### Headless Mode

For CI/CD integration:

```bash
HEADLESS=1 FRAMES=600 veloce test.nes
DEBUG=1 HEADLESS=1 FRAMES=600 veloce test.nes  # With debug output
```

## Project Structure

```
veloce/
    include/emu/              Plugin interfaces
    src/
        core/                 Application core
        gui/                  Dear ImGui interface
    cores/                    Emulator cores
        nes/                  NES emulator
        gb/                   Game Boy emulator
        gba/                  GBA emulator
        snes/                 SNES emulator
    plugins/                  Auxiliary plugins
        audio_default/        Audio backend
        input_default/        Input backend
        tas_default/          TAS engine
        speedrun_tools_default/  Built-in timer (IGamePlugin)
```

## Acknowledgments

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [SDL2](https://www.libsdl.org/) - Cross-platform multimedia
- [nlohmann/json](https://github.com/nlohmann/json) - JSON library
- [nes-test-roms](https://github.com/christopherpow/nes-test-roms) - NES emulator test ROMs
- [gba-tests](https://github.com/jsmolka/gba-tests) - GBA emulator test ROMs by jsmolka
- [snes-tests](https://github.com/gilyon/snes-tests) - SNES emulator test ROMs by gilyon
- [Project64](https://www.pj64-emu.com/) - Plugin architecture inspiration

## License

MIT License - See [LICENSE](LICENSE) file.
