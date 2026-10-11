# Veloce

<!-- completion:start -->
## Core completion percentages

| Platform | Delivery checklist completion | Delivered / total |
|---|---:|---:|
| NES/Famicom | **50%** | 10/20 |
| SNES | **40%** | 8/20 |
| Game Boy / Game Boy Color | **40%** | 8/20 |
| Game Boy Advance | **40%** | 8/20 |
| Nintendo 64 | **50%** | 10/20 |
| Virtual Boy | **20%** | 4/20 |
| Nintendo DS | **30%** | 6/20 |
| Nintendo DSi | **30%** | 6/20 |
| Nintendo 3DS | **30%** | 6/20 |
| GameCube | **25%** | 5/20 |
| Wii | **25%** | 5/20 |
| Wii U | **25%** | 5/20 |
| Nintendo Switch | **25%** | 5/20 |

These percentages count the 20 explicit delivery milestones in [the completion ledger](docs/CORE-COMPLETION.md); partial or unverified milestones receive zero credit. Each milestone is worth 5 percentage points. **They are not percentages of native hardware accuracy or retail compatibility.** Bounded profile tests do not complete a whole CPU or hardware subsystem.

Super Mario 64 (USA) was tested locally on 2026-10-11. The six-billion-attempt intro reaches the courtyard. A shared private checkpoint now verifies **analog walking, A jumping and B punching through actual RAM fields**, with stationary neutral controls and no terminal CPU/RSP/DP stop during those continuations. **180 legacy quantum movie frames and six seek boundaries match complete native states**; an executed neutral movie leaves Mario standing. New native sessions advance on **VI field boundaries**, with an original **5,160-case file oracle** and **121 VI movie frames / six exact seeks**; legacy states retain the old budget. Castle entry, menu/level progression, practical normal-speed play, full-title playability and native firmware boot remain unverified. **N64 now reaches 50%** of the fixed delivery checklist. Its original signed-stick corpus grades 65,536 cases / 1,048,576 RAM/register words; the original one-cycle rectangle corpus grades 1,048,576 cases / 13,631,488 words. All twelve integer trap instructions are now verified by an original **15,728,640-case ROM / 188,743,680 RAM/CP0 words**. Linked word/doubleword loads and conditional stores now have an original **1,048,576-case ROM / 10,485,760 RAM words**, including ERET failures and a live-link checkpoint. Disabled COP2 exception priority now has an original **9,056,256-case ROM / 108,675,072 RAM/CP0 words**, plus an exhaustive encoded-payload component gate. Four local controller ports now pass **194 ordinary frontend cases / 1,164 guest RAM words**, including all 24 device assignments and an executed wrong-route rejection. Independent extra-player profiles pass **3,026 remap/persistence cases / 18,156 guest RAM words** and remapped four-port movies. CP0 physical Watch traps now have an original **4,194,816-case ROM / 58,727,424 RAM/CP0 words**, with armed-checkpoint replay and R/W/EXL controls.

