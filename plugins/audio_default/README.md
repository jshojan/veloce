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

