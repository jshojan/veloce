#pragma once

#include "plugin_types.hpp"
#include <cstdint>
#include <vector>

#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_GAME_PLUGIN_API_VERSION 2

namespace emu {

// Information about the game plugin
struct GamePluginInfo {
    const char* name;           // "Super Mario Bros. Auto-splitter" or "Built-in Timer"
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description

    // Game identification (for game-specific plugins)
    // Set to nullptr/0 for universal plugins like the built-in timer
    const char* game_name;      // "Super Mario Bros." or nullptr for universal
    const char* platform;       // "NES" - must match emulator plugin, or nullptr
    uint32_t game_crc32;        // Primary ROM checksum (0 = any/universal)

    // Alternative CRC32s (for different ROM versions)
    const uint32_t* alt_crc32s;
    int alt_crc32_count;

    // Categories supported
    const char** categories;    // {"Any%", "Warpless", nullptr}

    // Plugin capabilities (GamePluginCapabilities flags)
    uint32_t capabilities;
};

// Capability flags for Game plugins
namespace GamePluginCapabilities {
    constexpr uint32_t None           = 0;
    constexpr uint32_t AutoSplit      = 1 << 0;  // Automatic split detection
    constexpr uint32_t Timer          = 1 << 1;  // Built-in timer functionality
    constexpr uint32_t LiveSplit      = 1 << 2;  // LiveSplit integration
    constexpr uint32_t Autosave       = 1 << 3;  // Automatic PB saving
    constexpr uint32_t Comparisons    = 1 << 4;  // Delta comparisons
    constexpr uint32_t GlobalHotkeys  = 1 << 5;  // Global hotkey support
    constexpr uint32_t RamWatch       = 1 << 6;  // RAM watch functionality
    constexpr uint32_t LuaScripting   = 1 << 7;  // Lua script support
}

// Memory watch definition for RAM watch feature
struct MemoryWatch {
    const char* name;           // "Player X", "Lives"
    uint16_t address;           // Memory address
    uint8_t size;               // 1, 2, or 4 bytes
    bool is_signed;             // Signed or unsigned
    bool is_hex;                // Display as hex
    const char* format;         // Optional format string
};

// Split timing information (for display purposes)
struct SplitTiming {
    uint64_t time_ms;           // Time in milliseconds
    int64_t delta_ms;           // Delta from comparison (positive = behind)
    bool is_gold;               // Best segment ever
    bool is_pb;                 // Personal best for this split
};

// Run comparison types
enum class ComparisonType {
    PersonalBest,       // Compare to PB
    BestSegments,       // Sum of best segments
    Average,            // Average of all runs
    Median,             // Median of all runs
    WorstRun,           // Worst completed run
    BestRun,            // Best completed run
    Custom              // User-defined comparison
};

// Timer state
enum class TimerState {
    NotRunning,         // Timer has not started
    Running,            // Timer is actively counting
    Paused,             // Timer is paused (manual pause)
    Finished            // Run completed
};

// Host interface provided to game plugins
// Provides memory access, emulator state, and logging
class IGameHost {
public:
    virtual ~IGameHost() = default;

    // Memory access (for auto-splitters and RAM watches)
    virtual uint8_t read_memory(uint16_t address) = 0;
    virtual uint16_t read_memory_16(uint16_t address) = 0;
    virtual uint32_t read_memory_32(uint16_t address) = 0;
    virtual void write_memory(uint16_t address, uint8_t value) = 0;

    // Emulator state
    virtual bool is_emulator_running() const = 0;
    virtual bool is_emulator_paused() const = 0;
    virtual uint64_t get_frame_count() const = 0;
    virtual double get_fps() const = 0;

    // ROM info
    virtual const char* get_rom_name() const = 0;
    virtual uint32_t get_rom_crc32() const = 0;
    virtual const char* get_platform_name() const = 0;

    // Category selection
    virtual const char* get_selected_category() const = 0;

    // Logging
    virtual void log_message(const char* message) = 0;

    // Notification callbacks to core (called by plugin)
    virtual void on_timer_started() = 0;
    virtual void on_timer_stopped() = 0;
    virtual void on_split_triggered(int split_index) = 0;
    virtual void on_run_completed(uint64_t final_time_ms) = 0;
    virtual void on_run_reset() = 0;
};

// Unified Game Plugin interface
// Combines auto-splitter, timer, and speedrun tools functionality
//
// There are two types of game plugins:
// 1. Universal plugins (like built-in timer) - handle timer/splits for any game
// 2. Game-specific plugins - auto-splitters for specific games
//
// A universal timer plugin provides timer/PB management.
// A game-specific plugin provides auto-split detection and may delegate
// timer functionality to a universal plugin via the host.
class IGamePlugin {
public:
    virtual ~IGamePlugin() = default;

    // Get plugin info
    virtual GamePluginInfo get_info() = 0;

    // Initialize with host interface
    virtual bool initialize(IGameHost* host) = 0;
    virtual void shutdown() = 0;

    // ============================================================
    // ROM Matching (for game-specific plugins)
    // ============================================================

    // Called when ROM loads - return true if this plugin handles it
    // Universal plugins should return true for any ROM
    virtual bool matches_rom(uint32_t crc32, const char* rom_name) = 0;

    // ============================================================
    // Timer Control
    // ============================================================

    virtual void start_timer() = 0;
    virtual void stop_timer() = 0;
    virtual void reset_timer() = 0;
    virtual void pause_timer() = 0;
    virtual void resume_timer() = 0;

