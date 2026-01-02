#pragma once

#include "emu/plugin_types.hpp"
#include "emu/emulator_plugin.hpp"
#include "emu/video_plugin.hpp"
#include "emu/audio_plugin.hpp"
#include "emu/input_plugin.hpp"
#include "emu/tas_plugin.hpp"
#include "emu/speedrun_tools_plugin.hpp"
#include "emu/game_plugin.hpp"
#include "emu/netplay_plugin.hpp"
#include "plugin_registry.hpp"
#include "plugin_config.hpp"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <filesystem>

namespace emu {

// Legacy PluginInfo struct for backward compatibility
struct PluginInfo {
    std::string path;
    std::string name;
    std::string version;
    std::vector<std::string> extensions;
    void* handle = nullptr;
    IEmulatorPlugin* instance = nullptr;

    // Function pointers
    using CreateFunc = IEmulatorPlugin* (*)();
    using DestroyFunc = void (*)(IEmulatorPlugin*);
    using VersionFunc = uint32_t (*)();

    CreateFunc create_func = nullptr;
    DestroyFunc destroy_func = nullptr;
};

// Active plugin instances (one per type)
struct ActivePlugins {
    IEmulatorPlugin* emulator = nullptr;
    IVideoPlugin* video = nullptr;
    IAudioPlugin* audio = nullptr;
    IInputPlugin* input = nullptr;
    ITASPlugin* tas = nullptr;
    ISpeedrunToolsPlugin* speedrun_tools = nullptr;
    IGamePlugin* game = nullptr;
    INetplayPlugin* netplay = nullptr;

    // Handles for cleanup
    PluginHandle* emulator_handle = nullptr;
    PluginHandle* video_handle = nullptr;
    PluginHandle* audio_handle = nullptr;
    PluginHandle* input_handle = nullptr;
    PluginHandle* tas_handle = nullptr;
    PluginHandle* speedrun_tools_handle = nullptr;
    PluginHandle* game_handle = nullptr;
    PluginHandle* netplay_handle = nullptr;
};

// Forward declaration
class PathsConfiguration;

// Callback for plugin changes
using PluginChangedCallback = std::function<void(PluginType type, const std::string& name)>;

class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    // Disable copy
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    // Initialize and scan for plugins
    bool initialize(const std::string& plugin_dir = "lib");

    // Shutdown and unload all plugins
    void shutdown();

    // Access to the registry and configuration
    PluginRegistry& get_registry() { return m_registry; }
    const PluginRegistry& get_registry() const { return m_registry; }
    PluginConfiguration& get_config() { return m_config; }
    const PluginConfiguration& get_config() const { return m_config; }

    // Load/save configuration
    bool load_config(const std::string& path);
    bool save_config(const std::string& path = "");

    // Get active plugin instances
    const ActivePlugins& get_active_plugins() const { return m_active; }
    IEmulatorPlugin* get_emulator_plugin() const { return m_active.emulator; }
    IVideoPlugin* get_video_plugin() const { return m_active.video; }
    IAudioPlugin* get_audio_plugin() const { return m_active.audio; }
    IInputPlugin* get_input_plugin() const { return m_active.input; }
    ITASPlugin* get_tas_plugin() const { return m_active.tas; }
    ISpeedrunToolsPlugin* get_speedrun_tools_plugin() const { return m_active.speedrun_tools; }
    IGamePlugin* get_game_plugin() const { return m_active.game; }
    INetplayPlugin* get_netplay_plugin() const { return m_active.netplay; }

    // Select and activate a plugin by type and name
    bool set_active_plugin(PluginType type, const std::string& name);

    // Activate the best game plugin for the current ROM
    bool activate_game_plugin_for_rom(uint32_t crc32);

    // Get available plugins for a type
    std::vector<std::string> get_available_plugins(PluginType type) const;

    // Get the name of the currently selected plugin for a type
    std::string get_selected_plugin_name(PluginType type) const;

    // Callback registration
    void on_plugin_changed(PluginChangedCallback callback);

    // Legacy API for backward compatibility
    const std::vector<PluginInfo>& get_plugins() const { return m_legacy_plugins; }
    PluginInfo* find_plugin_for_extension(const std::string& extension);
    PluginInfo* find_plugin_by_name(const std::string& name);
    IEmulatorPlugin* get_active_plugin() const { return m_active.emulator; }
    bool set_active_plugin(const std::string& name);
    bool set_active_plugin_for_file(const std::string& filepath);

    // ROM loading (delegates to emulator plugin)
    bool load_rom(const std::string& path);
    bool load_rom(const uint8_t* data, size_t size);
    void unload_rom();
    bool is_rom_loaded() const;
    uint32_t get_rom_crc32() const;

    // Set paths configuration (for battery save directory)
    void set_paths_config(PathsConfiguration* paths_config) { m_paths_config = paths_config; }

    // Battery-backed save file support
    // These are called automatically by load_rom/unload_rom when appropriate
    bool load_battery_save();
    bool save_battery_save();
    std::filesystem::path get_save_file_path() const;

private:
    // Activate a specific plugin
    bool activate_emulator_plugin(const std::string& name);
    bool activate_video_plugin(const std::string& name);
    bool activate_audio_plugin(const std::string& name);
    bool activate_input_plugin(const std::string& name);
    bool activate_tas_plugin(const std::string& name);
    bool activate_speedrun_tools_plugin(const std::string& name);
    bool activate_game_plugin(const std::string& name);
    bool activate_netplay_plugin(const std::string& name);

    // Deactivate and cleanup a plugin
    void deactivate_plugin(PluginType type);

    // Build legacy plugin list for compatibility
    void build_legacy_plugin_list();

    // Notify callbacks
    void notify_plugin_changed(PluginType type, const std::string& name);

    // Helper to get file extension
    std::string get_file_extension(const std::string& path);

    PluginRegistry m_registry;
    PluginConfiguration m_config;
    ActivePlugins m_active;

    // Legacy support
    std::vector<PluginInfo> m_legacy_plugins;
    PluginInfo* m_active_plugin_info = nullptr;

    std::string m_plugin_directory;
    std::vector<PluginChangedCallback> m_change_callbacks;

    // Current ROM path for save file support
    std::string m_current_rom_path;

    // Paths configuration for save directories
    PathsConfiguration* m_paths_config = nullptr;

    // Guard against double-shutdown
    bool m_shutdown_called = false;
};

} // namespace emu
