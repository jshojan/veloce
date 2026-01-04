#pragma once

#include "emu/plugin_types.hpp"
#include "emu/emulator_plugin.hpp"
#include "emu/video_plugin.hpp"
#include "emu/audio_plugin.hpp"
#include "emu/input_plugin.hpp"
#include "emu/tas_plugin.hpp"
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
#include <iostream>

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

// Game plugin instance with its handle
struct GamePluginInstance {
    IGamePlugin* plugin = nullptr;
    PluginHandle* handle = nullptr;
    std::string name;
    bool enabled = true;
    bool visible = true;  // GUI panel visibility
};

// Emulator plugin instance with its handle (kept loaded for configuration)
struct EmulatorPluginInstance {
    IEmulatorPlugin* plugin = nullptr;
    PluginHandle* handle = nullptr;
    std::string name;           // Name from registry (stable identifier)
    std::string library_path;   // Path to the .so file
};

// Active plugin instances (one per type, except Game which supports multiple)
struct ActivePlugins {
    IEmulatorPlugin* emulator = nullptr;
    IVideoPlugin* video = nullptr;
    IAudioPlugin* audio = nullptr;
    IInputPlugin* input = nullptr;
    ITASPlugin* tas = nullptr;
    INetplayPlugin* netplay = nullptr;

    // Multiple game plugins can be active simultaneously
    std::vector<GamePluginInstance> game_plugins;

    // Handles for cleanup (single-instance plugins)
    PluginHandle* emulator_handle = nullptr;
    PluginHandle* video_handle = nullptr;
    PluginHandle* audio_handle = nullptr;
    PluginHandle* input_handle = nullptr;
    PluginHandle* tas_handle = nullptr;
    PluginHandle* netplay_handle = nullptr;
};

// Forward declaration
class PathsConfiguration;

// Callback for plugin changes
using PluginChangedCallback = std::function<void(PluginType type, const std::string& name)>;

// GamePluginHost - Implementation of IGameHost that bridges game plugins to the emulator
// This is now managed by PluginManager, replacing the old SpeedrunManager
class GamePluginHost : public IGameHost {
public:
    explicit GamePluginHost(class PluginManager* pm) : m_plugin_manager(pm) {}

    // Memory access (delegates to emulator plugin)
    uint8_t read_memory(uint16_t address) override;
    uint16_t read_memory_16(uint16_t address) override;
    uint32_t read_memory_32(uint16_t address) override;
    void write_memory(uint16_t address, uint8_t value) override;

    // Emulator state
    bool is_emulator_running() const override;
    bool is_emulator_paused() const override;
    uint64_t get_frame_count() const override;
    double get_fps() const override;

    // ROM info
    const char* get_rom_name() const override;
    uint32_t get_rom_crc32() const override;
    const char* get_platform_name() const override;

    // Category selection
    const char* get_selected_category() const override;

    // Logging
    void log_message(const char* message) override;

    // Notification callbacks from plugins
    void on_timer_started() override;
    void on_timer_stopped() override;
    void on_split_triggered(int split_index) override;
    void on_run_completed(uint64_t final_time_ms) override;
    void on_run_reset() override;

    // Set ROM info (called when ROM is loaded)
    void set_rom_info(const std::string& name, uint32_t crc32);
    void set_category(const std::string& category) { m_category = category; }
    void set_paused(bool paused) { m_paused = paused; }

private:
    PluginManager* m_plugin_manager;
    std::string m_rom_name;
    uint32_t m_rom_crc32 = 0;
    std::string m_category = "Any%";
    bool m_paused = false;
};

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
    INetplayPlugin* get_netplay_plugin() const { return m_active.netplay; }

    // Game plugin access (multiple can be active)
    // Returns the first game plugin for backward compatibility
    IGamePlugin* get_game_plugin() const {
        return m_active.game_plugins.empty() ? nullptr : m_active.game_plugins[0].plugin;
    }

    // Get all active game plugins
    const std::vector<GamePluginInstance>& get_game_plugins() const { return m_active.game_plugins; }
    std::vector<GamePluginInstance>& get_game_plugins() { return m_active.game_plugins; }

    // Game plugin management (multiple can be active simultaneously)
    bool activate_game_plugin_by_name(const std::string& name);
    bool deactivate_game_plugin_by_name(const std::string& name);
    bool is_game_plugin_active(const std::string& name) const;

    // Load all enabled game plugins based on configuration
    void load_enabled_game_plugins();

    // Deactivate all game plugins
    void deactivate_all_game_plugins();

    // Initialize all active game plugins with the host interface
    void initialize_game_plugins();

    // Update all active game plugins (call on_frame)
    void update_game_plugins();

    // Notify game plugins about ROM load/unload
    void notify_game_plugins_rom_loaded();
    void notify_game_plugins_rom_unloaded();

    // Get the game plugin host interface
    IGameHost* get_game_host() { return m_game_host.get(); }

    // Set paused state (for IGameHost)
    void set_paused(bool paused) { if (m_game_host) m_game_host->set_paused(paused); }

    // Select and activate a plugin by type and name
    bool set_active_plugin(PluginType type, const std::string& name);

    // Activate the best game plugin for the current ROM
    bool activate_game_plugin_for_rom(uint32_t crc32);

    // Get available plugins for a type
    std::vector<std::string> get_available_plugins(PluginType type) const;

    // Get the name of the currently selected plugin for a type
    std::string get_selected_plugin_name(PluginType type) const;

    // Get all loaded emulator plugins (for configuration UI)
    const std::vector<EmulatorPluginInstance>& get_all_emulator_plugins() const { return m_emulator_plugins; }

    // Get an emulator plugin by registry name (for configuration)
    IEmulatorPlugin* get_emulator_plugin_by_name(const std::string& name) const;

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

    // Set netplay host (for initializing netplay plugins when they're activated)
    void set_netplay_host(INetplayHost* host) { m_netplay_host = host; }

    // Battery-backed save file support
    // These are called automatically by load_rom/unload_rom when appropriate
    bool load_battery_save();
    bool save_battery_save();
    std::filesystem::path get_save_file_path() const;

private:
    // Load all emulator plugins (for configuration access)
    void load_all_emulator_plugins();

    // Activate a specific plugin
    bool activate_emulator_plugin(const std::string& name);
    bool activate_video_plugin(const std::string& name);
    bool activate_audio_plugin(const std::string& name);
    bool activate_input_plugin(const std::string& name);
    bool activate_tas_plugin(const std::string& name);
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

    // Get config file path for an emulator plugin
    std::filesystem::path get_core_config_path(const std::string& core_name) const;

    PluginRegistry m_registry;
    PluginConfiguration m_config;
    ActivePlugins m_active;

    // All emulator plugins (kept loaded for configuration access)
    std::vector<EmulatorPluginInstance> m_emulator_plugins;

    // Legacy support
    std::vector<PluginInfo> m_legacy_plugins;
    PluginInfo* m_active_plugin_info = nullptr;

    std::string m_plugin_directory;
    std::vector<PluginChangedCallback> m_change_callbacks;

    // Current ROM path for save file support
    std::string m_current_rom_path;

    // Paths configuration for save directories
    PathsConfiguration* m_paths_config = nullptr;

    // Netplay host (for initializing netplay plugins)
    INetplayHost* m_netplay_host = nullptr;

    // Guard against double-shutdown
    bool m_shutdown_called = false;

    // Game plugin host (IGameHost implementation)
    std::unique_ptr<GamePluginHost> m_game_host;
};

} // namespace emu
