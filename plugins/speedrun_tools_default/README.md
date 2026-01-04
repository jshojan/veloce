# Default Game Plugin - Built-in Timer

Built-in speedrun timer for Veloce with splits tracking, PB management, and comparison support. Implements the unified IGamePlugin interface with fully self-contained GUI rendering.

## Overview

The Default Game Plugin (Built-in Timer) provides a complete speedrunning timer system. It tracks split times, manages personal bests, calculates sum of best segments, and supports multiple comparison modes. As a "universal" game plugin, it works with any ROM and can be used alongside game-specific auto-splitter plugins.

The plugin is **fully self-contained**:
- Renders its own ImGui panel via `render_gui()`
- Receives emulator state through the `IGameHost` interface
- No external panel classes or manager classes required in the core application
- Can be enabled/disabled independently of other game plugins

## Features

- Millisecond-precision timer
- Split tracking with segment times
- Personal best (PB) management
- Gold splits (best segment times)
- Sum of best calculation
- Best possible time display
- Multiple comparison types (PB, Best Segments)
- Attempt and completion counting
- JSON splits file format
- Autosave support
- **Self-contained ImGui panel rendering**
- **Can run alongside other game plugins**

## API

The plugin implements the **IGamePlugin** interface defined in `include/emu/game_plugin.hpp`.

### Plugin Information

| Property | Value |
|----------|-------|
| Name | Built-in Timer |
| Version | 1.0.0 |
| Author | Veloce Team |
| Plugin Type | Game |
| Autosave Support | Yes |
| Comparisons Support | Yes |
| LiveSplit Support | No (future) |

### IGamePlugin Interface

This plugin implements the unified IGamePlugin interface which combines:
- Timer functionality (start, stop, pause, reset)
- Split management (split, undo, skip)
- PB and comparison tracking
- Memory access for auto-splitters (provided by IGameHost from PluginManager)
- **GUI rendering via `render_gui(bool& visible)`**
- **Panel identification via `get_panel_name()`**

### IGameHost Interface

The plugin receives an `IGameHost` pointer during `initialize()` which provides:
- Memory access (`read_memory`, `write_memory`) for auto-splitters
- Emulator state (`is_emulator_running`, `is_emulator_paused`, `get_frame_count`)
- ROM info (`get_rom_name`, `get_rom_crc32`, `get_platform_name`)
- Logging (`log_message`)
- Callback notifications (`on_timer_started`, `on_split_triggered`, etc.)

The host implementation (`GamePluginHost`) is managed by `PluginManager`, replacing the old `SpeedrunManager` class.

### Key Methods

**Timer Control:**
- `start_timer()` - Start or resume timer
- `stop_timer()` - Stop timer (run finished)
- `reset_timer()` - Reset timer and splits
- `pause_timer()` / `resume_timer()` - Pause/resume

**Split Control:**
- `split()` - Record current split time
- `undo_split()` - Undo last split
- `skip_split()` - Skip current split

**Timer State:**
- `get_timer_state()` - NotRunning, Running, Paused, Finished
- `get_current_time_ms()` - Current elapsed time
- `get_current_split_index()` - Index of current split
- `get_total_splits()` - Total number of splits

**Split Timing:**
- `get_split_timing(index)` - Get timing info for a split
- `get_best_possible_time_ms()` - Current time + sum of best remaining
- `get_sum_of_best_ms()` - Sum of all gold segments

**Comparisons:**
- `get_comparison_type()` / `set_comparison_type()` - PB or Best Segments
- `get_comparison_count()` - Number of available comparisons
- `get_comparison_name(index)` - Name of comparison type

**Run History:**
- `get_attempt_count()` - Total attempts
- `get_completed_count()` - Completed runs

**File Management:**
- `load_splits(path)` - Load splits from JSON file
- `save_splits(path)` / `save_splits()` - Save splits to file
- `has_unsaved_changes()` - Check for unsaved changes

**ROM Matching:**
- `matches_rom(crc32, rom_name)` - Returns true for any ROM (universal plugin)

## Splits File Format

Splits are stored as JSON files in the `splits/` directory:

```json
{
  "game": "Super Mario Bros.",
  "category": "Any%",
  "attempts": 42,
  "completed": 15,
  "splits": [
    {"name": "World 1"},
    {"name": "World 4"},
    {"name": "World 8"}
  ],
  "personal_best": {
    "category": "Any%",
    "total_time_ms": 300000,
    "split_times": [60000, 180000, 300000],
    "gold_times": [58000, 115000, 118000]
  }
}
```

## Timer States

| State | Description |
|-------|-------------|
| NotRunning | Timer has not started or was reset |
| Running | Timer is actively counting |
| Paused | Timer is paused (manual pause) |
| Finished | Run completed (all splits hit) |

## Comparison Types

| Type | Description |
|------|-------------|
| PersonalBest | Compare to your PB split times |
| BestSegments | Compare each segment to your gold (best ever) |

## Split Timing Info

The `get_split_timing()` method returns a `SplitTiming` struct:

| Field | Description |
|-------|-------------|
| time_ms | Time when split was hit |
| delta_ms | Difference from comparison (positive = behind) |
| is_gold | True if this segment is a new best |
| is_pb | True if ahead of PB pace |

## Plugin Architecture

The unified IGamePlugin interface supports two types of plugins:

1. **Universal plugins** (like this one) - Handle timer/splits for any game
2. **Game-specific plugins** - Auto-splitters for specific games

This plugin is a universal timer that:
- Returns `true` from `matches_rom()` for any ROM
- Provides full timer functionality
- Renders its own ImGui panel with timer, splits table, and controls
- Can be used standalone or alongside game-specific auto-splitters

### Multi-Plugin Support

Multiple game plugins can be active simultaneously. The Plugin Manager maintains a list of active game plugins, and each one:
- Receives `on_frame()` calls for auto-split detection
- Has its `render_gui()` called to display its panel
- Can be individually shown/hidden via the Tools > Game Plugins menu

This enables use cases like:
- Built-in Timer + SMB Auto-splitter (auto-splits trigger the timer)
- Multiple timers with different layouts
- Community-created plugins alongside the default timer

### Creating Custom Game Plugins

To create a game-specific auto-splitter:

1. Implement `IGamePlugin` interface
2. Return `false` from `matches_rom()` for non-matching ROMs
3. Use `IGameHost` memory access to detect game events
4. Implement `render_gui()` for custom UI (optional)
5. Build as a shared library in the plugins directory

## Building

Built automatically as part of Veloce. See main README for build instructions.

Output: `libgame_timer_default.so` (Linux) / `game_timer_default.dll` (Windows)

## Source Files

| File | Purpose |
|------|---------|
| src/default_speedrun_tools_plugin.cpp | Plugin implementation |
| CMakeLists.txt | Build configuration |