    // ============================================================
    // Split Control
    // ============================================================

    virtual void split() = 0;
    virtual void undo_split() = 0;
    virtual void skip_split() = 0;

    // ============================================================
    // Timer State
    // ============================================================

    virtual TimerState get_timer_state() const = 0;
    virtual uint64_t get_current_time_ms() const = 0;
    virtual int get_current_split_index() const = 0;
    virtual int get_total_splits() const = 0;

    // ============================================================
    // Split Times and Comparisons
    // ============================================================

    virtual SplitTiming get_split_timing(int index) const = 0;
    virtual uint64_t get_best_possible_time_ms() const = 0;
    virtual uint64_t get_sum_of_best_ms() const = 0;

    // Comparison management
    virtual ComparisonType get_comparison_type() const = 0;
    virtual void set_comparison_type(ComparisonType type) = 0;
    virtual int get_comparison_count() const { return 2; }  // PB, Best Segments
    virtual const char* get_comparison_name(int index) const { return nullptr; }

    // ============================================================
    // Run History
    // ============================================================

    virtual int get_attempt_count() const = 0;
    virtual int get_completed_count() const = 0;

    // ============================================================
    // Splits File Management
    // ============================================================

    virtual bool load_splits(const char* path) = 0;
    virtual bool save_splits(const char* path) = 0;
    virtual bool save_splits() = 0;  // Save to current file
    virtual const char* get_splits_path() const = 0;
    virtual bool has_unsaved_changes() const = 0;

    // ============================================================
    // Split Definitions (for game-specific auto-splitters)
    // ============================================================

    // Get split definitions for a specific category
    virtual std::vector<SplitDefinition> get_splits(const char* category = nullptr) { return {}; }

    // Get split name by index
    virtual const char* get_split_name(int index) const { return nullptr; }

    // ============================================================
    // Memory Watches (for RAM watch display)
    // ============================================================

    virtual std::vector<MemoryWatch> get_memory_watches() { return {}; }

    // ============================================================
    // Frame Callbacks
    // ============================================================

    // Called each frame - plugin can check memory and trigger actions
    virtual void on_frame() = 0;

    // Called when a split is triggered externally (e.g., by auto-splitter)
    virtual void on_split_triggered() { split(); }

    // ============================================================
    // Lifecycle Callbacks
    // ============================================================

    virtual void on_rom_loaded() {}
    virtual void on_rom_unloaded() {}
    virtual void on_reset() {}
    virtual void on_run_complete(uint64_t final_time_ms) {}

    // ============================================================
    // Category Management
    // ============================================================

    virtual int get_category_count() const { return 0; }
    virtual const char* get_category_name(int index) const { return nullptr; }
    virtual void set_active_category(int index) {}
    virtual int get_active_category() const { return 0; }

    // ============================================================
    // LiveSplit Server Integration (optional)
    // ============================================================

    virtual bool connect_livesplit(const char* host = "localhost", int port = 16834) { return false; }
    virtual void disconnect_livesplit() {}
    virtual bool is_livesplit_connected() const { return false; }

    // ============================================================
    // Global Hotkeys (optional)
    // ============================================================

    virtual bool register_global_hotkey(const char* action, int key, int modifiers) { return false; }
    virtual void unregister_global_hotkey(const char* action) {}

    // ============================================================
    // Autosave Configuration
    // ============================================================

    virtual bool get_autosave_enabled() const { return false; }
    virtual void set_autosave_enabled(bool enabled) {}

    // ============================================================
    // Display Configuration (for built-in timer display)
    // ============================================================

    virtual bool get_show_timer() const { return true; }
    virtual void set_show_timer(bool show) {}
    virtual bool get_show_splits() const { return true; }
    virtual void set_show_splits(bool show) {}
    virtual bool get_show_delta() const { return true; }
    virtual void set_show_delta(bool show) {}

    // ============================================================
    // GUI Rendering (plugin renders its own ImGui panels)
    // ============================================================

    // Set the ImGui context from the main application
    // This MUST be called before render_gui() to ensure the plugin
    // uses the same ImGui context as the application
    virtual void set_imgui_context(void* context) {}

    // Render the plugin's GUI panel(s)
    // Called every frame by the application when this plugin is active
    // Plugin can draw any ImGui windows/widgets it needs
    // The visibility flag can be used to toggle the panel on/off
    virtual void render_gui(bool& visible) {}

    // Get the display name for this plugin's panel in menus
    virtual const char* get_panel_name() const { return "Game Plugin"; }

    // ============================================================
    // Lua Scripting Support (optional)
    // ============================================================

    virtual bool supports_lua() const { return false; }
    virtual bool load_lua_script(const char* path) { return false; }
    virtual void unload_lua_script() {}
    virtual bool is_lua_running() const { return false; }

    // ============================================================
    // Custom Variables (for advanced auto-splitters)
    // ============================================================

    virtual int get_custom_variable_count() const { return 0; }
    virtual const char* get_custom_variable_name(int index) const { return nullptr; }
    virtual double get_custom_variable_value(int index) const { return 0.0; }
};

} // namespace emu

// C interface for plugin loading
extern "C" {
    EMU_PLUGIN_EXPORT emu::IGamePlugin* create_game_plugin();
    EMU_PLUGIN_EXPORT void destroy_game_plugin(emu::IGamePlugin* plugin);
    EMU_PLUGIN_EXPORT uint32_t get_game_plugin_api_version();
}
