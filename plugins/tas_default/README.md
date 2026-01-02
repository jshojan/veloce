# Default TAS Plugin

Tool-assisted speedrun movie editor and player for Veloce.

## Overview

The Default TAS Plugin provides comprehensive TAS functionality including movie recording, playback, frame editing, and the greenzone system for instant seeking. It supports FCEUX FM2 movie import and includes undo/redo with 100 levels of history.

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
| Version | 1.0.0 |
| Author | Veloce Team |
| File Formats | .fm2, .tas |

### TAS Modes

| Mode | Description |
|------|-------------|
| Stopped | No movie loaded |
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

## File Formats

### Native Format (.tas)

Binary format with:
- Magic: "TAS1"
- TASMovieInfo header
- Optional start state
- Frame data array

### FM2 Import

Imports FCEUX movie format:
- Parses header comments
- Reads frame lines (|1|23456789|)
- Maps NES button layout

## Greenzone System

Automatic savestates every 60 frames during recording:
- Enables instant seeking during playback
- Invalidated when frames are edited
- Trades memory for seek speed

## Building

Built automatically as part of Veloce. See main README for build instructions.

## Source Files

| File | Purpose |
|------|---------|
| src/default_tas_plugin.cpp | Plugin implementation |
| CMakeLists.txt | Build configuration |

