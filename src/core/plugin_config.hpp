#pragma once

#include "emu/plugin_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace emu {

// Stores which plugin is selected for each plugin type
// Persisted to JSON configuration file
class PluginConfiguration {
public:
    PluginConfiguration();
    ~PluginConfiguration() = default;

    // Load configuration from file
    bool load(const std::filesystem::path& path);

    // Save configuration to file
    bool save(const std::filesystem::path& path) const;
    bool save() const;  // Save to last loaded path

    // Get/set selected plugin for a type
    std::string get_selected_plugin(PluginType type) const;
    void set_selected_plugin(PluginType type, const std::string& plugin_name);

    // Check if a plugin type has a selection
    bool has_selection(PluginType type) const;

    // Clear selection for a type (will use default)
    void clear_selection(PluginType type);

    // Get all selections
    const std::unordered_map<PluginType, std::string>& get_all_selections() const {
        return m_selections;
    }

    // Game plugin multi-selection (multiple game plugins can be enabled)
    std::vector<std::string> get_enabled_game_plugins() const;
    void set_enabled_game_plugins(const std::vector<std::string>& plugins);
    void add_enabled_game_plugin(const std::string& plugin_name);
    void remove_enabled_game_plugin(const std::string& plugin_name);
    bool is_game_plugin_enabled(const std::string& plugin_name) const;

    // Per-plugin configuration (plugin-specific settings)
    std::string get_plugin_setting(const std::string& plugin_name, const std::string& key) const;
    void set_plugin_setting(const std::string& plugin_name, const std::string& key, const std::string& value);

    // Check if configuration has been modified
    bool is_modified() const { return m_modified; }
    void clear_modified() { m_modified = false; }

    // Default plugin names (used when no selection is made)
    static const char* get_default_plugin_name(PluginType type);

private:
    std::unordered_map<PluginType, std::string> m_selections;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_plugin_settings;
    std::vector<std::string> m_enabled_game_plugins;  // Multiple game plugins can be enabled
    std::filesystem::path m_config_path;
    bool m_modified = false;
};

} // namespace emu
