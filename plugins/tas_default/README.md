# Default TAS Plugin

Tool-assisted speedrun movie editor and player for Veloce.

## Overview

The Default TAS Plugin implements movie operations, frame editing and seek snapshots. Its backend is tested through a deterministic host, and production application routing is verified with an original native NES controller probe.

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
they are not native guest register/RAM hardware accuracy ROMs. Cross-core state
compatibility, full FM2 peripherals, complete timing identity validation and
physical hardware remain unfinished or unverified. No core completion milestone
is added.

## Frontend integration

**Tools → Movie editor** exposes new/open/save-as, playback, pause/step, seek,
four controller masks, reset/save events, insertion/deletion and undo/redo.
Movie controls require a core that saves a nonempty native state.
The selected ABI 2 plugin receives a live host before activation; it shuts down
before that host is destroyed. Failed initialization preserves the prior plugin.
The application samples input once per logical frame and commits the movie once
after actual execution. Unsupported nonzero controller ports or failed save events
pause and stop playback with a notification. Netplay excludes movie execution.
Manual reset/state load stops movie recording/playback; ROM changes close movies.

Reset-start playback restores the native state captured at ROM load, including
any battery data loaded then. It does not use a soft reset to discard previous
movie state. A movie reset event performs the core's soft reset before its input;
a save event saves slot 0–9 before executing that frame. Replay runs synchronously
and suppresses streaming/batch audio; seeking clears buffered device audio.
Seek replay ends an active register capture so repeated observations cannot be
silently mixed into it. Native states retain core/build compatibility limits.

`tas_frontend_test` runs the production Application, GUI and selected default
input/audio/TAS plugins with the original `netplay_probe.nes`:

- **121 frames** on two ports: ordinary execution, recording and reopened playback
  match byte-for-byte native state and actual cartridge RAM at `$12/$13`.
- Seeks at **0, 59, 60, 61, 120, 121**, plus a 30-frame state-start movie,
  restore the ordinary boundary state. Device audio is empty after seeking.
- A **65-frame** movie executes a reset at frame 20 and state save at frame 40;
  playback matches ordinary state, the actual slot contains the pre-input
  boundary, and seeking across reset reproduces the target boundary.
- An executed one-bit input mutant changes the cartridge RAM observations.
  Manual reset stops recording, and the real editor window renders under Xvfb.

```sh
ctest --test-dir build -R '^tas_frontend_test$' --output-on-failure
```

Artifacts: `build/artifacts/tas_frontend/ram-observations.txt`, native movie/state
files and `frontend.log`. This verifies native input/replay/state routing for one
NES profile; it does not independently establish hardware timing accuracy.
Other cores, physical controllers/audio and exact video previews at snapshot-only
seek boundaries remain unverified. Some native states omit output framebuffer
pixels; the displayed image may remain from the prior frame until execution.
Clipboard/selection and markers remain backend APIs without editor controls;
Lua scripting remains unsupported. Core milestone percentages are unchanged.

## Building

Built automatically as part of Veloce. See main README for build instructions.

## Source Files

| File | Purpose |
|------|---------|
| src/default_tas_plugin.cpp | Movie operations and frame lifecycle |
| src/movie_file.cpp | Bounded native and FM2 file parsing |
| CMakeLists.txt | Build configuration |

