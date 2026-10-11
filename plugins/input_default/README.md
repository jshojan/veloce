# Default Input Plugin

Keyboard and gamepad input handler for Veloce with recording and turbo support.

## Overview

The Default Input Plugin handles all controller input for Veloce. It polls keyboard and USB gamepads, maps physical inputs to virtual buttons, and provides input recording/playback for TAS workflows.

## Features

- Keyboard input with configurable bindings
- USB gamepad support with hot-plugging
- Per-controller button mappings
- Turbo/autofire functionality
- Input recording and playback
- Input override for TAS editing
- Controller layout awareness from emulator cores

## API

The plugin implements the IInputPlugin interface defined in include/emu/input_plugin.hpp.

### Plugin Information

| Property | Value |
|----------|-------|
| Name | Default Input |
| Version | 1.0.0 |
| Author | Veloce Team |
| Recording Support | Yes |
| Playback Support | Yes |
| Turbo Support | Yes |

### Key Methods

- initialize(IInputHost* host) - Initialize with host interface
- begin_frame() - Called at start of each frame
- get_input_state(int controller) - Returns button bitmask for controller
- get_binding/set_binding() - Configure input mappings
- set_turbo_enabled/get_turbo_rate() - Turbo configuration
- start_recording/stop_recording/get_recording() - Input recording
- start_playback/stop_playback() - Input playback

## Default Bindings

### Keyboard (Controller 0)

| Button | Key |
|--------|-----|
| Up | Arrow Up |
| Down | Arrow Down |
| Left | Arrow Left |
| Right | Arrow Right |
| A | Z |
| B | X |
| Start | Enter |
| Select | Right Shift |

### Gamepad

Gamepads are automatically detected and use standard SDL mappings.

## Turbo System

Turbo toggles button state at a configurable rate:

- Default rate: 2 frames per press
- Enabled per-button per-controller
- Automatically toggles during held input

## Input Recording

Recording captures input snapshots each frame:

1. Call start_recording() to begin
2. Play normally - inputs are captured
3. Call stop_recording() to finish
4. Use get_recording() to retrieve InputSnapshot vector

## Controller reconnect evidence

The frontend `InputManager` now keeps identical gamepads separate and persists
custom binding GUIDs plus available serial numbers. An unambiguous reconnect
or config reload restores the binding; multiple indistinguishable pads require
manual mapping. Numeric-only per-device configs need remapping once to save
identity metadata. This is frontend polling behavior; it does not establish
working multiplayer routing or the input plugin recording/playback paths.

`ctest --test-dir build -L controllers --output-on-failure` uses SDL virtual
pads to verify duplicate events, identical pads, custom bindings, capture,
reconnect, persistence, selection after removal, ambiguity, and axis endpoints.
Physical USB/Bluetooth and driver behavior remain unverified.

## Building

Built automatically as part of Veloce. See main README for build instructions.

## Source Files

| File | Purpose |
|------|---------|
| src/default_input_plugin.cpp | Plugin implementation |
| CMakeLists.txt | Build configuration |



## Frontend integration

The GUI initializes the input plugin selected in `config/plugins.json` (default:
Default Input) with a live `IInputHost`, supplies the active core's controller
layout, and reads the processed button masks. Each logical core frame samples
once; retries at the same frame reuse those masks. Local execution and netplay's
local input argument use the same cached controller-zero value. A pause without
frame advance does not advance plugin turbo or recording.

Default Input accepts an optional separate `IMappedInputHost` capability. Its
base state comes from the frontend's existing per-platform bindings, wildcard
pad fallback, persistent device identities and mapping-capture suppression.
The original ABI v1 interfaces keep their vtables; raw-polling input plugins
continue to work. Mapping capture also blocks the final core/netplay input even
when a plugin supplies an override. GUI profiles currently configure controller
zero; additional controllers require explicit plugin bindings/overrides. Generic
local `run_frame()` still has a single-controller input field.

Plugin `get_binding()` exposes current mapped bindings in raw polling's device
index coordinates; disconnected identities become -2, while wildcard -1 stays
-1. The persisted profile retains its SDL instance/identity fields. Explicit
plugin bindings override that virtual button's mapped state and currently use
connected-device indices. They are not persistent identity profiles. Physical
controller identity persistence remains owned by the frontend manager.

Input candidates initialize before replacement; failure preserves the active
instance. No-host activation is rejected, and shutdown precedes destruction.
ROM/core/plugin changes, reset and manual state loads invalidate the frame
sample. ROM load, reset and manual state load also stop recording/playback so
absolute snapshot frame numbers cannot cross timelines. Netplay rollback does
not resample physical input mid-replay.

Recording snapshots represent **controller zero**, use absolute host frame
numbers, include plugin overrides/playback and avoid duplicate snapshots from
repeated reads of a frame. Playback accepts strictly increasing nonnegative
frame numbers, holds sparse snapshots until the next event and returns to live
input after its last frame. It does not advance when another controller is read.
Rewinding the host frame counter stops recording/playback and resets turbo phase;
full reset/state event recording and alternate TAS-plugin integration are pending.

Executed evidence on Linux:

- A dynamically loaded input/core fixture in the production GUI receives exact
  A/B/C-Up masks. Two controller ports are polled once for each of three logical
  frames; a repeated frame attempt reuses its sample. Layout, host metadata,
  absent raw devices, clean lifecycle and result-file termination are checked.
- The real Default Input plugin checks mapped input, explicit key/axis overrides,
  turbo phases, controller-zero recording, recording of overrides, duplicate
  read suppression, sparse playback, playback termination, malformed recordings
  and frame-counter rewind behavior.
- SDL virtual-controller tests now pass all mapped states and binding views
  through the real Default Input plugin, including identical-pad separation,
  custom remaps/axes, hot-plugging and saved identity/profile restoration.
- Dynamic lifecycle checks execute no-host/missing-plugin failures, rejected
  initialization with a usable prior instance, successful replacement and one
  shutdown/destruction per initialized instance.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -R '^(input_plugin_test|input_frontend_test|input_lifecycle_test|input_hotplug_test)$' --output-on-failure
```

The GUI test requires `xvfb-run`; artifacts remain in
`build/artifacts/input_frontend/` and `build/artifacts/input_lifecycle/`.
These are frontend routing tests, not exhaustive guest hardware ROMs. Physical
controllers, analog calibration/remapping, full local multiplayer configuration, plugin
binding persistence and TAS-editor routing remain unfinished or unverified.
Core completion percentages retain their existing milestone scores.


## N64 signed stick input

The frontend supplies optional `IControllerInputEncodingHost` alongside the
unchanged input ABI v1 host. Default Input calls it after digital mapping/turbo
and before recording; overrides/playback remain canonical words. N64 uses
[`n64_input.hpp`](../../include/emu/n64_input.hpp) to preserve fourteen buttons
and signed X/Y bytes in the same word carried by movies and netplay. Other
platforms retain digital masks. Third-party hosts without the capability keep
digital output; third-party plugins must explicitly use the capability to add
frontend stick input.

Choose the port-zero gamepad in Controller settings: its left stick uses a
15% axial dead zone, rescales to +/-80, and reverses SDL Y. Keyboard selection
uses T/F/G/H for up/left/down/right; arrow bindings remain D-pad input.
Physical calibration, analog remapping and full local multiplayer device
configuration remain pending. See [N64 evidence](../../cores/n64/README.md#signed-stick-input-and-cached-si-observations).
