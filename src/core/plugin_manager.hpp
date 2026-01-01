#pragma once

#include "emu/emulator_plugin.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace emu {

// Information about a loaded plugin
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

    // Get list of available plugins
    const std::vector<PluginInfo>& get_plugins() const { return m_plugins; }

    // Find plugin by file extension
    PluginInfo* find_plugin_for_extension(const std::string& extension);

    // Find plugin by name
    PluginInfo* find_plugin_by_name(const std::string& name);

    // Get currently active plugin
    IEmulatorPlugin* get_active_plugin() const { return m_active_plugin; }

    // Set active plugin
    bool set_active_plugin(const std::string& name);
    bool set_active_plugin_for_file(const std::string& filepath);

    // Load ROM into active plugin
    bool load_rom(const std::string& path);
    bool load_rom(const uint8_t* data, size_t size);

    // Unload current ROM
    void unload_rom();

    // Check if ROM is loaded
    bool is_rom_loaded() const;

    // Get ROM CRC32
    uint32_t get_rom_crc32() const;

private:
    bool scan_plugins(const std::string& directory);
    bool load_plugin(const std::string& path);
    void unload_plugin(PluginInfo& plugin);
    std::string get_file_extension(const std::string& path);

    std::vector<PluginInfo> m_plugins;
    IEmulatorPlugin* m_active_plugin = nullptr;
    PluginInfo* m_active_plugin_info = nullptr;
    std::string m_plugin_directory;
};

} // namespace emu
