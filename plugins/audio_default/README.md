# Default Audio Plugin

Audio passthrough plugin for Veloce with volume control and muting.

## Overview

The Default Audio Plugin provides basic audio processing for emulator output. It receives audio samples from the active emulator core, applies volume adjustments and muting, and forwards the processed audio to the host application for playback.

## Features

- Volume control (0.0 to 1.0 range)
- Mute toggle
- Direct passthrough at full volume (zero-copy optimization)
- Stack-based buffer for small sample batches

## API

The plugin implements the IAudioPlugin interface defined in include/emu/audio_plugin.hpp.

### Plugin Information

| Property | Value |
|----------|-------|
| Name | Default Audio |
| Version | 1.0.0 |
| Author | Veloce Team |
| Recording Support | No |
| Effects Support | No |

### Key Methods

- initialize(IAudioHost* host) - Initialize with host interface
- process(const float* input, size_t sample_count) - Process audio samples
- set_volume(float volume) / get_volume() - Volume control (0.0 - 1.0)
- set_muted(bool muted) / is_muted() - Mute control

## Building

Built automatically as part of Veloce. See main README for build instructions.

## Source Files

| File | Purpose |
|------|---------|
| src/default_audio_plugin.cpp | Plugin implementation |
| CMakeLists.txt | Build configuration |



## Frontend integration

The GUI frontend initializes the selected audio plugin from `config/plugins.json`
(default: Default Audio) with a live `IAudioHost`. Both streaming core samples
and the frame-end fallback pass through the plugin. The host then resamples
processed samples to the SDL device rate. Headless hardware-test runs continue
to avoid audio-device initialization.

`process()` receives **individual interleaved stereo floats**, not stereo-pair
counts. Query `host->get_format()` during every process call: it describes the
current source rate, two channels and 32-bit floats. Submit output synchronously
at that same rate; asynchronous submission or format conversion inside a plugin
is outside the current contract. Before the first process call the host reports
44100 Hz. These contract clarifications do not change ABI v1's vtable.

The selected plugin is resolved on each sample submission, so switching plugins
does not leave a callback pointing at a destroyed instance. A replacement is
initialized before the working plugin is shut down; a rejected initialization
keeps the working instance. Shutdown precedes destruction, with the host/device
still alive. Activation without an installed host is rejected. If the configured
plugin is unavailable at startup, the GUI reports device passthrough.

The bridge discards replay audio during rollback. Actual streamed samples
suppress the frame-end fallback, preventing duplicate audio. A core which only
stores the callback and never streams still has its batch audio delivered.
The device's master volume remains independent of plugin processing volume.

Executed evidence (Linux, SDL dummy audio / Xvfb):

- Production GUI plus dynamically loaded core/audio fixtures deliver exactly
  three buffers in each streaming/batch mode, with exact sample values and
  changing rates **32040 / 48000 / 44100 Hz**, and clean lifecycle/exit.
- The real Default Audio plugin produces exact half-volume and muted samples;
  streaming/batch duplication, rollback suppression, passthrough, bad inputs
  and host pause/resume are checked against actual output callbacks.
- Real plugin-manager replacement rejects a failing initializer, preserves and
  exercises the previous instance, then proves successful replacement and
  single shutdown/destruction. No-host and unknown-plugin activation fail.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build -R '^(audio_pipeline_test|audio_lifecycle_test|audio_frontend_test)$' --output-on-failure
```

`audio_frontend_test` is available when `xvfb-run` is installed. Artifacts remain
under `build/artifacts/audio_frontend/` and `build/artifacts/audio_lifecycle/`.
The fixtures verify frontend routing; they are not guest hardware accuracy ROMs.
Physical speakers/devices, asynchronous audio plugins, recording/effects, and
alternate TAS plugin routing remain unverified or unfinished. Input routing is
now [implemented and checked](../input_default/README.md#frontend-integration). Core delivery
percentages retain their existing milestone scores.
