# Default TAS Plugin

Tool-assisted speedrun movie editor and player for Veloce.

## Overview

The Default TAS Plugin implements movie operations, frame editing and seek snapshots. Its backend is tested through a deterministic host; application routing and editor controls remain unfinished.

## Features

- Movie recording and playback
- Frame-by-frame editing (insert, delete, modify)
- Greenzone (automatic savestate snapshots for seeking)
- Undo/redo with 100 levels of history
- FM2 movie import (FCEUX format)
- Selection and clipboard operations
- Frame markers with descriptions
- Rerecord counting

## API

The plugin implements the ITASPlugin interface defined in include/emu/tas_plugin.hpp.

### Plugin Information

| Property | Value |
|----------|-------|
| Name | TAS Editor |
| Version | 2.0.0 |
| Author | Veloce Team |
| File Formats | .fm2, .tas |

### TAS Modes

| Mode | Description |
|------|-------------|
| Stopped | Movie inactive; live input passes through |
| Recording | Recording new inputs |
| Playing | Playing back movie |
| ReadOnly | Playing, cannot modify |
| ReadWrite | Playing with editing enabled |

### Key Methods

**Movie Operations:**
- new_movie(filename, from_savestate) - Create new movie
- open_movie(filename) - Load existing movie
- save_movie() / save_movie_as(filename) - Save movie
- close_movie() - Close current movie

**Playback Control:**
- start_recording() / stop_recording() - Recording control
- start_playback() / stop_playback() - Playback control
- on_frame(controller) - Called each frame, returns input

**Frame Editing:**
- get_frame(frame) / set_frame(frame, data) - Access frame data
- insert_frame(after_frame) - Insert blank frame
- delete_frame(frame) - Remove frame
- clear_input(start, end) - Clear input range

**Undo/Redo:**
- undo() / redo() - Undo/redo operations
- can_undo() / can_redo() - Check availability

**Greenzone:**
- has_greenzone_at(frame) - Check for savestate
- seek_to_frame(frame) - Seek using greenzone
- invalidate_greenzone(from_frame) - Invalidate after edit

**Selection:**
- set_selection(start, end) - Set selection range
- copy_selection() / cut_selection() - Clipboard operations
- paste_at(frame) - Paste from clipboard

**Markers:**
- add_marker(frame, description) - Add frame marker
- remove_marker(frame) - Remove marker
- get_marker_count/frame/description() - Access markers

## Movie files

New `.tas` files use **TAS2**, with explicit little-endian fields rather than
compiler struct padding. Reads check bounds, complete payloads, string
termination, canonical frame numbers, state flags and slot numbers. Failed
loads preserve the previous movie. Same-build legacy TAS1 files can be imported;
TAS1 portability across compilers/architectures is not promised. Imported FM2
files require Save As to a native filename; saving cannot overwrite the FM2
source with native binary data.

[TAS2 layout and limits](../../../docs/testing/TAS-MOVIES.md) describes the
1,000,000-frame, 64 MiB state and 100 MiB file limits. Native state bytes still
belong to the relevant core/build; TAS2 file portability does not make those
bytes portable across core state-format versions.

### FM2 import

Import follows the [FCEUX format specification](https://fceux.com/web/help/fm2.html):
ASCII or binary logs, two gamepad/empty ports or Four Score, empty expansion
port, and soft-reset commands. Controller bytes map into frontend virtual-button
masks. All 256×256 two-port combinations are checked in both log encodings;
four-port tests cover every byte value per port under stated patterns. Other
commands, Zapper/expansion devices and embedded FCEUX states are rejected.
Declared log length is honored. MD5/GUID/timing metadata is not used to validate
native playback; native CRC32 checks cannot validate FM2 ROM identity. Native playback checks
nonzero CRC32 and nonempty platform metadata before resetting/restoring the host.

## Frame lifecycle and seeking

**TAS ABI 2** adds `on_frame_completed()` and synchronous
`ITASHost::run_replay_frame()`. Rebuild TAS plugins/hosts against this header;
ABI1 libraries are rejected before any factory/vtable invocation. Other plugin
ABI versions are unchanged.

`on_frame()` samples one movie frame for every controller, independently of
query order. Repeated reads return that sample; they do not advance recording
or playback. Commit once with `on_frame_completed()` after actual emulation.
Do not commit a waiting/retried frame. Stopped mode passes through live input.

Greenzone snapshots are saved after every 60 completed movie frames. Boundary N
represents state after N inputs, immediately before input N. Editing input N
keeps boundary N and invalidates later boundaries; undo/redo invalidate cached
states. Read-only mode blocks frame/clipboard/history edits.

Seek restores the nearest earlier snapshot or the initial state, then calls the
host's synchronous replay method for each required frame. It does not queue GUI
frame-advance requests or count them as executed frames. Seeking to the terminal
movie boundary is allowed. Failed replay attempts restore the saved host state;
a host that also rejects restoration cannot provide that recovery guarantee.
Successful seeking pauses at the requested boundary and does not count as a
rerecord. Reset/state events must be executed by the replay host.

## Verified backend evidence

- **131,072 actual imported FM2 records** enumerate every two-port byte pair
  in ASCII and binary, with soft-reset flags. **512 Four Score records** cover
  every byte value on each port using stated patterns, not all four-port tuples.
- Independently packed TAS2 bytes include high input bits, start-state data,
  reset/save flags and signed slot fields. All **1,786 truncation prefixes**,
  trailing bytes and malformed fields are rejected. Malformed FM2 payloads and
  unsupported devices/commands fail.
- A deterministic four-port host executes **121 committed frames**. Repeated
  and shuffled controller reads retain one sample; exact snapshot/replay
  boundaries (0/59/60/61/120/121), failed replay restoration, playback, state
  starts, editing/history and noncanonical legacy bool fields are checked.
- A real ABI1 fixture is rejected before its factory runs; the actual ABI2
  default library is discovered successfully.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -R '^(tas_movie_format_test|tas_runtime_test|tas_registry_test)$' --output-on-failure
```

Artifacts remain under `build/artifacts/tas_formats/` and
`build/artifacts/tas_runtime/`. These verify movie data and host sequencing;
they are not native guest register/RAM hardware accuracy ROMs. Frontend/editor
routing, native core replay/state events, cross-core state compatibility, full
FM2 peripherals, movie identity/timing validation and physical hardware remain
unfinished or unverified. No core completion milestone is added.

## Building

Built automatically as part of Veloce. See main README for build instructions.

## Source Files

| File | Purpose |
|------|---------|
| src/default_tas_plugin.cpp | Movie operations and frame lifecycle |
| src/movie_file.cpp | Bounded native and FM2 file parsing |
| CMakeLists.txt | Build configuration |

