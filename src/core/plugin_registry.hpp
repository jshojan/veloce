#pragma once

#include "emu/plugin_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <functional>

namespace emu {

// Handle to a loaded plugin library
struct PluginHandle {
    void* library_handle = nullptr;
    std::filesystem::path path;
    PluginMetadata metadata;

    // Function pointers (set based on plugin type)
    void* create_func = nullptr;
    void* destroy_func = nullptr;

    bool is_loaded() const { return library_handle != nullptr; }
};

// Plugin registry - discovers and manages all plugin types
class PluginRegistry {
public:
    PluginRegistry();
    ~PluginRegistry();

    // Disable copy
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    // Scan a directory for plugins
    // Can be called multiple times for different directories
    bool scan_directory(const std::filesystem::path& directory);

    // Get all discovered plugins
    const std::vector<PluginMetadata>& get_all_plugins() const { return m_plugins; }

    // Get plugins of a specific type
    std::vector<PluginMetadata> get_plugins_of_type(PluginType type) const;

    // Find a specific plugin by type and name
    const PluginMetadata* find_plugin(PluginType type, const std::string& name) const;

    // Find plugins that support a specific file extension (for emulator plugins)
    std::vector<PluginMetadata> find_plugins_for_extension(const std::string& extension) const;

    // Find game plugins that match a ROM CRC32
    std::vector<PluginMetadata> find_game_plugins_for_rom(uint32_t crc32) const;

    // Load a plugin and get a handle
    PluginHandle* load_plugin(const PluginMetadata& metadata);

    // Unload a specific plugin
    void unload_plugin(PluginHandle* handle);

    // Unload all plugins
    void unload_all();

    // Get loaded plugin handles
    const std::vector<PluginHandle>& get_loaded_plugins() const { return m_loaded_plugins; }

    // Find a loaded plugin by path
    PluginHandle* find_loaded_plugin(const std::filesystem::path& path);

    // Refresh - rescan all directories
    void refresh();

    // Get plugin directories
    const std::vector<std::filesystem::path>& get_plugin_directories() const { return m_plugin_directories; }

    // Add a plugin directory (will be scanned on next refresh)
    void add_plugin_directory(const std::filesystem::path& directory);

private:
    // Probe a library file to check if it's a valid plugin
    bool probe_plugin(const std::filesystem::path& path, PluginMetadata& metadata);

    // Load library and get function pointer
    void* load_library(const std::filesystem::path& path);
    void unload_library(void* handle);
    void* get_symbol(void* handle, const char* symbol_name);

    // Get the shared library extension for this platform
    static const char* get_library_extension();

    std::vector<PluginMetadata> m_plugins;
    std::vector<PluginHandle> m_loaded_plugins;
    std::vector<std::filesystem::path> m_plugin_directories;
};

} // namespace emu
