#pragma once

#include "plugin_types.hpp"
#include <cstdint>
#include <vector>

#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_SPEEDRUN_TOOLS_PLUGIN_API_VERSION 1

namespace emu {

// Speedrun tools plugin information
struct SpeedrunToolsInfo {
    const char* name;           // "Built-in Timer", "LiveSplit Integration"
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description
    uint32_t capabilities;      // SpeedrunToolsCapabilities flags
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

// Host interface provided to speedrun tools plugins
class ISpeedrunToolsHost {
public:
    virtual ~ISpeedrunToolsHost() = default;

    // Emulator state
    virtual bool is_emulator_running() const = 0;
    virtual bool is_emulator_paused() const = 0;
    virtual uint64_t get_frame_count() const = 0;
    virtual double get_fps() const = 0;

    // ROM info
    virtual const char* get_rom_name() const = 0;
    virtual uint32_t get_rom_crc32() const = 0;
    virtual const char* get_platform_name() const = 0;

    // Split definitions (from game plugin)
    virtual int get_split_count() const = 0;
    virtual const char* get_split_name(int index) const = 0;

    // Notification to core
    virtual void on_timer_started() = 0;
    virtual void on_timer_stopped() = 0;
    virtual void on_split_triggered(int split_index) = 0;
    virtual void on_run_completed(uint64_t final_time_ms) = 0;
    virtual void on_run_reset() = 0;
};

// Speedrun tools plugin interface
// This handles the timer, splits display, and external integrations (LiveSplit)
class ISpeedrunToolsPlugin {
public:
    virtual ~ISpeedrunToolsPlugin() = default;

    // Get plugin info
    virtual SpeedrunToolsInfo get_info() = 0;

    // Initialize with host interface
    virtual bool initialize(ISpeedrunToolsHost* host) = 0;
    virtual void shutdown() = 0;

    // Timer control
    virtual void start_timer() = 0;
    virtual void stop_timer() = 0;
    virtual void reset_timer() = 0;
    virtual void pause_timer() = 0;
    virtual void resume_timer() = 0;

    // Split control
    virtual void split() = 0;
    virtual void undo_split() = 0;
    virtual void skip_split() = 0;

    // Timer state
    virtual TimerState get_timer_state() const = 0;
    virtual uint64_t get_current_time_ms() const = 0;
    virtual int get_current_split_index() const = 0;
    virtual int get_total_splits() const = 0;

    // Split times
    virtual SplitTiming get_split_timing(int index) const = 0;
    virtual uint64_t get_best_possible_time_ms() const = 0;
    virtual uint64_t get_sum_of_best_ms() const = 0;

    // Comparison management
    virtual ComparisonType get_comparison_type() const = 0;
    virtual void set_comparison_type(ComparisonType type) = 0;
    virtual int get_comparison_count() const { return 0; }
    virtual const char* get_comparison_name(int index) const { return nullptr; }

    // Run history
    virtual int get_attempt_count() const = 0;
    virtual int get_completed_count() const = 0;

    // Splits file management
    virtual bool load_splits(const char* path) = 0;
    virtual bool save_splits(const char* path) = 0;
    virtual bool save_splits() = 0;  // Save to current file
    virtual const char* get_splits_path() const = 0;
    virtual bool has_unsaved_changes() const = 0;

    // LiveSplit Server integration (optional)
    virtual bool connect_livesplit(const char* host = "localhost", int port = 16834) { return false; }
    virtual void disconnect_livesplit() {}
    virtual bool is_livesplit_connected() const { return false; }

    // Global hotkeys (optional)
    virtual bool register_global_hotkey(const char* action, int key, int modifiers) { return false; }
    virtual void unregister_global_hotkey(const char* action) {}

    // Called each frame by the core (for timer updates)
    virtual void on_frame() = 0;

    // Called when a split is triggered by the game plugin
    virtual void on_split_triggered() { split(); }

    // Autosave configuration
    virtual bool get_autosave_enabled() const { return false; }
    virtual void set_autosave_enabled(bool enabled) {}

    // Display configuration (for built-in timer display)
    virtual bool get_show_timer() const { return true; }
    virtual void set_show_timer(bool show) {}
    virtual bool get_show_splits() const { return true; }
    virtual void set_show_splits(bool show) {}
    virtual bool get_show_delta() const { return true; }
    virtual void set_show_delta(bool show) {}
};

} // namespace emu

// C interface for plugin loading
extern "C" {
    EMU_PLUGIN_EXPORT emu::ISpeedrunToolsPlugin* create_speedrun_tools_plugin();
    EMU_PLUGIN_EXPORT void destroy_speedrun_tools_plugin(emu::ISpeedrunToolsPlugin* plugin);
    EMU_PLUGIN_EXPORT uint32_t get_speedrun_tools_plugin_api_version();
}
