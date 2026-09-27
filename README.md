# Veloce

A plugin-based, cross-platform emulator framework for NES, SNES, Game Boy / Color, and Game Boy Advance, built toward speedrunning and TAS use.

![Version](https://img.shields.io/badge/version-0.1.0-blue)
![Language](https://img.shields.io/badge/C%2B%2B-17-blue)
![Build](https://img.shields.io/badge/build-CMake%203.16%2B-informational)
![License](https://img.shields.io/badge/license-MIT-green)

*Veloce* is Italian for "fast".

## Overview

Veloce is a single frontend (`veloce`, built on SDL2, Dear ImGui, and OpenGL) that loads emulator cores and auxiliary components as shared-library plugins, in the spirit of Project64. Each console lives in its own core under [`cores/`](cores/), and every plugin talks to the application only through the C++ interfaces in [`include/emu/`](include/emu/).

The project prioritizes accuracy where speedrunners and TAS authors notice it: timing-sensitive behavior, deterministic execution, and a measured, reproducible accuracy number per core rather than a hand estimate. A shared test framework scores accuracy from verdicts that test ROMs write to a file, never from eyeballing screenshots (see [Testing](#testing)).

Veloce is at version 0.1.0 and under active development. The four cores run commercial software to varying degrees, and the speedrun timer works. The TAS, netplay, and deterministic-savestate features the architecture is designed around are **not usable yet**. [Project status](#project-status) lists exactly what works today.

## Project status

This table reflects the code on `main`, including the September 2026 cross-platform audit. Items marked planned have interfaces or partial implementations in the tree but do not work end to end.

| Area | Status | Detail |
|------|--------|--------|
| Emulator cores | Working, maturity varies | Per-console status in [Supported systems](#supported-systems) |
| Speedrun timer and splits | Working | Built-in game plugin: live timer, splits, personal best, gold segments, sum of best. See [plugins/speedrun_tools_default](plugins/speedrun_tools_default/README.md) |
| Save states (10 slots) | Partial | Save/load works for casual play, but no core serializes its complete machine state yet, so a loaded state is not guaranteed to replay identically. The container also carries no core identity or per-core format version |
| TAS (movie record/playback, greenzone) | Planned | `ITASPlugin` and `plugins/tas_default` exist, but the frame loop never drives the TAS plugin, so no TAS feature is reachable. Also blocked on deterministic save states |
| Input and audio plugins | Planned | `IInputPlugin`/`IAudioPlugin` and default implementations exist, but the frame loop uses the application's built-in input and audio managers instead |
| Netplay | Planned (experimental UI only) | The default netplay plugin has a panel (Ctrl+N) but no network transport, no real input exchange, and no rollback |
| Multiple controllers | Not supported | The per-frame input state carries a single controller |
| Speed control | Partial | Only 100% speed is paced correctly; other multipliers are not applied |
| Test-result channel (VELOCE-RESULT/1) | Partial | The application-side sink is implemented and unit-tested; no core emits to it yet, so suites still use the legacy detectors. See [Testing](#testing) |

## Supported systems

Per-core accuracy numbers change as tests and fixes land, so they live in one place: [COMPLETENESS.md](COMPLETENESS.md). The status lines below are qualitative.

| System | Core | Status |
|--------|------|--------|
| NES / Famicom | [cores/nes](cores/nes/README.md) | Most mature core, with the strongest verified test coverage and a broad mapper set. Remaining accuracy debt is concentrated in interrupt/DMA cycle timing and APU frame-counter timing. [Scorecard](COMPLETENESS.md#nes) |
| Super Nintendo | [cores/snes](cores/snes/README.md) | 65C816, PPU (including Mode 7), SPC700, and S-DSP implemented. No enhancement chips (SuperFX, SA-1, DSP-n, and others). Very little is verified end to end yet, because most SNES test ROMs report on screen and have no scorable result signal. [Scorecard](COMPLETENESS.md#snes) |
| Game Boy / Color | [cores/gb](cores/gb/README.md) | DMG and CGB; no-MBC, MBC1, MBC3, and MBC5 cartridges. The Blargg subset is verified; the large Mooneye-family block stays unverified until the core emits its result signal. [Scorecard](COMPLETENESS.md#game-boy) |
| Game Boy Advance | [cores/gba](cores/gba/README.md) | ARM7TDMI with HLE BIOS; SRAM, Flash, and EEPROM saves. CPU, memory, BIOS, and most save tests pass; cycle-accurate DMA/IRQ/timer timing mostly fails. [Scorecard](COMPLETENESS.md#gba) |

## Quick start (Linux)

```bash
sudo apt-get install -y build-essential git python3 libgl-dev libsdl2-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/bin/veloce path/to/game.nes       # run from the repository root
```

If `cmake` is not on your `PATH`, see [Getting CMake](#getting-cmake).

## Building

Veloce is C++17 and needs CMake 3.16 or newer. CMake `FetchContent` downloads Dear ImGui (v1.90.1) and nlohmann/json (v3.11.3) at configure time, so the first configure needs network access and `git`. CMake uses the system SDL2 if it can find one; otherwise it fetches SDL 2.28.5 and builds it statically.

### Linux

| Package (Debian/Ubuntu) | Why |
|-------------------------|-----|
| `build-essential` | C++17 compiler and linker |
| `git` | `FetchContent` clones |
| `libgl-dev` | OpenGL (required) |
| `libsdl2-dev` | Recommended. Without it, SDL2 is built from source and needs your distribution's X11/Wayland development headers to open a window |
| `python3` | Required at configure time while the test framework is enabled (the default). Configure with `-DVELOCE_BUILD_TESTS=OFF` to build without it |

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Use `-DCMAKE_BUILD_TYPE=Debug` for development; Release runs much faster.

#### Getting CMake

Any CMake 3.16+ works: the distribution package, the upstream installer, or the PyPI wheel. The PyPI wheel is convenient when you cannot install system packages:

```bash
python3 -m pip install --user cmake      # or inside a pyenv / virtualenv
cmake --version
```

CMake records the absolute path of the `cmake` binary that configured a build tree (for example `/usr/bin/cmake`) in the generated `build/Makefile`. If running `make` inside `build/` fails with `cmake: not found`, that `cmake` has moved or was never at that path. Re-run `cmake -B build` with the `cmake` you have, and prefer `cmake --build build` over calling `make` directly.

### Build output

| Path | Contents |
|------|----------|
| `build/bin/veloce` | The application |
| `build/bin/cores/` | Emulator core plugins: `nes`, `snes`, `gb`, `gba` |
| `build/bin/plugins/` | Auxiliary plugins: audio, input, TAS, netplay, and the built-in timer |
| `build/bin/test_file_sink` | C++ unit test for the test-result sink (built when tests are enabled) |

### macOS

```bash
xcode-select --install
brew install cmake sdl2
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The CMake files contain a macOS configuration, but CI does not build it; reports and fixes are welcome.

### Windows

Requires Visual Studio 2022 or 2026 with the "Desktop development with C++" workload, which bundles CMake and Ninja. Only the MSVC toolchain is supported, and CI does not build Windows.

- **IDE:** open the repository root with **Open a local folder**. The committed [`CMakeSettings.json`](CMakeSettings.json) provides `x64-Debug` and `x64-Release` Ninja configurations; binaries land in `out\build\<config>\bin\`.
- **Command line** (Developer PowerShell for VS, so `cl.exe` and Ninja are on `PATH`):

  ```powershell
  cmake -S . -B out\build\x64-Release -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build out\build\x64-Release
  ```

## Running

```bash
veloce [OPTIONS] [ROM_FILE]
```

| Option | Effect |
|--------|--------|
| `-d`, `--debug` | Open the debug panel (CPU/PPU state, memory viewer) |
| `-h`, `--help` | Print usage, including the environment variables below |
| `-v`, `--version` | Print the version |

ROM formats: `.nes` (NES), `.sfc` / `.smc` (SNES), `.gb` / `.gbc` (Game Boy), `.gba` (GBA). With no ROM argument, the GUI opens and you can load a ROM from the menu.

**Run from the repository root or from `build/`.** The application searches for plugins relative to the current working directory (`./cores`, `./bin/cores`, `./build/bin/cores`, and the matching `plugins/` paths), not relative to the executable. It also creates `config/`, `saves/`, `savestates/`, and `screenshots/` in the current working directory.

Known issue: `--help`, `--version`, and unrecognized options print their output and then crash (exit code 139) instead of exiting cleanly.

### Hotkeys

| Action | Key |
|--------|-----|
| Pause / resume | Escape |
| Frame advance (while paused) | F |
| Reset | Ctrl+R |
| Save state to slot 1-10 | F1-F10 |
| Load state from slot 1-10 | Shift+F1-F10 |
| Screenshot | Print Screen |
| Fullscreen | F11 |
| Debug panel | F12 |
| Netplay panel (experimental) | Ctrl+N |

### Default controls

Bindings are per platform and can be changed in the input configuration panel. SDL game controllers are supported with hot-plugging.

| Button | Keyboard | Game controller |
|--------|----------|-----------------|
| D-pad | Arrow keys | D-pad |
| A / B | Z / X | A / B |
| X / Y | A / S | X / Y |
| L / R | Q / W | Shoulder buttons |
| Start | Enter | Start |
| Select | Right Shift | Back |

### Headless mode and environment variables

Headless mode runs a ROM for a fixed number of frames with no window, audio, or GUI. The test harness drives the emulator this way.

```bash
# Run 600 frames, hold Start (0x40) on frames 200-201, screenshot frame 600
HEADLESS=1 FRAMES=600 INPUT="200:40,202:0" SAVE_SCREENSHOT=600 ./build/bin/veloce test.nes

# Test session: write the ROM's result channel to a file (implies HEADLESS=1)
VELOCE_TEST_OUT=out.result FRAMES=900 ./build/bin/veloce test.nes
```

| Variable | Effect |
|----------|--------|
| `HEADLESS=1` | Run without a window. Requires a ROM argument |
| `FRAMES=N` | Frame budget in headless mode (default 600) |
| `INPUT=frame:mask,...` | Deterministic player-1 input schedule, headless only. `mask` is hex; each entry holds until the next. Bits: A `1`, B `2`, X `4`, Y `8`, L `10`, R `20`, Start `40`, Select `80`, Up `100`, Down `200`, Left `400`, Right `800` |
| `SAVE_SCREENSHOT=N` | Save a PNG at frame N. A non-numeric value is an output path, written at exit |
| `TRACE=1` | NES only: emit a nestest-format CPU trace to stdout, or to `TRACE_FILE=<path>` |
| `DEBUG=1` | Verbose core diagnostics on stdout/stderr. Meant for humans; the legacy stdout detectors also rely on it |
| `VELOCE_TEST_OUT=<path>` | Write the VELOCE-RESULT/1 test-result file to `<path>` |
| `VELOCE_TEST_EXIT=0\|1` | Stop at the result terminator (default 1) |
| `VELOCE_TEST_RESETS=N` | Maximum ROM-requested resets (default 3) |

[docs/testing/VELOCE-RESULT.md](docs/testing/VELOCE-RESULT.md) specifies the exact semantics of the `VELOCE_TEST_*` variables, the result-file format, and the per-console ROM ports.

## Testing

Veloce measures accuracy instead of asserting it. [TESTING.md](TESTING.md) states the principles in full:

- **Verdicts come from files, not pixels.** A test ROM writes what it observed to a result file in the [VELOCE-RESULT/1](docs/testing/VELOCE-RESULT.md) format, and the harness scores that file. Screenshots, framebuffer CRCs, and scraped stdout are never a verdict.
- **Every number is reproducible.** Each test contributes a weighted credit, and `tests/veloce_testkit` computes the per-core score from data.
- **Unknowns are reported, not hidden.** A test that runs without a readable verdict counts as unverified and is excluded from the score; it is never counted as a pass.

**Current state:** the application writes the result file, and a C++ unit test covers it, but no core implements the test-sink ABI yet (the file's header line shows an empty `channels=`). Until each core migrates, suites fall back to the legacy `memory`/`serial` stdout detectors and the pending `screenshot-crc` references described in TESTING.md.

```bash
ctest --test-dir build -L fast        # fast gates: testkit selftest, config validation, sink unit test
python3 tests/run_all.py              # all per-console accuracy suites (clones test ROMs on first run)
```

- Per-console runners, CI gates, and the scoring formula: [TESTING.md](TESTING.md)
- Current numbers: [COMPLETENESS.md](COMPLETENESS.md)
- Framework internals: [tests/README.md](tests/README.md)
- Reproducible from-source test-ROM builds: [`tests/roms-src/build.py`](tests/roms-src/build.py), using the pinned assembler image in [`tools/rom-toolchain/`](tools/rom-toolchain/)

## Architecture

The application owns a single frame loop. Each frame it reads input from its built-in input manager, calls the active core's `run_frame`, feeds audio to its built-in audio manager, and updates game plugins. Other plugin types load and can be configured but, as noted under [Project status](#project-status), the loop does not drive them yet.

```
veloce (src/core, src/gui)
  |
  +-- Frame loop ........ InputManager -> IEmulatorPlugin::run_frame -> AudioManager
  |                       -> game plugins updated each frame
  +-- SavestateManager .. 10 slots per ROM
  +-- TestFileSink ...... VELOCE-RESULT/1 writer (headless test sessions)
  |
  +-- PluginManager / PluginRegistry (ABI version checked at load)
        +-- Emulator cores (IEmulatorPlugin)   nes, snes, gb, gba   driven
        +-- Game plugins   (IGamePlugin)       built-in timer        driven
        +-- Netplay        (INetplayPlugin)    default               UI only
        +-- TAS            (ITASPlugin)        default               not driven
        +-- Input          (IInputPlugin)      default               not driven
        +-- Audio          (IAudioPlugin)      default               not driven
```

| Header in [`include/emu/`](include/emu/) | Interface | Implemented by |
|------------------------------------------|-----------|----------------|
| `emulator_plugin.hpp` | `IEmulatorPlugin`: ROM load, `run_frame`, save states, test sink (ABI v2) | [NES](cores/nes/README.md), [SNES](cores/snes/README.md), [GB](cores/gb/README.md), [GBA](cores/gba/README.md) |
| `game_plugin.hpp` | `IGamePlugin`, `IGameHost`: timer, splits, auto-splitters, each with its own ImGui panel | [Built-in timer](plugins/speedrun_tools_default/README.md); shared helpers in `plugins/game_plugin_common` |
| `tas_plugin.hpp` | `ITASPlugin` | [Default TAS plugin](plugins/tas_default/README.md) (not wired in) |
| `input_plugin.hpp` | `IInputPlugin` | [Default input plugin](plugins/input_default/README.md) (not wired in) |
| `audio_plugin.hpp` | `IAudioPlugin` | [Default audio plugin](plugins/audio_default/README.md) (not wired in) |
| `netplay_plugin.hpp` | `INetplayPlugin` | `plugins/netplay_default` (no transport) |
| `video_plugin.hpp` | `IVideoPlugin` | None |
| `speedrun_tools_plugin.hpp` | `ISpeedrunToolsPlugin` | None; superseded by `IGamePlugin` |
| `input_types.hpp`, `controller_layout.hpp`, `plugin_types.hpp` | Shared types | - |

The plugin READMEs describe each plugin's intended feature set. For plugins marked not wired in, those features are not reachable from the application yet.

## Project structure

```
.
+-- include/emu/            Plugin interfaces (the ABI)
+-- src/
|   +-- core/               Application, frame loop, plugin manager, savestates, test sink
|   +-- gui/                Dear ImGui panels (menu, game view, debug, input, plugin and path config)
+-- cores/
|   +-- nes/ snes/ gb/ gba/
|       +-- src/            Core implementation
|       +-- tests/          test_config.json and runner for that console
+-- plugins/
|   +-- audio_default/ input_default/ tas_default/ netplay_default/
|   +-- speedrun_tools_default/   Built-in timer (IGamePlugin)
|   +-- game_plugin_common/       Shared code for game plugins
+-- tests/
|   +-- veloce_testkit/     Shared detection and scoring framework (Python)
|   +-- cpp/                C++ unit tests
|   +-- roms-src/           Reproducible test-ROM build recipes and manifest
|   +-- run_all.py          Cross-console accuracy roll-up
+-- tools/rom-toolchain/    Pinned assembler Docker image for roms-src
+-- docs/testing/           VELOCE-RESULT/1 specification and reference transcripts
+-- assets/                 Runtime assets, copied next to the binary
+-- external/               Vendored single-header libraries (stb_image_write)
+-- .githooks/              Tracked pre-push hook
+-- .github/workflows/      CI (self-hosted runners only)
```

## Contributing

All changes go through a branch and a pull request; nothing is committed to `main` directly. After cloning, enable the tracked pre-push hook, which refuses pushes to `main`:

```bash
git config core.hooksPath .githooks
```

CI runs only on self-hosted runners, never GitHub-hosted ones, so run the fast gates, plus the accuracy suite of every console you touch, locally before merging. [CONTRIBUTING.md](CONTRIBUTING.md) covers branch naming, commit conventions, the multi-agent workflow, and the exact pre-merge gates.

## Acknowledgements

Veloce builds on the work of many people.

**Libraries and inspiration**

- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut and contributors: immediate-mode GUI
- [SDL2](https://www.libsdl.org/): windowing, input, and audio
- [nlohmann/json](https://github.com/nlohmann/json) by Niels Lohmann: JSON
- [stb_image_write](https://github.com/nothings/stb) by Sean Barrett: screenshot encoding
- [Project64](https://www.pj64-emu.com/): the plugin architecture that inspired Veloce's

**Test ROMs.** The accuracy framework exists because these authors published hardware-verified tests.

- NES: [nes-test-roms](https://github.com/christopherpow/nes-test-roms) (collection maintained by christopherpow, with tests by Shay Green (blargg), Joel Yliluoma, and others), [nestest](https://www.qmtpro.com/~nes/misc/nestest.txt) by kevtris, [holy-mapperel](https://github.com/pinobatch/holy-mapperel) by Damian Yerrick (pinobatch)
- SNES: [snes-tests](https://github.com/gilyon/snes-tests) by gilyon, [PeterLemon/SNES](https://github.com/PeterLemon/SNES) by krom (Peter Lemon), blargg's SNES tests via [higan/snes-test-roms](https://gitlab.com/higan/snes-test-roms)
- Game Boy: [game-boy-test-roms](https://github.com/c-sp/gameboy-test-roms) (bundle by c-sp), blargg's [gb-test-roms](https://github.com/retrio/gb-test-roms), [mooneye-test-suite](https://github.com/Gekkio/mooneye-test-suite) by Gekkio, [SameSuite](https://github.com/LIJI32/SameSuite) by LIJI32, wilbertpol's [mooneye-gb](https://github.com/wilbertpol/mooneye-gb) tests, and [dmg-acid2](https://github.com/mattcurrie/dmg-acid2), [cgb-acid2](https://github.com/mattcurrie/cgb-acid2), and [mealybug-tearoom-tests](https://github.com/mattcurrie/mealybug-tearoom-tests) by Matt Currie
- GBA: [gba-tests](https://github.com/jsmolka/gba-tests) by jsmolka, [gba-tests](https://github.com/alyosha-tas/gba-tests) by alyosha-tas, [NanoBoyAdvance hw-test](https://github.com/nba-emu/hw-test), [FuzzARM](https://github.com/DenSinH/FuzzARM) by DenSinH, [armwrestler-gba-fixed](https://github.com/destoer/armwrestler-gba-fixed) by destoer, and the [mGBA test suite](https://github.com/mgba-emu/suite)

## License

MIT License. See [LICENSE](LICENSE).
