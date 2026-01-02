#pragma once

#include "input_types.hpp"
#include <cstdint>
#include <vector>
#include <string>

// DLL export macro for Windows
#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_TAS_PLUGIN_API_VERSION 1

namespace emu {

// TAS plugin information
struct TASPluginInfo {
    const char* name;           // "TAS Editor", etc.
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description
    const char** file_formats;  // Supported file formats {".fm2", ".bk2", nullptr}
};

// TAS movie metadata
struct TASMovieInfo {
    char title[256];            // Movie title
    char author[128];           // Author name
    char description[1024];     // Description/comments
    char platform[32];          // "NES", "SNES", etc.
    char rom_name[256];         // ROM filename
    uint32_t rom_crc32;         // ROM checksum
    uint64_t frame_count;       // Total frames in movie
    uint64_t rerecord_count;    // Number of rerecords
    bool starts_from_savestate; // True if movie starts from savestate
};

// Frame data in a TAS movie
struct TASFrameData {
    uint64_t frame_number;
    uint32_t controller_inputs[4];  // Up to 4 controllers
    bool has_reset;                 // Reset on this frame
    bool has_savestate;             // Savestate on this frame
    int savestate_slot;             // Which slot, if applicable
};

// Host interface provided to TAS plugins
class ITASHost {
public:
    virtual ~ITASHost() = default;

    // Emulation control
    virtual void reset_emulator() = 0;
    virtual void pause_emulator() = 0;
    virtual void resume_emulator() = 0;
    virtual void frame_advance() = 0;
    virtual bool is_emulator_paused() const = 0;

    // Frame info
    virtual uint64_t get_current_frame() const = 0;
    virtual double get_fps() const = 0;

    // Input injection
    virtual void set_controller_input(int controller, uint32_t buttons) = 0;
    virtual uint32_t get_controller_input(int controller) const = 0;

    // Save states
    virtual bool save_state(int slot) = 0;
    virtual bool load_state(int slot) = 0;
    virtual bool save_state_to_buffer(std::vector<uint8_t>& buffer) = 0;
    virtual bool load_state_from_buffer(const std::vector<uint8_t>& buffer) = 0;

    // ROM info
    virtual const char* get_rom_name() const = 0;
    virtual uint32_t get_rom_crc32() const = 0;
    virtual const char* get_platform_name() const = 0;

    // Memory access (for Lua scripting support)
    virtual uint8_t read_memory(uint16_t address) = 0;
    virtual void write_memory(uint16_t address, uint8_t value) = 0;
};

// TAS mode
enum class TASMode {
    Stopped,        // No movie loaded
    Recording,      // Recording new inputs
    Playing,        // Playing back movie
    ReadOnly,       // Playing, but can't modify
    ReadWrite       // Playing with editing enabled
};

// TAS plugin interface
class ITASPlugin {
public:
    virtual ~ITASPlugin() = default;

    // Get plugin info
    virtual TASPluginInfo get_info() = 0;

    // Initialize with host interface
    virtual bool initialize(ITASHost* host) = 0;
    virtual void shutdown() = 0;

    // Movie file operations
    virtual bool new_movie(const char* filename, bool from_savestate) = 0;
    virtual bool open_movie(const char* filename) = 0;
    virtual bool save_movie() = 0;
    virtual bool save_movie_as(const char* filename) = 0;
    virtual void close_movie() = 0;
    virtual bool is_movie_loaded() const = 0;

    // Get movie info
    virtual TASMovieInfo get_movie_info() const = 0;
    virtual void set_movie_info(const TASMovieInfo& info) = 0;

    // Playback/recording control
    virtual TASMode get_mode() const = 0;
    virtual void set_mode(TASMode mode) = 0;
    virtual void start_recording() = 0;
    virtual void stop_recording() = 0;
    virtual void start_playback() = 0;
    virtual void stop_playback() = 0;

    // Called each frame by the core
    // Returns the input to use for this frame
    virtual uint32_t on_frame(int controller) = 0;

    // Frame operations for editing
    virtual TASFrameData get_frame(uint64_t frame) const = 0;
    virtual void set_frame(uint64_t frame, const TASFrameData& data) = 0;
    virtual void insert_frame(uint64_t after_frame) = 0;
    virtual void delete_frame(uint64_t frame) = 0;
    virtual void clear_input(uint64_t start_frame, uint64_t end_frame) = 0;

    // Editing operations
    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual bool can_undo() const = 0;
    virtual bool can_redo() const = 0;
    virtual void increment_rerecord_count() = 0;

    // Greenzone (savestate snapshots for instant replay)
    virtual void invalidate_greenzone(uint64_t from_frame) = 0;
    virtual bool has_greenzone_at(uint64_t frame) const = 0;
    virtual bool seek_to_frame(uint64_t frame) = 0;

    // Selection (for batch editing in GUI)
    virtual void set_selection(uint64_t start, uint64_t end) = 0;
    virtual void get_selection(uint64_t& start, uint64_t& end) const = 0;
    virtual void copy_selection() = 0;
    virtual void cut_selection() = 0;
    virtual void paste_at(uint64_t frame) = 0;

    // Input display for current/target frame
    virtual uint64_t get_current_frame() const = 0;
    virtual uint64_t get_total_frames() const = 0;
    virtual uint64_t get_rerecord_count() const = 0;

    // Markers/bookmarks
    virtual void add_marker(uint64_t frame, const char* description) = 0;
    virtual void remove_marker(uint64_t frame) = 0;
    virtual int get_marker_count() const = 0;
    virtual uint64_t get_marker_frame(int index) const = 0;
    virtual const char* get_marker_description(int index) const = 0;

    // Lua scripting (optional)
    virtual bool supports_lua() const { return false; }
    virtual bool load_lua_script(const char* filename) { return false; }
    virtual void unload_lua_script() {}
    virtual bool is_lua_running() const { return false; }
};

} // namespace emu

// C interface for plugin loading
extern "C" {
    EMU_PLUGIN_EXPORT emu::ITASPlugin* create_tas_plugin();
    EMU_PLUGIN_EXPORT void destroy_tas_plugin(emu::ITASPlugin* plugin);
    EMU_PLUGIN_EXPORT uint32_t get_tas_plugin_api_version();
}
