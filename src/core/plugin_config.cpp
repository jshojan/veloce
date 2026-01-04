#include "plugin_config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace emu {

// Helper to convert PluginType to string key
static std::string type_to_key(PluginType type) {
    switch (type) {
        case PluginType::Emulator:      return "emulator";
        case PluginType::Video:         return "video";
        case PluginType::Audio:         return "audio";
        case PluginType::Input:         return "input";
        case PluginType::TAS:           return "tas";
        case PluginType::Game:          return "game";
        case PluginType::Netplay:       return "netplay";
        default:                        return "unknown";
    }
}

// Helper to convert string key to PluginType
static PluginType key_to_type(const std::string& key) {
    if (key == "emulator")       return PluginType::Emulator;
    if (key == "video")          return PluginType::Video;
    if (key == "audio")          return PluginType::Audio;
    if (key == "input")          return PluginType::Input;
    if (key == "tas")            return PluginType::TAS;
    if (key == "game")           return PluginType::Game;
    if (key == "speedrun_tools") return PluginType::Game;  // Legacy migration
    if (key == "netplay")        return PluginType::Netplay;
    return PluginType::Emulator;  // Default fallback
}

PluginConfiguration::PluginConfiguration() = default;

bool PluginConfiguration::load(const std::filesystem::path& path) {
    m_config_path = path;

    if (!std::filesystem::exists(path)) {
        // No config file yet, use defaults
        return true;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open plugin config: " << path << std::endl;
            return false;
        }

        nlohmann::json json;
        file >> json;

        // Load plugin selections
        if (json.contains("selected_plugins") && json["selected_plugins"].is_object()) {
            for (auto& [key, value] : json["selected_plugins"].items()) {
                if (value.is_string()) {
                    PluginType type = key_to_type(key);
                    m_selections[type] = value.get<std::string>();
                }
            }
        }

        // Load per-plugin settings
        if (json.contains("plugin_settings") && json["plugin_settings"].is_object()) {
            for (auto& [plugin_name, settings] : json["plugin_settings"].items()) {
                if (settings.is_object()) {
                    for (auto& [key, value] : settings.items()) {
                        if (value.is_string()) {
                            m_plugin_settings[plugin_name][key] = value.get<std::string>();
                        }
                    }
                }
            }
        }

        // Load enabled game plugins (multi-select)
        if (json.contains("enabled_game_plugins") && json["enabled_game_plugins"].is_array()) {
            m_enabled_game_plugins.clear();
            for (const auto& name : json["enabled_game_plugins"]) {
                if (name.is_string()) {
                    m_enabled_game_plugins.push_back(name.get<std::string>());
                }
            }
        }

        m_modified = false;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading plugin config: " << e.what() << std::endl;
        return false;
    }
}

bool PluginConfiguration::save(const std::filesystem::path& path) const {
    try {
        nlohmann::json json;

        // Save plugin selections
        nlohmann::json selections;
        for (const auto& [type, name] : m_selections) {
            selections[type_to_key(type)] = name;
        }
        json["selected_plugins"] = selections;

        // Save per-plugin settings (ensure empty object, not null)
        nlohmann::json settings = nlohmann::json::object();
        for (const auto& [plugin_name, plugin_settings] : m_plugin_settings) {
            nlohmann::json plugin_json = nlohmann::json::object();
            for (const auto& [key, value] : plugin_settings) {
                plugin_json[key] = value;
            }
            settings[plugin_name] = plugin_json;
        }
        json["plugin_settings"] = settings;

        // Save enabled game plugins (multi-select)
        json["enabled_game_plugins"] = m_enabled_game_plugins;

        // Create parent directories if needed
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open plugin config for writing: " << path << std::endl;
            return false;
        }

        file << json.dump(4);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error saving plugin config: " << e.what() << std::endl;
        return false;
    }
}

bool PluginConfiguration::save() const {
    if (m_config_path.empty()) {
        std::cerr << "No config path set" << std::endl;
        return false;
    }
    return save(m_config_path);
}

std::string PluginConfiguration::get_selected_plugin(PluginType type) const {
    auto it = m_selections.find(type);
    if (it != m_selections.end()) {
        return it->second;
    }
    return get_default_plugin_name(type);
}

void PluginConfiguration::set_selected_plugin(PluginType type, const std::string& plugin_name) {
    m_selections[type] = plugin_name;
    m_modified = true;
}

bool PluginConfiguration::has_selection(PluginType type) const {
    return m_selections.find(type) != m_selections.end();
}

void PluginConfiguration::clear_selection(PluginType type) {
    auto it = m_selections.find(type);
    if (it != m_selections.end()) {
        m_selections.erase(it);
        m_modified = true;
    }
}

std::string PluginConfiguration::get_plugin_setting(const std::string& plugin_name, const std::string& key) const {
    auto plugin_it = m_plugin_settings.find(plugin_name);
    if (plugin_it != m_plugin_settings.end()) {
        auto setting_it = plugin_it->second.find(key);
        if (setting_it != plugin_it->second.end()) {
            return setting_it->second;
        }
    }
    return "";
}

void PluginConfiguration::set_plugin_setting(const std::string& plugin_name, const std::string& key, const std::string& value) {
    m_plugin_settings[plugin_name][key] = value;
    m_modified = true;
}

std::vector<std::string> PluginConfiguration::get_enabled_game_plugins() const {
    return m_enabled_game_plugins;
}

void PluginConfiguration::set_enabled_game_plugins(const std::vector<std::string>& plugins) {
    m_enabled_game_plugins = plugins;
    m_modified = true;
}

void PluginConfiguration::add_enabled_game_plugin(const std::string& plugin_name) {
    if (!is_game_plugin_enabled(plugin_name)) {
        m_enabled_game_plugins.push_back(plugin_name);
        m_modified = true;
    }
}

void PluginConfiguration::remove_enabled_game_plugin(const std::string& plugin_name) {
    auto it = std::find(m_enabled_game_plugins.begin(), m_enabled_game_plugins.end(), plugin_name);
    if (it != m_enabled_game_plugins.end()) {
        m_enabled_game_plugins.erase(it);
        m_modified = true;
    }
}

bool PluginConfiguration::is_game_plugin_enabled(const std::string& plugin_name) const {
    return std::find(m_enabled_game_plugins.begin(), m_enabled_game_plugins.end(), plugin_name)
           != m_enabled_game_plugins.end();
}

const char* PluginConfiguration::get_default_plugin_name(PluginType type) {
    switch (type) {
        case PluginType::Emulator:      return "NES";
        case PluginType::Video:         return "Default Video";
        case PluginType::Audio:         return "Default Audio";
        case PluginType::Input:         return "Default Input";
        case PluginType::TAS:           return "TAS Editor";
        case PluginType::Game:          return "Built-in Timer";
        case PluginType::Netplay:       return "";
        default:                        return "";
    }
}

} // namespace emu
