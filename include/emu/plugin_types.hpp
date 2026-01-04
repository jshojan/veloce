#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

// DLL export macro for Windows
#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

namespace emu {

// Plugin type enumeration - matches Project64 style categories
enum class PluginType {
    Emulator,       // Console cores (NES, SNES, GB, etc.)
    Video,          // Graphics rendering (OpenGL, Vulkan, software)
    Audio,          // Audio output and processing
    Input,          // Controller handling
    TAS,            // Tool-Assisted Speedrun tools
    Game,           // Game plugins (timer, auto-splitters, Lua scripts)
    Netplay         // Network multiplayer (rollback, delay-based)
};

// Convert plugin type to string for display/logging
inline const char* plugin_type_to_string(PluginType type) {
    switch (type) {
        case PluginType::Emulator:      return "Emulator";
        case PluginType::Video:         return "Video";
        case PluginType::Audio:         return "Audio";
        case PluginType::Input:         return "Input";
        case PluginType::TAS:           return "TAS";
        case PluginType::Game:          return "Game";
        case PluginType::Netplay:       return "Netplay";
        default:                        return "Unknown";
    }
}

// Plugin metadata structure - common info for all plugin types
struct PluginMetadata {
    PluginType type;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    uint32_t api_version;
    std::filesystem::path path;

    // For emulator plugins: supported file extensions
    std::vector<std::string> file_extensions;

    // For game plugins: CRC32 of supported ROMs (empty = universal)
    std::vector<uint32_t> supported_roms;

    // Game plugins: additional ROM info
    uint32_t game_crc32 = 0;
    std::vector<uint32_t> alt_crc32s;

    // Plugin capabilities (type-specific, stored as flags)
    uint32_t capabilities;
};

// Capability flags for Video plugins
namespace VideoCapabilities {
    constexpr uint32_t None           = 0;
    constexpr uint32_t Shaders        = 1 << 0;  // CRT shaders, etc.
    constexpr uint32_t Filters        = 1 << 1;  // xBRZ, HQ2x, etc.
    constexpr uint32_t Recording      = 1 << 2;  // Video recording
    constexpr uint32_t Screenshot     = 1 << 3;  // Screenshot capture
    constexpr uint32_t Vsync          = 1 << 4;  // VSync support
    constexpr uint32_t Fullscreen     = 1 << 5;  // Fullscreen mode
}

// Capability flags for Audio plugins
namespace AudioCapabilities {
    constexpr uint32_t None           = 0;
    constexpr uint32_t Recording      = 1 << 0;  // Audio recording
    constexpr uint32_t Effects        = 1 << 1;  // DSP effects
    constexpr uint32_t DynamicRate    = 1 << 2;  // Dynamic rate control
}

// Capability flags for Input plugins
namespace InputCapabilities {
    constexpr uint32_t None           = 0;
    constexpr uint32_t Recording      = 1 << 0;  // Input recording
    constexpr uint32_t Playback       = 1 << 1;  // Input playback
    constexpr uint32_t Turbo          = 1 << 2;  // Turbo/autofire
    constexpr uint32_t Rumble         = 1 << 3;  // Rumble support
    constexpr uint32_t Netplay        = 1 << 4;  // Network input
}

// Capability flags for TAS plugins
namespace TASCapabilities {
    constexpr uint32_t None           = 0;
    constexpr uint32_t Greenzone      = 1 << 0;  // Savestate snapshots
    constexpr uint32_t LuaScripting   = 1 << 1;  // Lua support
    constexpr uint32_t PianoRoll      = 1 << 2;  // Piano roll editor
    constexpr uint32_t RamWatch       = 1 << 3;  // RAM watch/search
}

// Capability flags for Netplay plugins
namespace NetplayCapabilities {
    constexpr uint32_t None           = 0;
    constexpr uint32_t DelayBased     = 1 << 0;  // Simple input delay netplay
    constexpr uint32_t Rollback       = 1 << 1;  // GGPO-style rollback netcode
    constexpr uint32_t Spectators     = 1 << 2;  // Spectator mode support
    constexpr uint32_t Chat           = 1 << 3;  // In-game chat
    constexpr uint32_t Matchmaking    = 1 << 4;  // Lobby/matchmaking server
    constexpr uint32_t Relay          = 1 << 5;  // Relay server for NAT traversal
    constexpr uint32_t SaveStateSync  = 1 << 6;  // State sync for desync recovery
    constexpr uint32_t MultiPlayer    = 1 << 7;  // More than 2 players
}

// Base plugin information structure (returned by all plugins)
struct BasePluginInfo {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    uint32_t capabilities;
};

// Split trigger conditions (for auto-splitters)
enum class SplitCondition {
    Equals,         // Trigger when value equals target
    NotEquals,      // Trigger when value does not equal target
    GreaterThan,    // Trigger when value > target
    LessThan,       // Trigger when value < target
    ChangesTo,      // Trigger when value changes to target
    ChangesFrom,    // Trigger when value changes from target
    Increases,      // Trigger when value increases
    Decreases,      // Trigger when value decreases
    BitSet,         // Trigger when specific bit is set
    BitClear        // Trigger when specific bit is clear
};

// Definition of a single split for auto-splitting
struct SplitDefinition {
    const char* name;           // Display name ("Enter 1-2", "Bowser Fight")
    uint16_t watch_address;     // Memory address to watch
    uint8_t trigger_value;      // Value for comparison
    SplitCondition condition;   // Trigger condition
    uint8_t bit_index;          // For BitSet/BitClear conditions (0-7)
};

} // namespace emu

// C interface for plugin type identification
// Every plugin must export this function
extern "C" {
    // Returns the plugin type
    EMU_PLUGIN_EXPORT emu::PluginType get_plugin_type();

    // Returns base plugin info (all plugins implement this)
    EMU_PLUGIN_EXPORT emu::BasePluginInfo get_plugin_info();
}
