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

## Building

Built automatically as part of Veloce. See main README for build instructions.

## Source Files

| File | Purpose |
|------|---------|
| src/default_input_plugin.cpp | Plugin implementation |
| CMakeLists.txt | Build configuration |

