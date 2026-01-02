#pragma once

#include "plugin_types.hpp"
#include <cstdint>
#include <vector>

#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_GAME_PLUGIN_API_VERSION 1

namespace emu {

// Information about the game plugin
struct GamePluginInfo {
    const char* name;           // "Super Mario Bros. Auto-splitter"
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description

    // Game identification
    const char* game_name;      // "Super Mario Bros."
    const char* platform;       // "NES" - must match emulator plugin
    uint32_t game_crc32;        // Primary ROM checksum (0 = any)

    // Alternative CRC32s (for different ROM versions)
    const uint32_t* alt_crc32s;
    int alt_crc32_count;

    // Categories supported
    const char** categories;    // {"Any%", "Warpless", nullptr}
};

// Memory watch definition for RAM watch feature
struct MemoryWatch {
    const char* name;           // "Player X", "Lives"
    uint16_t address;           // Memory address
    uint8_t size;               // 1, 2, or 4 bytes
    bool is_signed;             // Signed or unsigned
    bool is_hex;                // Display as hex
    const char* format;         // Optional format string
};

// Host interface provided to game plugins
class IGameHost {
public:
    virtual ~IGameHost() = default;

    // Memory access
    virtual uint8_t read_memory(uint16_t address) = 0;
    virtual uint16_t read_memory_16(uint16_t address) = 0;
    virtual uint32_t read_memory_32(uint16_t address) = 0;
    virtual void write_memory(uint16_t address, uint8_t value) = 0;

    // Timer control (delegates to SpeedrunToolsPlugin)
    virtual void start_timer() = 0;
    virtual void stop_timer() = 0;
    virtual void reset_timer() = 0;
    virtual void split() = 0;
    virtual void undo_split() = 0;
    virtual void skip_split() = 0;

    // Timer state
    virtual bool is_timer_running() const = 0;
    virtual uint64_t get_current_time_ms() const = 0;
    virtual int get_current_split_index() const = 0;

    // Frame info
    virtual uint64_t get_frame_count() const = 0;

    // Category selection
    virtual const char* get_selected_category() const = 0;

    // Logging
    virtual void log_message(const char* message) = 0;
};

// Game plugin interface
// Used for per-game features: auto-splitters, RAM watches, Lua scripts
class IGamePlugin {
public:
    virtual ~IGamePlugin() = default;

    // Get plugin info
    virtual GamePluginInfo get_info() = 0;

    // Get split definitions for a specific category
    virtual std::vector<SplitDefinition> get_splits(const char* category = nullptr) = 0;

    // Get memory watches (for RAM watch display)
    virtual std::vector<MemoryWatch> get_memory_watches() { return {}; }

    // Called each frame - plugin can check memory and trigger actions
    virtual void on_frame(IGameHost* host) = 0;

    // Called when ROM loads - return true if this plugin handles it
    virtual bool matches_rom(uint32_t crc32, const char* rom_name) = 0;

    // Lifecycle callbacks
    virtual void on_rom_loaded(IGameHost* host) {}
    virtual void on_rom_unloaded() {}
    virtual void on_reset() {}
    virtual void on_run_complete(uint64_t final_time_ms) {}

    // Category management
    virtual int get_category_count() const = 0;
    virtual const char* get_category_name(int index) const = 0;
    virtual void set_active_category(int index) {}
    virtual int get_active_category() const { return 0; }

    // Lua scripting support (optional)
    virtual bool supports_lua() const { return false; }
    virtual bool load_lua_script(const char* path) { return false; }
    virtual void unload_lua_script() {}
    virtual bool is_lua_running() const { return false; }

    // Custom variables (for advanced auto-splitters)
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