Twenty-eight doubleword integer/LWU families now enforce User/Supervisor mode restrictions: **5,768,792 original guest cases / 69,225,504 RAM/CP0 words**, with executed NOP rejection and native checkpoint/rollback. Nine wide integer memory families now verify Kernel32 address formation: **4,718,592 original guest cases / 56,623,104 RAM/CP0 words**, with extended-address/alignment controls and native rollback. CP0 BadVAddr/PRId/CacheErr write protection now has an original **12,582,912-case cartridge / 150,994,944 actual RAM/register words**, with hardware exception seeding, executed wrong-read rejection and native rollback. Config fixed/writable fields and actual K0 cache/bypass reads now have an original **16,777,216-case cartridge / 201,326,592 actual RAM/register words**, with EP/BE defaults held and clock-field preservation controls. Status writable/reserved/TS write rules now have an original **4,194,304-case cartridge / 50,331,648 actual RAM/register words**, with safe EXL-held readbacks and unconstrained component controls. TagLo/TagHi/PErr field writes now have an original **12,582,912-case cartridge / 150,994,944 actual RAM/register words**, plus every physical tag in defined functional CACHE consumer controls. This remains partial CPU coverage; N64 stays 50%. Per-player analog sources, inversion, dead zone and range now have **4,035 ordinary frontend cases / 24,210 actual guest RAM words**, saved-profile migration and remapped four-port native movies. Physical controller calibration/peripherals, two-cycle graphics/LOD, enabled expansion-coprocessor behavior, precise atomic/cache interference and full CPU/hardware/audio/timing remain unfinished. See [checkpoint control and movie evidence](cores/n64/README.md#private-courtyard-controls-and-movie-checkpoints).

3DS ARM11 now implements twelve ARMv6 signed/unsigned extend-and-add forms and four Thumb extensions. Its original image grades **13,631,488 snapshots / 95,420,416 actual register words** on both ARM11 contexts, with independent arithmetic, executed negative controls and native checkpoint replay. This remains partial CPU support; 3DS stays **30%**. Native firmware/retail boot, MMU, graphics/audio and analog/touch/wireless peripherals remain pending.

3DS ARM11 scalar and dual-halfword SSAT/USAT saturation now has an original **12,582,912-case image / 88,080,384 actual register words**, independent clamping arithmetic, sticky overflow checks, executed negative controls and complete native checkpoint replay. This remains bounded CPU coverage; 3DS stays **30%**.

3DS ARM11 now implements 36 ARMv6 parallel byte/halfword arithmetic forms, GE flag writes and SEL. The original image grades **19,922,944 snapshots / 179,306,496 actual register words**, including a following SEL that consumes real arithmetic flags. Independent grading, executed negative controls and native checkpoint replay cover the documented finite domains. The whole CPU milestone remains unfinished; 3DS stays **30%**.
<!-- completion:end -->

A cross-platform, plugin-based emulator framework built for speedrunners and TAS creators.

*Veloce* - Italian for "fast"

| Platform | Status |
|----------|--------|
| Windows  | Build configured; runtime unverified in this workspace |
| Linux    | Build and executed local gates verified |
| macOS    | Build configured; runtime unverified in this workspace |

## Overview

Veloce is a modular emulator platform that combines Nintendo console emulation with features for speedrunning and tool-assisted speedruns (TAS). The architecture follows a Project64-style plugin system where emulator cores, audio backends, input handlers, and TAS tools are all swappable components.

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
| Game Boy / Color | [cores/gb](cores/gb/README.md) | In Progress | DMG and CGB, MBC1/3/5; serial register test channel |
| Game Boy Advance | [cores/gba](cores/gba/README.md) | In Progress | ARM7TDMI, all video modes |
| Super Nintendo | [cores/snes](cores/snes/README.md) | In Progress | 65C816, Mode 7, SPC700 |
| Nintendo 64 | [cores/n64](cores/n64/README.md) | Experimental | MIPS/RSP/RDP profiles with complete packets, fill-cycle triangles and functional one-cycle RGBA32 shade/primitive/environment pixels, AI PCM/VI framebuffer, four digital/analog ports and two-player TCP rollback; native boot/playability, complete textured/depth-tested rendering, physical analog calibration and timing pending |
| Virtual Boy | [cores/vb](cores/vb/README.md) | Experimental | V810 integer homebrew, multiply/divide, exception and arithmetic bit-string corpora; no retail games, video, audio or controller hardware |
| Nintendo DS | [cores/nds](cores/nds/README.md) | Experimental | Dual-CPU ARM/Thumb integer, memory, long-call and stack/block homebrew with digital keypad input with shared/private RAM; no firmware/retail boot, graphics, audio or peripheral hardware |
| Nintendo DSi | [cores/dsi](cores/dsi/README.md) | Experimental | Distinct plugin; dual-CPU integer homebrew, 16 MiB RAM/aliases and RAM-limit controls, digital keypad and TCP host/viewer replay; firmware/retail boot and most DSi peripherals pending |
| Nintendo 3DS | [cores/3ds](cores/3ds/README.md) | Experimental | Distinct plugin; two ARM11 and one ARM9 integer contexts, 128 MiB shared FCRAM, AXI/private RAM and digital HID; TCP host/viewer replay; native boot and graphics/audio pending |
| Wii U | [cores/wiiu](cores/wiiu/README.md) | Experimental | Distinct three-context PowerPC integer plugin, 32 MiB MEM1/2 GiB MEM2, sparse states, original input latch and TCP host/viewer replay; native boot, GPU/audio and GamePad hardware pending |
| GameCube | [cores/ppc](cores/ppc/README.md) | Experimental | Original PowerPC arithmetic/carry, multiply/divide, memory transfers, bitwise, rotate/mask, shift/unary, branch and condition-register programs; settled digital GameCube-controller SI reads; no retail boot, graphics/audio, analog/polling or netplay |
| Wii | [cores/ppc](cores/ppc/README.md) | Experimental | Original PowerPC arithmetic/carry, multiply/divide, memory transfers, bitwise, rotate/mask, shift/unary, branch and condition-register programs; settled digital GameCube-controller SI reads; no retail boot, MEM2, IOS, Wii Remotes, graphics/audio or netplay |
| Nintendo Switch | [cores/switch](cores/switch/README.md) | Experimental | Uncompressed NSO0, AArch64 integer, multiply/divide, conditional-select, shift/bit-operation, bit-field/extract, logical-immediate and conditional-compare guests, independent register corpora and full profile states; full CPU/MMU/HOS, firmware, graphics/audio, peripherals and retail playability pending |

Nintendo Switch now has an experimental original AArch64/uncompressed NSO0 core; firmware, HOS and retail compatibility remain pending. DSi enhanced hardware remains partially implemented. Native hardware accuracy/completion is not yet
measured; delivery checklist completion is reported above. See the [hardware coverage and remaining work](docs/HARDWARE-COVERAGE.md).

## Architecture

Veloce uses a plugin architecture where functionality is divided into distinct, swappable components:

```
veloce (application)
    |
    +-- Plugin Manager
    |       |
    |       +-- Emulator Cores (IEmulatorPlugin)
    |       |       NES, GB, GBA, SNES, N64, VB, DS, DSi, 3DS, GC, Wii, Wii U
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
- [N64 Core](cores/n64/README.md) - Experimental Nintendo 64 integer homebrew core
- [Nintendo DS Core](cores/nds/README.md) - Experimental dual-CPU ARM homebrew core
- [Nintendo 3DS Core](cores/3ds/README.md) - Experimental ARM11/ARM9 integer profile
- [Nintendo DSi Core](cores/dsi/README.md) - Experimental expanded-RAM dual-CPU homebrew plugin
- [Wii U Core](cores/wiiu/README.md) - Experimental three-context PowerPC integer profile
- [GameCube/Wii Cores](cores/ppc/README.md) - Experimental shared PowerPC integer profiles
- [Virtual Boy Core](cores/vb/README.md) - Experimental V810 integer homebrew core

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
- SNES states use `VSNES002` to preserve pending math operations; recreate older SNES savestates.
- Visual input configuration with interactive controller display
- Per-platform controller bindings
- USB gamepad support with hot-plugging
- Debug tools (memory viewer, CPU/PPU state)

### Frontend plugin wiring

The frame loop drives the selected `IAudioPlugin` for both streaming and
frame-end audio. The host resamples processed stereo floats to the SDL device;
volume/mute, source-rate changes, plugin replacement and rollback suppression
have executed checks. See [audio integration evidence](plugins/audio_default/README.md#frontend-integration).
The selected `IInputPlugin` now supplies frame input. Default Input preserves
the built-in manager’s saved controller profiles and applies plugin overrides,
turbo and recording/playback. Local play and netplay read one cached sample per
logical frame. See [input integration evidence](plugins/input_default/README.md#frontend-integration).
TAS ABI 2 plugins are initialized with the live application host. Netplay and game plugins are driven by the frontend;
netplay support depends on the active core's implemented interface and profile.
Physical controller and audio-device validation remains pending.

### Speedrun Features

- Millisecond-precision live timer with splits
- Personal best tracking and comparison
- Gold splits (best segment times)
- Sum of best calculation
- Color-coded delta display
- Per-game auto-splitter support via game plugins
- LiveSplit integration (planned)

### TAS Features

Open **Tools → Movie editor** with a loaded ROM and disconnected netplay.
Record, open/save, play, pause/step, seek, edit controller masks and reset/save
events, insert/delete frames, and undo/redo. The frontend commits movie input
only after the core advances. Reset-start movies restore the ROM-load boundary;
state-start movies restore their captured native state.

The production NES frontend test compares **121 frames** of ordinary execution,
recording and playback in native serialized state and cartridge RAM files.
Six seek boundaries, state starts, reset/save events and an executed input mutant
are checked. This establishes replay consistency for that NES profile; cross-core
hardware accuracy, complete FM2 timing/peripherals and exact paused video previews
remain unverified. Core completion percentages are unchanged. See the
[TAS evidence and limits](plugins/tas_default/README.md#frontend-integration).

Selection/clipboard and markers are backend APIs; the editor currently exposes
single-frame operations. Lua scripting and complete peripheral support remain
unfinished.

### Netplay Features

The default plugin supports direct IPv4 host/join sessions. A
[rollback module](plugins/netplay_default/README.md)
now saves frame history, predicts input, restores/replays late corrections, and
stalls at its speculation limit. Two delayed NES sessions match an ordinary
100-frame run byte-for-byte, including guest register observations in files.
A nonblocking IPv4 TCP transport validates ROM/core/build/start-state agreement.
Two independent processes over loopback TCP also match the full baseline state;
ROM mismatches and four malformed-message cases are rejected.

The default plugin now uses both modules in the frontend. Load the same ROM in
both apps, choose **Netplay → Host Game** on one and **Join Game** on the other,
and enter the host's IPv4 address/TCP port. Both cores reload and pause until the
handshake matches; play then starts automatically. The frontend polls while
paused, runs one live frame through the plugin, and suppresses replay audio.
Reset, ROM/core replacement, and savestate loads disconnect and pause the session.

The production shared plugin passes the same two-process comparison, mismatch,
and malformed-message tests, plus replay-audio and core/frame-change checks;
results are saved under `build/artifacts/netplay-plugin/`. Netplay ABI is now v2;
rebuild external netplay plugins. Single-controller cores now support a host
and one joining viewer. The DS host owns its keypad; the viewer predicts and
corrects host input, with complete states and eight guest words matching a
100-frame baseline. This mirrors one DS instance; DS wireless multiplayer
remains pending. Reports remain in `build/artifacts/netplay-nds/`.
DSi and 3DS also pass production host/viewer replay. The 3DS probe grades
twelve guest words from both ARM11 contexts and ARM9; both peers match the
complete 135,791,096-byte baseline. This mirrors one 3DS instance; wireless
multiplayer is pending. Full rollback states use substantial memory (~1.14 GiB
per peer at window 8, plus the core). Reports are in
`build/artifacts/netplay-3ds/`; see [scope and usage](cores/3ds/README.md).
Wii U now passes production host/viewer rollback with sparse states and twelve
independently graded guest words, including high MEM2 readbacks. Its original
probe's full final state is 17,392 bytes; omitted pages represent zero across
the full RAM map. History buffers and frontend snapshots now query current
serialized size and grow as needed. Native GamePad/VPAD/KPAD and wireless
multiplayer remain pending. Reports are in `build/artifacts/netplay-wiiu/`.
N64 now passes two-player production TCP rollback: host/client inputs reach separate
digital/analog PIF ports, while ports 2/3 remain neutral. Both peers match 600 original
guest words and complete 9,527,616-byte S029 states. Four virtual ports are supported
by the core API; signed stick input, selected-pad mapping and input recording are
verified. Physical analog calibration, accessories and a four-peer network remain
pending. Reports: `build/artifacts/netplay-n64/`; [implementation and scope](cores/n64/README.md#four-digital-ports-and-production-netplay).
Relay/session codes, chat, additional spectators, input
delay scheduling, reconnects, desync recovery, Windows and real two-machine
verification remain. Internet multiplayer is not validated.

### Controller connection and bindings

Identical SDL gamepads remain separate devices. Custom gamepad bindings now save
the model GUID and serial when available and restore after an unambiguous
reconnect or restart. Devices of the same model without unique serials may
require remapping when several are connected. Legacy configurations containing
only numeric connection IDs need remapping once to gain saved identities.

The SDL virtual-controller integration test covers identical pads, duplicate
events, remapping, capture mode, disconnect/reconnect, saved bindings, selection
after removal, ambiguous identities, and normalized axis endpoints:
`ctest --test-dir build -L controllers --output-on-failure`.
N64 exposes four local ports in Input Configuration. Player one keeps its saved
button profile. Select **Edit player** to map each player’s buttons independently;
extra-player gamepad bindings follow the assigned device, and explicit keyboard
keys can be configured separately. Defaults use standard gamepad buttons,
left-stick analog, left-trigger Z and right-stick C buttons. Clear removes an
extra-player binding; Reset restores only the selected profile. A pad belongs
to one assigned port.
Saved port assignments use GUID/serial identity and require reselection when
identical models cannot be distinguished. All 24 assignments and every native
button on each port are verified through real SDL virtual devices, the production
frontend/default input plugin and original guest SI replies in RAM.
Independent extra-player button/axis maps now pass 3,026 remap/persistence cases
and remapped four-port movie replay/seek tests. Saved profiles include cleared
bindings and migrate older configuration files to standard extra-player maps.
Physical USB/Bluetooth, cross-platform drivers and analog-axis remapping remain
unverified or unfinished.

## Building

### Prerequisites

Requires a C++17 compiler and CMake 3.16+. CMake uses system SDL2 when available;
otherwise it fetches SDL2. Dear ImGui and nlohmann/json are fetched at configure
time, so a first configure needs network access and Git. Python 3 is required
while `VELOCE_BUILD_TESTS` is enabled (the default).

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install -y build-essential cmake git python3 libgl-dev libsdl2-dev
```

**macOS:**
```bash
xcode-select --install
brew install cmake sdl2
```

**Windows:**
- Visual Studio 2022 or 2026 with the "Desktop development with C++" workload
- CMake and Ninja are bundled with Visual Studio (no separate install needed)
- The build targets the MSVC toolchain; MinGW/Clang are not currently tested

### Build Commands

**Linux / macOS:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/veloce
```

**Windows — Visual Studio IDE (recommended):**

The repository ships a `CMakeSettings.json` with an `x64-Debug` (Ninja) configuration, so the project builds out of the box:

1. Launch Visual Studio 2022/2026 and choose **Open a local folder**, then select the repository root.
2. Visual Studio detects `CMakeLists.txt` and configures automatically (the first run downloads SDL2/ImGui/json via FetchContent — give it a minute).
3. Pick the `x64-Debug` configuration and build with **Build > Build All** (or `Ctrl+Shift+B`).
4. The binary and plugins land in `out\build\x64-Debug\bin\`. Run via **Select Startup Item > veloce.exe**.

**Windows — command line (Developer PowerShell for VS):**
```powershell
# Debug — configures into the same Ninja tree the IDE's x64-Debug uses
cmake -S . -B out\build\x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out\build\x64-Debug
.\out\build\x64-Debug\bin\veloce.exe

# Release — optimized (/O2), matches the IDE's x64-Release configuration.
# Use this build for actual gameplay; Debug is significantly slower.
cmake -S . -B out\build\x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out\build\x64-Release
.\out\build\x64-Release\bin\veloce.exe
```

> Run the commands from a **Developer PowerShell / Developer Command Prompt for VS** so the MSVC compiler (`cl.exe`) and Ninja are on `PATH`. Both `x64-Debug` and `x64-Release` are also selectable directly from the Visual Studio configuration dropdown (see `CMakeSettings.json`).

**Windows — Visual Studio solution generator (alternative):**
```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\veloce.exe
```

> The MSVC build sets `_CRT_SECURE_NO_WARNINGS` and `/Zc:__cplusplus /utf-8` automatically (see `CMakeLists.txt`), so the POSIX-style standard-library calls in the cores compile without warning noise.

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
  VELOCE_TEST_OUT=<path>    Write the machine-readable VELOCE-RESULT/1 test
                            verdict to <path> (implies HEADLESS=1). See
                            docs/testing/VELOCE-RESULT.md.
  VELOCE_TEST_EXIT=0|1     Stop the frame loop once a verdict is seen
                            (default 1); FRAMES stays a hard budget either way.
  VELOCE_TEST_RESETS=N     Cap on ROM-requested resets during a test run
                            (default 3).
  INPUT=frame:hexmask,...  Deterministic button-press schedule for tests
                            that need scripted input.
```

`--help` and `--version` exit before initializing SDL, plugins or runtime paths.
Unknown options exit with status 1. Seven executed CLI cases cover both short
and long flags, a missing-ROM/help combination and diagnostic/version output.

### Hotkeys

| Action | Key |
|--------|-----|
| Pause/Resume | Escape |
| Frame Advance | F (when paused) |
| Reset | Ctrl+R |
| Fullscreen | F11 |
| Debug Panel | F12 |
| Netplay Panel | Ctrl+N |
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

Veloce ships a shared, cross-console accuracy framework that produces a
defensible, reproducible "this core is N% accurate" number per subsystem and per
console. Start here:

- [TESTING.md](TESTING.md) - testing philosophy, harness architecture, the
  result-detection protocols, how to run the suites, and exactly how the
  completeness percentage is computed.
- [COMPLETENESS.md](COMPLETENESS.md) - the per-console accuracy scorecard, every
  number justified against what is verified versus unverified.
- [docs/testing/VELOCE-RESULT.md](docs/testing/VELOCE-RESULT.md) - the
  `VELOCE_TEST_OUT` result-file protocol every scored verdict is built on:
  the ROM-facing port per console, the plugin ABI, and how a core adopts it.

| Console | Verified headline | Detail |
|---------|-------------------|--------|
| NES | ~90-94% (strongest verified coverage) | [scorecard](COMPLETENESS.md#nes) / [notes](cores/nes/README.md#testing) |
| SNES | Original settled math: 68,864 cases and 504,832 selected timing observations verified; external visual catalog unscored | [scorecard](COMPLETENESS.md#snes) / [notes](cores/snes/README.md#testing) |
| Game Boy | Blargg serial subset, original probe, 8,388,608 ALU pairs and 73,728 unary pairs verified; Mooneye unscored | [scorecard](COMPLETENESS.md#game-boy) / [notes](cores/gb/README.md#testing) |
| GBA | partial (CPU/memory/BIOS/saves verified) | [scorecard](COMPLETENESS.md#gba) / [notes](cores/gba/README.md#test-status) |
| N64 | Experimental integer/PI/native handoff: 524,288 cartridge-DMA cases and twelve startup captures, plus 524,288 RI cases and 393,216 MI transitions and 14,336 device cases plus 2,097,152 cache cases, 196,608 SP control and 65,536 SP DMA cases, 2,048 AI FIFO cases and 65,536 signed PCM codes per channel; 28,127 integer, 262,623 doubleword multiply/divide and 3,158,016 merge-memory results and all 16,384 digital controller combinations and 65,536 interrupt gates verified; FPU data/control/conversion and 346,224 arithmetic capture rows also pass, with unfiltered VI video, AI PCM and 2,277,376 RSP scalar rows; full platform completion unknown | [implementation and limits](cores/n64/README.md) |
| Virtual Boy | Experimental integer homebrew: 6,512,640 result/PSW pairs plus 4,210,688 multiply/divide and 4,685,824 TRAP/illegal and 4,982,784 address-trap snapshots, plus 17,055,744 CAXI/port and 14,680,064 arithmetic bit-string snapshots, plus 100,712,448 immediate-instruction snapshots; full platform completion unknown | [implementation and limits](cores/vb/README.md) |
| Nintendo DS | Experimental dual-CPU homebrew: 44,238,336 integer plus 38,338,560 multiply snapshots plus 51,426,816 Thumb result/CPSR/source/PC and 49,694,208 memory plus 9,437,184 long-call snapshots matched; 3,720,960 stack/block snapshots matched; 4,096/4,096 digital keypad combinations matched; full platform completion unknown | [implementation and limits](cores/nds/README.md) |
| Nintendo DSi | 387,072/387,072 original RAM observations matched; 67,109,008 component bus observations; 100-frame TCP host/viewer states and eight guest register words match oracle; full hardware completion unknown | [implementation and limits](cores/dsi/README.md) |
| Nintendo 3DS | 25,165,824/25,165,824 original reversal captures matched; 36,123,648 component register snapshots and 541,065,300 RAM bus observations; 4,096 digital HID inputs; 100-frame TCP host/viewer states and twelve guest words match baseline/oracle; full hardware completion unknown | [implementation and limits](cores/3ds/README.md) |
| Nintendo Switch | Experimental uncompressed NSO0/AArch64 profile: independent arithmetic, multiply/divide and conditional-select and shift/bit-operation and bit-field/extract and logical-immediate and conditional-compare guest register corpora, complete condition/NZCV domains and production state replay; full native hardware completion unknown | [implementation and limits](cores/switch/README.md) |
| Wii U | 25,165,824/25,165,824 original ADDI/ADDIS captures matched; 28,311,552 component register snapshots and 8,724,152,428 RAM bus observations; 100-frame TCP states and twelve guest words match baseline/oracle; full hardware completion unknown | [implementation and limits](cores/wiiu/README.md) |
| GameCube / Wii | Each profile: 49,152/49,152 arithmetic, 74,752/74,752 branch and 42,108,928/42,108,928 condition-register and 6,324,224/6,324,224 integer logical and 9,437,184/9,437,184 rotate/mask and 4,416,528/4,416,528 shift/unary and 25,168,384/25,168,384 carry arithmetic snapshots and 8,420,320/8,420,320 defined multiply/divide results matched; 1,056 undefined division rows have defined flags/preserved fields checked; 12,586,368/12,586,368 memory-transfer snapshots matched; full platform completion unknown | [implementation and limits](cores/ppc/README.md) |

Run external ROM suites with `python tests/run_all.py`; see [TESTING.md](TESTING.md)
for per-console and local usage. The percentages above describe that measured
subset, not whole-core completion.

### Original register-output ROMs

| Original corpus | Measured coverage | Result artifacts |
|---|---|---|
| [NES ADC/SBC](tests/roms-src/nes/alu_exhaustive/README.md) | 524,288/524,288 A/P observations matched (100% of all 8-bit operand pairs and C/D states, zero-page addressing) | `build/artifacts/owned/nes_alu/` |
| [NES rollback controller probe](tests/roms-src/nes/netplay_probe/README.md) | Execution, TCP transport, and production-plugin 100-frame checks match complete baseline states; guest counters recorded; hardware timing unverified | `build/artifacts/rollback/`, `build/artifacts/network/`, `build/artifacts/netplay-plugin/` |
| [GB CPU/WRAM probe](tests/roms-src/gb/register_probe/README.md) | 2/2 checks passed; injected wrong WRAM value fails | `build/artifacts/owned/gb_register_probe/` |
| [GB byte ALU](tests/roms-src/gb/alu_exhaustive/README.md) | 8,388,608/8,388,608 A/F pairs matched (100% of every operand pair and incoming Z/N/H/C state for eight E-register ALU operations); timing and other operands remain unverified | `build/artifacts/owned/gb_alu/` |
| [GB unary instructions](tests/roms-src/gb/unary_exhaustive/README.md) | 73,728/73,728 A/F pairs matched (100% of 18 accumulator instructions, all bytes and all incoming flags) | `build/artifacts/owned/gb_unary/` |
| [SNES math registers](tests/roms-src/snes/math_registers/README.md) | 65,536/65,536 multiplication pairs and 3,328 division boundary cases matched; settled results only | `build/artifacts/owned/snes_math/` |
| [SNES math timing](tests/roms-src/snes/math_timing/README.md) | 504,832/504,832 register bytes matched across CPU phases 2–18 (100% of selected multiplicands/dividends, all 256 second operands, four result registers) | `build/artifacts/owned/snes_math_timing/` |
| [Virtual Boy integer registers](tests/roms-src/vb/integer_registers/README.md) | 6,512,640/6,512,640 pairs matched; six arithmetic/logic operations on all repeated-byte pairs and boundaries, three register shifts, all incoming flags; platform hardware/timing pending | `build/artifacts/owned/vb_integer/` |
| [Virtual Boy multiply/divide](tests/roms-src/vb/muldiv/README.md) | 4,210,688 nine-word snapshots, including 8,704 zero-divisor exception/RETI cases; all signed/unsigned byte pairs, full-width boundaries and incoming flags | `build/artifacts/owned/vb_muldiv/` |
| [Virtual Boy exceptions](tests/roms-src/vb/exceptions/README.md) | 4,685,824/4,685,824 thirteen-word snapshots matched: all TRAP parameters, three illegal opcode families, every writable nonfatal PSW combination and complete low-bit encoding sweeps; entry/RETI context | `build/artifacts/owned/vb_exceptions/` |
| [Virtual Boy address traps](tests/roms-src/vb/address_traps/README.md) | 4,982,784/4,982,784 fourteen-word snapshots matched: full nonfatal PSWs on six addresses, every single-bit PC flip and low-half offset; 131,168 matching enabled cases, entry/RETI context and nonmatching execution | `build/artifacts/owned/vb_address_traps/` |
| [Virtual Boy memory transactions](tests/roms-src/vb/memory_ops/README.md) | 17,055,744/17,055,744 nine-word snapshots matched: CAXI byte comparisons/replacements/full-width triples and OUT/IN byte/half/word pairs; all low-bit lanes and incoming flags; storage-profile ports, physical wiring/timing pending | `build/artifacts/owned/vb_memory_ops/` |
| [Virtual Boy arithmetic bit strings](tests/roms-src/vb/bitstrings/README.md) | 14,680,064/14,680,064 fourteen-word snapshots matched: eight operations, every byte pair, all offsets and twelve lengths; memory/progress/PSW observations; overlap and timing unverified | `build/artifacts/owned/vb_bitstrings/` |
| [Virtual Boy immediate instructions](tests/roms-src/vb/immediates/README.md) | 100,712,448/100,712,448 three-word snapshots matched: all wide/short immediates for twelve instructions on 16 operand patterns and all incoming flags; full operands and timing unverified | `build/artifacts/owned/vb_immediates/` |
| [Nintendo DS dual-CPU integer registers](tests/roms-src/nds/integer_registers/README.md) | 44,238,336/44,238,336 four-word snapshots matched: all sixteen data-processing operations, repeated-byte pairs and full-width boundaries, all NZCV and ARM9 Q states; firmware, timing and most hardware pending | `build/artifacts/owned/nds_integer/` |
| [Nintendo DS multiply registers](tests/roms-src/nds/multiply_registers/README.md) | 38,338,560/38,338,560 five-word observations matched: six multiply/accumulate forms, S clear/set, all NZCV/ARM9 Q, byte pairs and boundary/accumulator seeds; unpredictable ARM7 flags unscored, timing pending | `build/artifacts/owned/nds_multiply/` |
| [Nintendo DS saturating arithmetic](tests/roms-src/nds/saturating_registers/README.md) | 10,846,208/10,846,208 four-word observations matched: QADD/QSUB/QDADD/QDSUB, repeated-byte pairs, full-width boundaries, all NZCV/Q, conditions and aliases; executed mutant rejected; timing/full DSP pending | `build/artifacts/owned/nds_saturating/` |
| [Nintendo DS CLZ registers](tests/roms-src/nds/clz_registers/README.md) | 12,646,272/12,646,272 three-word observations matched: all 16-bit operands in three placements, zero/bit boundaries, all NZCV/Q, conditions and aliases; executed off-by-one mutant rejected; timing/full CPU pending | `build/artifacts/owned/nds_clz/` |
| [Nintendo DS memory exchanges](tests/roms-src/nds/swap_registers/README.md) | 33,300,480/33,300,480 six-word register/RAM snapshots matched on both CPUs: SWP/SWPB byte pairs/boundaries, lanes, aliases, all flags/conditions; executed missing-store mutant rejected; physical locking/aborts/timing pending | `build/artifacts/owned/nds_swap/` |
| [Nintendo DS ARM block transfers](tests/roms-src/nds/arm_multiple_registers/README.md) | 2,881,632/2,881,632 register/RAM snapshots matched on both CPUs: all nonempty lists, address modes and defined write-back/base cases; 50,385,536 decoder checks; User-bank/SPSR, physical aborts/timing pending | `build/artifacts/owned/nds_arm_multiple/` |
| [Nintendo DS doubleword transfers](tests/roms-src/nds/doubleword_registers/README.md) | 16,646,144/16,646,144 nine-word snapshots matched: LDRD/STRD, every immediate offset, full-width register boundaries, all addressing forms/flags/conditions and valid aliases; executed missing-load mutant rejected; physical abort/alignment configuration/timing pending | `build/artifacts/owned/nds_doubleword/` |
| [Nintendo DS DSP multiplication](tests/roms-src/nds/dsp_multiply_registers/README.md) | 15,454,208/15,454,208 seven-word snapshots matched: all 16 ARMv5TE signed multiply forms, complete selected-halfword sweeps, accumulation/flags/conditions/aliases; executed wrong-half mutant rejected; full CPU/timing pending | `build/artifacts/owned/nds_dsp_multiply/` |
| [Nintendo DS ARM BLX immediate](tests/roms-src/nds/arm_blx_registers/README.md) | 16,777,088/16,777,088 eight-word snapshots matched with actual Thumb targets; all 33,554,432 immediate/H encodings pass a separate decoder gate; executed wrong-H mutant rejected; cache/timing/full CPU pending | `build/artifacts/owned/nds_arm_blx/` |
| [Nintendo DS Thumb registers](tests/roms-src/nds/thumb_registers/README.md) | 51,426,816/51,426,816 five-word observations matched: all low ALU operations, register ADD/SUB, all shift and low register ALU immediates, all NZCV/ARM9 Q and observed Thumb PC; ARM7 MUL C unscored, timing pending | `build/artifacts/owned/nds_thumb/` |
| [Nintendo DS memory registers](tests/roms-src/nds/memory_registers/README.md) | 49,694,208/49,694,208 nine-word snapshots matched: ARM halfword/signed and Thumb transfers, all halfword values and immediate fields; physical timing pending | `build/artifacts/owned/nds_memory/` |
| [DSi RAM registers](tests/roms-src/dsi/ram_registers/README.md) | 387,072/387,072 fourteen-word captures matched: RAM modes/banks/CPU writers/aliases/widths/lanes/byte-complement patterns; timing pending | `build/artifacts/owned/dsi_ram/` |
| [3DS ARM11 reversals](tests/roms-src/3ds/reversal_registers/README.md) | 25,165,824/25,165,824 seven-word captures matched: both ARM11s, ARM/Thumb REV/REV16/REVSH, all NZCVQ and complemented-half values; full operands/timing pending | `build/artifacts/owned/3ds_reversal/` |
| [Wii U immediates](tests/roms-src/wiiu/immediate_registers/README.md) | 25,165,824/25,165,824 nine-word captures matched: three contexts, ADDI/ADDIS, all 16-bit immediates and XER flag combinations on four sources/zero and ordinary bases; full operands/timing pending | `build/artifacts/owned/wiiu_immediates/` |
| [Switch conditional comparisons](tests/roms-src/switch/ccmp_registers/README.md) | 786,432 packed rows / 3,932,160 actual words / 12,582,912 flag observations; every condition and input/fallback NZCV; 14,155,776 component snapshots | `build/artifacts/owned/switch_ccmp/` |
| [Switch logical immediates](tests/roms-src/switch/logical_imm_registers/README.md) | 1,449,984 rows / 7,249,920 actual words across every legal encoded mask and both widths; 197,296,128 component snapshots; all 20,224 reserved controls reject | `build/artifacts/owned/switch_logical_imm/` |
| [Switch bit-fields/extract](tests/roms-src/switch/bitfield_registers/README.md) | 494,592 rows / 2,967,552 actual words across every legal immediate in both widths; 247,414,784 component snapshots; physical timing pending | `build/artifacts/owned/switch_bitfield/` |
| [Switch shifts/bit operations](tests/roms-src/switch/bitops_registers/README.md) | 1,120,096 rows / 5,600,480 actual words across masked shifts, bit/byte reversal and leading counts; 9,218,304 component snapshots; physical timing pending | `build/artifacts/owned/switch_bitops/` |
| [Switch conditional select](tests/roms-src/switch/select_registers/README.md) | 524,288 rows / 2,621,440 actual words across both widths, all condition/NZCV inputs and 16 boundary operands; 151,846,912 component snapshots; physical timing pending | `build/artifacts/owned/switch_select/` |
| [Switch multiply/divide](tests/roms-src/switch/muldiv_registers/README.md) | 2,071,552 rows / 12,429,312 words matched across 14 forms, both widths and boundary/repeated-byte operands; 33,147,520 component snapshots; physical hardware timing pending | `build/artifacts/owned/switch_muldiv/` |
| [Switch AArch64 arithmetic](tests/roms-src/switch/arithmetic_registers/README.md) | 1,572,864 observations / 7,864,320 actual words matched across six forms, both widths and carry values; 13,877,248 integer component cases; native hardware scope incomplete | `build/artifacts/owned/switch_arithmetic/` |
| [Wii U netplay probe](tests/roms-src/wiiu/netplay_probe/README.md) | 100-frame independent TCP peers match complete canonical states and twelve guest words, including high-RAM readbacks; host original input/viewer; native GamePad pending | `build/artifacts/netplay-wiiu/` |
| [3DS netplay probe](tests/roms-src/3ds/netplay_probe/README.md) | 100-frame independent TCP peers match full 135,791,096-byte states and twelve guest words; host HID/viewer; wireless multiplayer pending | `build/artifacts/netplay-3ds/` |
| [DSi netplay probe](tests/roms-src/dsi/netplay_probe/README.md) | 100-frame independent TCP peers match full baseline states and eight guest register words; host keypad/viewer, wireless multiplayer pending | `build/artifacts/netplay-dsi/` |
| [Nintendo DS long calls](tests/roms-src/nds/long_calls/README.md) | 9,437,184/9,437,184 eight-word snapshots matched: adjacent BL/ARM9 BLX pairs, every high and valid low field individually, both halfword positions, all NZCV/ARM9 Q; physical cache coherence/timing pending | `build/artifacts/owned/nds_long_calls/` |
| [Nintendo DS stack/block registers](tests/roms-src/nds/multiple_registers/README.md) | 3,720,960/3,720,960 twenty-five-word snapshots matched: every defined list/base encoding, sixteen register/memory seeds, all NZCV/ARM9 Q and POP-PC target states; physical abort/timing pending | `build/artifacts/owned/nds_multiple/` |
| [Nintendo DS keypad](tests/roms-src/nds/keypad/README.md) | 4,096/4,096 digital combinations matched through the actual frontend schedule, both CPU KEYINPUT and ARM7 X/Y halfword/byte captures; unknown/reserved bits unscored; physical timing pending | `build/artifacts/owned/nds_keypad/` |
| [Nintendo DS netplay probe](tests/roms-src/nds/netplay_probe/README.md) | Production-plugin TCP host/viewer: 100 frames match complete baseline states and eight independently graded guest words; DS wireless link remains pending | `build/artifacts/netplay-nds/` |
| [GameCube integer registers](tests/roms-src/gc/integer_registers/README.md) | 49,152/49,152 six-word snapshots matched; six arithmetic families, sixteen operand patterns, all incoming XER flags and OE/Rc forms; most hardware pending | `build/artifacts/owned/gc_integer/` |
| [Wii integer registers](tests/roms-src/wii/integer_registers/README.md) | 49,152/49,152 six-word snapshots matched on the supplied Broadway profile; most hardware pending | `build/artifacts/owned/wii_integer/` |
| [GameCube branch registers](tests/roms-src/gc/branch_registers/README.md) | 74,752/74,752 seven-word BC/BCLR/BCCTR snapshots matched: canonical BO/all BI, CTR/CR boundaries, LK and indirect target alignments; physical timing pending | `build/artifacts/owned/gc_branches/` |
| [Wii branch registers](tests/roms-src/wii/branch_registers/README.md) | 74,752/74,752 seven-word conditional-branch snapshots matched on the supplied profile; most hardware pending | `build/artifacts/owned/wii_branches/` |
| [GameCube conditions](tests/roms-src/gc/condition_registers/README.md) | 42,108,928/42,108,928 eight-word compare/CR snapshots matched: every 16-bit immediate on sixteen operands/all CR fields/SO states, all logical bit-field encodings on selected CR patterns, MCRF/MCRXR; timing pending | `build/artifacts/owned/gc_conditions/` |
| [Wii conditions](tests/roms-src/wii/condition_registers/README.md) | 42,108,928/42,108,928 eight-word compare/CR snapshots matched on the supplied profile; most hardware pending | `build/artifacts/owned/wii_conditions/` |
| [GameCube integer logical](tests/roms-src/gc/logical_registers/README.md) | 6,324,224/6,324,224 nine-word snapshots matched: eight register logical families/all XER flags/Rc and all immediate values for six logical forms on eight operands/both SO states; timing pending | `build/artifacts/owned/gc_logical/` |
| [Wii integer logical](tests/roms-src/wii/logical_registers/README.md) | 6,324,224/6,324,224 nine-word snapshots matched on the supplied profile; most hardware pending | `build/artifacts/owned/wii_logical/` |
| [GameCube rotate/mask](tests/roms-src/gc/rotate_registers/README.md) | 9,437,184/9,437,184 ten-word snapshots matched: all shifts/mask fields on eight sources/four insert destinations/four RLWNM upper-count patterns/both SO/Rc; timing pending | `build/artifacts/owned/gc_rotates/` |
| [Wii rotate/mask](tests/roms-src/wii/rotate_registers/README.md) | 9,437,184/9,437,184 ten-word snapshots matched on the supplied profile; most hardware pending | `build/artifacts/owned/wii_rotates/` |
| [GameCube shift/unary](tests/roms-src/gc/shift_registers/README.md) | 4,416,528/4,416,528 nine-word snapshots matched: all byte/halfword extension inputs on four upper patterns, all shift counts on sixteen sources/all XER flags/Rc, every leading-zero result; timing pending | `build/artifacts/owned/gc_shifts/` |
| [Wii shift/unary](tests/roms-src/wii/shift_registers/README.md) | 4,416,528/4,416,528 nine-word snapshots matched on the supplied profile; most hardware pending | `build/artifacts/owned/wii_shifts/` |
| [GameCube carry arithmetic](tests/roms-src/gc/carry_registers/README.md) | 25,168,384/25,168,384 nine-word snapshots matched: NEG/extended unary OE/Rc forms and all carry immediates on sixteen sources/all XER flags; timing pending | `build/artifacts/owned/gc_carry/` |
| [Wii carry arithmetic](tests/roms-src/wii/carry_registers/README.md) | 25,168,384/25,168,384 nine-word snapshots matched on the supplied profile; most hardware pending | `build/artifacts/owned/wii_carry/` |
| [GameCube multiply/divide](tests/roms-src/gc/multiply_registers/README.md) | 8,420,320/8,420,320 defined results matched; 1,056 undefined division rows graded for defined flags/preserved fields; timing pending | `build/artifacts/owned/gc_products/` |
| [Wii multiply/divide](tests/roms-src/wii/multiply_registers/README.md) | 8,420,320/8,420,320 defined results matched; 1,056 undefined division rows graded for defined flags/preserved fields; timing pending | `build/artifacts/owned/wii_products/` |
| [GameCube memory registers](tests/roms-src/gc/memory_registers/README.md) | 12,586,368/12,586,368 ten-word observations matched: every aligned 16-bit displacement on eight patterns/three aliases, indexed/update/signed/reversed forms; timing pending | `build/artifacts/owned/gc_memory/` |
| [Wii memory registers](tests/roms-src/wii/memory_registers/README.md) | 12,586,368/12,586,368 ten-word observations matched: every aligned 16-bit displacement on eight patterns/three aliases, indexed/update/signed/reversed forms; timing pending | `build/artifacts/owned/wii_memory/` |
| [GameCube controller serial](tests/roms-src/gc/controller_serial/README.md) | 4,096/4,096 sixteen-word digital captures matched: all twelve-button combinations, idle ID, absent ports, settled completion/errors and acknowledgement; polling/analog/timing pending | `build/artifacts/owned/gc_controller/` |
| [Wii GameCube-controller serial](tests/roms-src/wii/controller_serial/README.md) | 4,096/4,096 sixteen-word captures matched through Wii SI address; Wii Remotes, polling/analog/timing pending | `build/artifacts/owned/wii_controller/` |
| [N64 cache coherence](tests/roms-src/n64/cache_coherence/README.md) | 2,097,152/2,097,152 D-cache cases; 12,582,912 guest values; I-cache/tag/dirty replay component evidence; physical timing/errors pending | `build/artifacts/owned/n64_cache_coherence/` |
| [N64 SP controls](tests/roms-src/n64/sp_controls/README.md) | 196,608/196,608 eight-word captures matched: status pairs, MI source, semaphore, PC and CPU interrupt level; RSP execution pending | `build/artifacts/owned/n64_sp_controls/` |
| [N64 SP DMA](tests/roms-src/n64/sp_dma/README.md) | 65,536/65,536 six-word captures matched: every raw length, both directions/banks and four wrap boundaries; scheduling/readback physical validation pending | `build/artifacts/owned/n64_sp_dma/` |
| [N64 TLB translation](tests/roms-src/n64/tlb_translation/README.md) | 114,688/114,688 eleven-word guest captures matched; seven sizes, all ASIDs, global/valid/dirty faults; physical pipeline/shutdown behavior pending | `build/artifacts/owned/n64_tlb_translation/` |
| [N64 FPU controls](tests/roms-src/n64/fpu_controls/README.md) | 8,192/8,192 eight-word guest captures matched; exhaustive declared control/branch fields in component harness, arithmetic/timing pending | `build/artifacts/owned/n64_fpu_controls/` |
| [N64 AI FIFO](tests/roms-src/n64/ai_fifo/README.md) | 2,048/2,048 eleven-word guest captures matched; clock/status-counter physical behavior pending | `build/artifacts/owned/n64_ai_fifo/` |
| [N64 AI PCM](tests/roms-src/n64/ai_pcm/README.md) | Nine guest FIFO/IRQ captures and 66,169 production ABI stereo pairs covering all 65,536 signed sample values per channel matched; buffered/streaming replay verified, physical timing pending | `build/artifacts/owned/n64_ai_pcm_audio/` |
| [N64 RDRAM devices](tests/roms-src/n64/rdram_devices/README.md) | 14,336/14,336 cases; 86,016 guest values; four-device register discovery/storage; native RAM remapping/calibration pending | `build/artifacts/owned/n64_rdram_devices/` |
| [N64 MI controls](tests/roms-src/n64/mi_controls/README.md) | 393,216/393,216 mode/mask transitions; 3,145,728 guest captures with real SI/PI requests; RDRAM transaction effects/timing pending | `build/artifacts/owned/n64_mi_controls/` |
| [N64 RI controls](tests/roms-src/n64/ri_registers/README.md) | 524,288/524,288 cases; 3,145,728 guest reads; current-load/state component checks; RDRAM chip startup and physical timing pending | `build/artifacts/owned/n64_ri_registers/` |
| [N64 cartridge DMA](tests/roms-src/n64/pi_dma/README.md) | 524,288/524,288 aligned cases; 8,388,608 guest values; busy/quirk/peripheral timing pending | `build/artifacts/owned/n64_pi_dma/` |
| [N64 VI registers](tests/roms-src/n64/vi_registers/README.md) | 65,536 rows / 983,040 actual register values matched; CURRENT/IRQ component domains verified | `build/artifacts/owned/n64_vi_registers/` |
| [N64 VI framebuffer](tests/roms-src/n64/vi_framebuffer/README.md) | 24 guest values and 131,072 production ABI pixels matched across RGBA16/RGBA32; filters/RSP/RDP/physical timing pending | `build/artifacts/owned/n64_vi_video/` |
| [N64 FPU data](tests/roms-src/n64/fpu_data/README.md) | 131,072 rows / 1,179,648 actual values matched for all eight register/memory transfer instructions in both FR modes; arithmetic and physical timing pending | `build/artifacts/owned/n64_fpu_data/` |
| [N64 integer conversions](tests/roms-src/n64/fpu_integer_conversions/README.md) | 524,288 rows / 2,621,440 captured words matched across CVT.S.W/CVT.D.W and all rounding modes; defined result bits graded | `build/artifacts/owned/n64_fpu_integer_conversions/` |
| [N64 conversion edges](tests/roms-src/n64/fpu_conversion_edges/README.md) | 289,536 rows / 1,447,680 captured words matched across conversion/fixed-rounding functions, exponent/enable/FS/format domains; physical timing pending | `build/artifacts/owned/n64_fpu_conversion_edges/` |
| [N64 arithmetic pairs](tests/roms-src/n64/fpu_arithmetic_pairs/README.md) | 131,072 rows / 786,432 captured words matched across all signed six-bit operand pairs, four binary S/D functions and four rounding modes | `build/artifacts/owned/n64_fpu_arithmetic_pairs/` |
| [N64 arithmetic edges](tests/roms-src/n64/fpu_arithmetic_edges/README.md) | 215,152 rows / 1,290,912 captured words matched across all S/D exponent fields, eight arithmetic/unary functions, special operand pairs and selected full enable/FS products | `build/artifacts/owned/n64_fpu_arithmetic_edges/` |
| [N64 RSP scalar ALU](tests/roms-src/n64/rsp_scalar_alu/README.md) | 704,512 rows / 4,931,584 captured words matched across repeated-byte operand pairs and every shift amount; actual RSP results reach the CPU through DMEM | `build/artifacts/owned/n64_rsp_scalar_alu/` |
| [N64 RSP immediates](tests/roms-src/n64/rsp_scalar_immediates/README.md) | 1,572,864 rows / 11,010,048 captured words matched across all 16-bit immediates, eight opcode forms and three operand patterns | `build/artifacts/owned/n64_rsp_scalar_immediates/` |
| [N64 DP registers](tests/roms-src/n64/dp_registers/README.md) | 262,144 rows / 2,621,440 captured words matched through actual shared CPU/RSP DP control access | `build/artifacts/owned/n64_dp_registers/` |
| [N64 DP fill rectangles](tests/roms-src/n64/dp_fill_rectangles/README.md) | 524,288 rows / 6,815,744 captured words matched across every 16-bit color, both pixel sizes, DRAM/DMEM, whole/center scissor, guards and FullSync IRQ | `build/artifacts/owned/n64_dp_fill_rectangles/` |
| [N64 vector loads](tests/roms-src/n64/rsp_vector_loads/README.md) | 786,432 cases / 19,660,800 actual captured words; independent byte/sign/mask oracle with field and stream controls | `build/artifacts/owned/n64_rsp_vector_loads/` |
| [N64 vector stores](tests/roms-src/n64/rsp_vector_stores/README.md) | 786,432 cases / 25,952,256 actual captured words; independent byte/sign/mask oracle with field and stream controls | `build/artifacts/owned/n64_rsp_vector_stores/` |
| [N64 vector scalar transfers](tests/roms-src/n64/rsp_vector_transfers/README.md) | 1,048,576 cases / 10,485,760 actual captured words; independent byte/sign/mask oracle with field and stream controls | `build/artifacts/owned/n64_rsp_vector_transfers/` |
| [N64 RSP multiply pairs](tests/roms-src/n64/rsp_multiply_pairs/README.md) | 917,504 cases / 16,515,072 actual captured words; seven functions, all selectors and packed repeated-byte pairs | `build/artifacts/owned/n64_rsp_multiply_pairs/` |
| [N64 RSP accumulator edges](tests/roms-src/n64/rsp_accumulate_edges/README.md) | 294,912 cases / 7,372,800 actual captured words; nine functions, raw halfwords and independently captured accumulator setup | `build/artifacts/owned/n64_rsp_accumulate_edges/` |
| [N64 FPU comparison pairs](tests/roms-src/n64/fpu_comparison_pairs/README.md) | 262,144 cases / 1,835,008 actual captured words; sixteen S/D predicates and guest branch markers | `build/artifacts/owned/n64_fpu_comparison_pairs/` |
| [N64 FPU comparison edges](tests/roms-src/n64/fpu_comparison_edges/README.md) | 348,384 cases / 2,438,688 actual captured words; exponents, NaNs/denormals, controls and exception recovery | `build/artifacts/owned/n64_fpu_comparison_edges/` |
| [N64 TMEM transfers](tests/roms-src/n64/rdp_tmem_commands/README.md) | 393,216/393,216 cases / 2,752,512 register/RAM words matched: block/tile/palette consumption, FullSync and guard; 665,884 independent component byte cases; sampling and rasterization pending | `build/artifacts/owned/n64_rdp_tmem_commands/` |
| [N64 RDP tile commands](tests/roms-src/n64/rdp_tile_commands/README.md) | 131,072/131,072 cases / 917,504 register/RAM words matched: tile/image command progression, FullSync IRQ and untouched guard; internal retention separately checked with 10,107,008 descriptor pairs and 1,048,576 DRAM/XBUS streams; TMEM loads and texture rendering pending | `build/artifacts/owned/n64_rdp_tile_commands/` |
| [N64 RDP combiner commands](tests/roms-src/n64/rdp_combiner_commands/README.md) | 131,072 cases / 917,504 actual register/RAM words; command progression, DMEM wrap and real FullSync IRQ; scalar arithmetic component passes (268,173,312 reduced cases); pixel pipeline pending | `build/artifacts/owned/n64_rdp_combiner_commands/` |
| [N64 RDP fill pipeline](tests/roms-src/n64/rdp_fill_pipeline/README.md) | 589,824 rows / 6,488,064 actual words; safe mode flags, CI8/RGBA16/RGBA32, fractional/interlaced scissoring, color/depth clears and guards | `build/artifacts/owned/n64_rdp_fill_pipeline/` |
| [N64 RSP comparison pairs](tests/roms-src/n64/rsp_comparison_pairs/README.md) | 262,144 cases / 4,718,592 actual words; eight comparisons/clips/merge, repeated-byte pairs and nonuniform flags | `build/artifacts/owned/n64_rsp_comparison_pairs/` |
| [N64 RSP comparison edges](tests/roms-src/n64/rsp_comparison_edges/README.md) | 262,144 cases / 4,718,592 actual words; every VS halfword, boundary VT values and accumulator upper preservation | `build/artifacts/owned/n64_rsp_comparison_edges/` |
| [N64 RSP integer pairs](tests/roms-src/n64/rsp_integer_pairs/README.md) | 2,883,584 cases / 51,904,512 actual words; eleven arithmetic/logic functions, every repeated-byte operand pair and all incoming lane flags | `build/artifacts/owned/n64_rsp_integer_pairs/` |
| [N64 RSP integer edges](tests/roms-src/n64/rsp_integer_edges/README.md) | 2,883,584 cases / 51,904,512 actual words; every VS halfword, all incoming lane flags, rotating selectors and boundary VT values | `build/artifacts/owned/n64_rsp_integer_edges/` |
| [N64 RSP divider/move](tests/roms-src/n64/rsp_divider/README.md) | 5,898,240 cases / 123,863,040 actual words; halfword/high-half sweeps, shared pending/output latches, full result and probe captures | `build/artifacts/owned/n64_rsp_divider/` |
| [N64 IPL3 handoff](tests/roms-src/n64/ipl3_handoff/README.md) | 12/12 original startup captures; native retail boot remains unverified; longer Mario trace passes actual buffer clears; RSP reaches a real BREAK with no unsupported instruction at the longer bound; RDP TMEM block/tile/palette transfers now pass; shaded/textured/depth-tested triangles remain blocked | `build/artifacts/owned/n64_ipl3_handoff/` |
| [N64 interrupt gates](tests/roms-src/n64/interrupts/README.md) | 65,536/65,536 cases matched; 524,288 guest values for software/SI/timer, MI/CPU masks and IE/EXL/ERL; pipeline and physical timer timing pending | `build/artifacts/owned/n64_interrupts/` |
| [N64 four-port / netplay guest](tests/roms-src/n64/controller_netplay/README.md) | 16,384 rows / 98,304 words matched across four digital ports; 100-quanta production TCP peers match 600 guest words and complete states; analog/accessories pending | `build/artifacts/owned/n64_controller_netplay/`, `build/artifacts/netplay-n64/` |
| [N64 signed stick input](tests/roms-src/n64/controller_analog_reads/README.md) | 65,536 cached X/Y cases / 1,048,576 captured RAM/register words matched; four-port component axes and two-peer TCP rollback verified; physical calibration/accessories pending | `build/artifacts/owned/n64_controller_analog_reads/` |
| [N64 digital controller](tests/roms-src/n64/controller_serial/README.md) | 16,384/16,384 combinations matched; 131,072 guest values for digital input, identity, neutral second pad and settled SI/MI registers; analog and physical timing pending | `build/artifacts/owned/n64_controller/` |
| [N64 merge memory](tests/roms-src/n64/merge_memory/README.md) | 3,158,016/3,158,016 observations matched; eight instructions, every byte lane, all repeated-byte pairs and full-width boundaries; full multi-byte domains/timing unverified | `build/artifacts/owned/n64_merge_memory/` |
| [N64 RDP command packets](tests/roms-src/n64/rdp_packets/README.md) | 200,704 cases / 1,404,928 actual words; all triangle/rectangle packet sizes, partial END splits, opcode aliases, RDRAM/XBUS; partial-state replay; rendering remains pending | `build/artifacts/owned/n64_rdp_packets/` |
| [N64 fill-cycle triangles](tests/roms-src/n64/rdp_triangle_fill/README.md) | 795,648 cases / 10,343,424 actual framebuffer/guard/register words; halfword patterns, native packet aliases, field/source/geometry matrix; full native graphics pending | `build/artifacts/owned/n64_rdp_triangle_fill/` |
| [N64 one-cycle shade pixels](tests/roms-src/n64/rdp_triangle_shade/README.md) | 792,576 cases / 10,303,488 actual framebuffer/guard/register words; all RG pairs, eight combiner programs, geometry/field/source/coverage matrix; full texturing/blending/depth pending | `build/artifacts/owned/n64_rdp_triangle_shade/` |
| [N64 affine textured pixels](tests/roms-src/n64/rdp_triangle_texture/README.md) | 786,432 cases / 10,223,616 actual framebuffer/guard/register words; packed source words, all fractional pairs and format/size codes under selected textures; point/triangular/average filtering and RDRAM/XBUS; full graphics pending | `build/artifacts/owned/n64_rdp_triangle_texture/` |
| [N64 texture color conversion](tests/roms-src/n64/rdp_color_convert/README.md) | 1,318,912 cases / 17,145,856 actual framebuffer/guard/register words; K0..K3 field/source seeds, all K4/K5 pairs, fractional YUV/RGBA/IA profiles and RDRAM/XBUS; full graphics pending | `build/artifacts/owned/n64_rdp_color_convert/` |
| [N64 perspective texture division](tests/roms-src/n64/rdp_texture_perspective/README.md) | 1,572,864 cases / 20,447,232 actual framebuffer/guard/register words; all W with selected S/T, all S with selected W, RDRAM/XBUS; every scalar S/W pair independently checked; full graphics pending | `build/artifacts/owned/n64_rdp_texture_perspective/` |
| [N64 rejected color history](tests/roms-src/n64/rdp_rejected_history/README.md) | 655,360 cases / 8,519,680 actual pixel/CI8/hidden/guard/register words; rejected RGBA history consumed by following fills | `build/artifacts/owned/n64_rdp_rejected_history/` |
| [N64 one-cycle triangle depth](tests/roms-src/n64/rdp_triangle_depth/README.md) | 524,288 cases / 11,010,048 actual color/depth/hidden/guard/register words; all stored words × four Z modes × two command sources with selected incoming operands; 8,388,608 combined depth/blender decisions; nine executed pipeline faults rejected | `build/artifacts/owned/n64_rdp_triangle_depth/` |
| [N64 primitive depth](tests/roms-src/n64/rdp_primitive_depth/README.md) | 1,048,576 cases / 22,020,096 actual color/depth/hidden/guard/register words; complete individual command fields with selected partners; seven executed component faults rejected; S028 replay and S027 import | `build/artifacts/owned/n64_rdp_primitive_depth/` |
| [N64 ordered dithering](tests/roms-src/n64/rdp_ordered_dither/README.md) | 1,179,648 cases / 15,335,424 actual pixel/hidden/guard/register words; ordered RGB/alpha seeds, saturation, coverage ordering and ignored fields; fifteen executed faults; at that increment, the private stream completed with corrupted video (superseded by the current title-head probe) | `build/artifacts/owned/n64_rdp_ordered_dither/` |
| [N64 copy-cycle transfers](tests/roms-src/n64/rdp_copy_rectangles/README.md) | 1,572,864 cases / 20,447,232 actual framebuffer/hidden/guard/register words; 4/8/16-bit copy, flipped coordinates, alpha and wrapped DMEM; ten executed production faults; private Mario title head observed, gameplay unverified | `build/artifacts/owned/n64_rdp_copy_rectangles/` |
| [N64 one-cycle texture rectangles](tests/roms-src/n64/rdp_texture_rectangles/README.md) | 1,048,576 cases / 13,631,488 actual framebuffer/hidden/guard/register words; both flips, filters and sources; private Peach castle intro observed, gameplay unverified | `build/artifacts/owned/n64_rdp_texture_rectangles/` |
| [N64 cached controller reads](tests/roms-src/n64/controller_cached_reads/README.md) | 16,384 cases / 262,144 actual SI packet/control/interrupt words with one format write and changing input; ten executed production faults; S029 replay; Mario input changes execution but a one-cycle E4 rectangle is blocked | `build/artifacts/owned/n64_controller_cached_reads/` |
| [N64 RGBA16 hidden coverage](tests/roms-src/n64/rdp_hidden_rgba16/README.md) | 655,360 cases / 8,519,680 actual pixel/hidden-metadata/guard/register words; one-cycle RGBA16 blending, shared CI8/16/32 fill metadata, S028 replay | `build/artifacts/owned/n64_rdp_hidden_rgba16/` |
| [N64 hidden RDRAM](tests/roms-src/n64/hidden_rdram/README.md) | 65,536 cases / 589,824 actual RAM/register words; CPU widths and MI EBUS, CPU/masked/cache/DMA metadata gates, S028 production replay; RDP paths graded by separate cartridges | `build/artifacts/owned/n64_hidden_rdram/` |
| [N64 one-cycle blending](tests/roms-src/n64/rdp_one_cycle_blend/README.md) | 655,360 cases / 8,519,680 actual framebuffer/guard/register words; color/alpha seeds, all blender selectors, AA/coverage profiles, RDRAM/XBUS; exhaustive scalar equations and deterministic state replay; depth/full graphics pending | `build/artifacts/owned/n64_rdp_one_cycle_blend/` |
| [N64 doubleword multiply/divide](tests/roms-src/n64/muldiv64/README.md) | 262,623/262,623 defined LO/HI pairs matched; every signed/unsigned byte pair and 16×16 full-width boundaries; 545 undefined inputs observed but unscored | `build/artifacts/owned/n64_muldiv64/` |
| [N64 integer registers](tests/roms-src/n64/integer_registers/README.md) | 28,127/28,127 defined results matched across 20 instructions and selected patterns; 33 undefined division inputs observed but unscored; full core incomplete | `build/artifacts/owned/n64_integer/` |
| [GBA ARM register shifts](tests/roms-src/gba/arm_shifts/README.md) | 65,536/65,536 result/CPSR pairs matched; all low-byte amounts and C/V inputs, 16 selected operands | `build/artifacts/owned/gba_shifts/` |

```bash
python3 tests/roms-src/build.py --original-only --check
ctest --test-dir build -L owned --output-on-failure
```

The NES ROM writes every observed accumulator/status pair to `$401E`; its host
oracle grades the captured file using independent arithmetic expectations. The
ROM itself makes no pass claim. Its small opcode mutation must fail, and missing
or malformed register files are rejected. The SNES ROM streams products,
quotients, and remainders through `$21FE` for its independent host oracle.
The GBA ROM records ARM shift results and CPSR flags through mGBA debug
registers, with an independent host oracle and file-channel boundary controls.
The experimental N64 ROM outputs observed GPR/HI/LO values through an emulator
debug register. Its marked homebrew loader bypasses retail boot; it provides
integer/FPU execution and settled digital SI/PIF controller evidence. Additional
original ROMs grade AI PCM, VI framebuffer and device registers. RSP/RDP game
graphics, RSP audio microcode, analog sticks, serial/pipeline timing and other
interrupt producers remain pending.
The experimental Virtual Boy ROMs record V810 result/PSW pairs, full
multiply/divide results and divide-zero/TRAP/illegal/address-trap exception context and CAXI/port and arithmetic bit-string results through an
emulator debug channel. It runs original reset-vector code; video, audio,
controllers and retail compatibility remain pending.
The experimental DS plugin executes separate ARM9/ARM7 guest programs, capturing
ARM/Thumb integer result/CPSR/source registers, capturing Thumb PC reads and
using main RAM to order their output. Memory transfers capture addresses,
source values, loaded/stored memory and guards alongside CPSR and SP. Adjacent
Thumb long calls capture tagged LR, target PC and ARM9 interworking state.
Stack/block transfers capture all low registers, SP/LR, target PC and memory
words with guards. Twelve digital controls reach the keypad registers; ARM7
also reads X/Y, with a core-supplied controller layout and serialized input latch. Guest
word-output ports serialize the captured values into the same independently
graded files; legacy byte output remains supported and checked for equivalence. Firmware, ARM block transfers,
video, audio and peripherals remain pending. DSi now has a separate
[experimental expanded-memory plugin](cores/dsi/README.md), with most
enhanced hardware still pending.
Original suites retain raw
register output and reports under `build/artifacts/`. Source generators and ROM
images are stored in this repo. CI/CD is disabled for now; run verification locally.

The repository now includes an original Game Boy ROM at
[`tests/roms-src/gb/register_probe/register_probe.gb`](tests/roms-src/gb/register_probe/register_probe.gb).
Its [source generator](tests/roms-src/gb/register_probe/build.py) produces the
same 32 KiB ROM with Python 3. The ROM writes observed WRAM and ALU values to
the Game Boy serial registers; the GB core forwards `SC=$81` writes to
`VELOCE_TEST_OUT`, where the application records `REG`, `CHECK`, and `END` lines.
Run `python3 tests/roms-src/gb/register_probe/test_probe.py build/bin/veloce`
or `ctest --test-dir build -R gb_register_probe --output-on-failure`. The test
also mutates the ROM's WRAM stimulus and requires a scored failure, guarding
against a result path that always reports success. This is two CPU/memory
checks. Most GB subsystems, the remaining NES CPU and peripheral behaviors,
remaining SNES and GBA hardware still need repository-owned register-output ROMs.

### Headless Mode

For local headless execution:

```bash
HEADLESS=1 FRAMES=600 veloce test.nes
DEBUG=1 HEADLESS=1 FRAMES=600 veloce test.nes  # With debug output
```

A verdict never comes from stdout/stderr or a screenshot/framebuffer CRC.
For a machine-readable pass/fail, set `VELOCE_TEST_OUT` and read back the
`VELOCE-RESULT/1` file it writes (implies `HEADLESS=1`; `DEBUG` plays no
part in the verdict):

```bash
VELOCE_TEST_OUT=out.result FRAMES=600 veloce test.nes
cat out.result
```

Each core that has adopted the channel exposes it over a small,
hardware-inert ROM-facing port (e.g. the NES core's `$401E`/`$401F`, see
[cores/nes/README.md](cores/nes/README.md#result-detection)) plus adapters
for existing unmodified test ROMs; the full wire format, the plugin ABI
hook cores implement it through, and the per-console port table are in
[docs/testing/VELOCE-RESULT.md](docs/testing/VELOCE-RESULT.md).

Test ROMs whose upstream binary cannot report a verdict are rebuilt from source
so that they write to the port. Each is recorded as `rom_variant: veloce` in a
core's `test_config.json`. `tests/roms-src/build.py` does the rebuild:

- it fetches each upstream at a pinned commit;
- it applies the committed patches in `tests/roms-src/<console>/<suite>/`;
- it builds inside the `tools/rom-toolchain` Docker image;
- it records every ROM's sha256 and tool versions in `tests/roms-src/rom_manifest.json`.

Supplemental upstream ROMs are written to ignored `build/roms/<console>/`.
Repository-owned original ROM sources and reproducible images are archived in
`tests/roms-src/`. The harness refuses to score a `veloce` ROM whose hash is not in the
manifest, and until the ROM has been built the test reports SKIP.

```bash
python3 tests/roms-src/build.py nes/read_joy3          # one suite
python3 tests/roms-src/build.py --check nes/read_joy3  # verify against the manifest
```

Each suite's `PROVENANCE.md` records where every expected value comes from.

The NES result-channel branch is now integrated with the original capture path.
Its file adapter observes fresh CPU writes; eight original production fixtures
verify signature/status and port precedence. The 524,288-case original ADC/SBC
corpus and production TCP/rollback gates pass. Supplemental ROM rebuilds do not
establish a whole-core hardware accuracy percentage. See the [integration
evidence](cores/nes/README.md#result-channel-branch-integration-2026-10-11).

The Game Boy result-channel branch is also integrated with the original
register corpus: 8,388,608 arithmetic and 73,728 unary observations pass, with
actual production adapter captures saved to files. Delivery remains 40%;
whole-platform hardware accuracy remains unmeasured. See the [GB integration
evidence](cores/gb/README.md#result-channel-branch-integration-2026-10-11).

The GBA result-channel branch is integrated: 65,536 original result/CPSR pairs
match the host oracle, and nine production fixtures capture ARM/Thumb and mGBA
adapter behavior. Both original emitter dialects repeat exactly with a deliberate
failure. Delivery remains 40%; timing and full compatibility remain unverified.
See the [GBA integration evidence](cores/gba/README.md#result-channel-branch-integration-2026-10-11).

The SNES result-channel branch is integrated: 68,864 settled math pairs and
504,832 timed captures pass independent grading. Fresh opcode ROMs report
1,609/1,610 S-CPU checks (FAIL) and 1,368/1,368 SPC700 checks (PASS), with exact
repeat captures. Delivery stays 40%; full hardware timing remains unverified.
See the [SNES integration evidence](cores/snes/README.md#result-channel-branch-integration-2026-10-11).

The remaining NES audio timing branch is reconciled. The eight file-reporting
APU ROMs now pass (previously five); a new original cartridge independently
grades 160 actual status reads around NTSC frame IRQ boundaries and rejects
an executed mutant. NES stays 50%; complete APU timing remains unfinished.
See the [timing evidence](cores/nes/README.md#frame-irq-timing-branch-reconciliation-2026-10-11).

Nine SNES CPU stack boundary defects are repaired. A new original ROM grades
5,632 cases from actual register/RAM files, including every emulation stack
pointer value, with an executed negative control and exact repeat captures.
The decimal SBC repair now brings the measured CPU catalog to **1,610/1,610**.
An original ROM grades **8,193 actual accumulator/flag pairs**, rejects an
executed mutant and repeats exactly. SNES stays **40%**; full hardware accuracy
remains unfinished.
See the [stack evidence](cores/snes/README.md#cpu-stack-boundaries-2026-10-11)
and [decimal subtraction evidence](cores/snes/README.md#decimal-sbc-2026-10-11).

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
        n64/                  Experimental N64 integer homebrew emulator
        switch/               Experimental AArch64/uncompressed NSO0 process core
        vb/                   Experimental V810 integer homebrew emulator
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
- [hw-test](https://codeberg.org/nba-emu/hw-test) - GBA hardware timing tests by fleroviux (NanoBoyAdvance), rebuilt with a result-channel patch
- [snes-tests](https://github.com/gilyon/snes-tests) - SNES emulator test ROMs by gilyon
- [stb_image_write](https://github.com/nothings/stb) - Screenshot encoding
- Game Boy test sources: [Blargg](https://github.com/retrio/gb-test-roms), [Mooneye](https://github.com/Gekkio/mooneye-test-suite), [SameSuite](https://github.com/LIJI32/SameSuite), and the [c-sp collection](https://github.com/c-sp/gameboy-test-roms)
- Additional supplemental tests: [mGBA suite](https://github.com/mgba-emu/suite), [alyosha GBA tests](https://github.com/alyosha-tas/gba-tests), [PeterLemon SNES](https://github.com/PeterLemon/SNES), and [holy-mapperel](https://github.com/pinobatch/holy-mapperel)
- [Project64](https://www.pj64-emu.com/) - Plugin architecture inspiration

## License

MIT License - See [LICENSE](LICENSE) file.

## Branch reconciliation

The older README refresh has been reconciled with current implementation and
executed evidence. Its outdated four-core, missing-file-channel and missing-
netplay status was superseded. See the [branch integration record](docs/BRANCH-INTEGRATION.md).
CI/CD remains disabled; run the documented gates locally.
