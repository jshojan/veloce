#pragma once

#include "emu/plugin_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace emu {

class Application;
class PluginManager;

// Project64-style plugin configuration panel
class PluginConfigPanel {
public:
    PluginConfigPanel();
    ~PluginConfigPanel();

    // Render the plugin configuration window
    // Returns true if the window should close
    bool render(Application& app, bool& visible);

    // Refresh the plugin list
    void refresh_plugins(PluginManager& pm);

private:
    // Plugin selection state (before applying)
    struct PluginSelection {
        std::string name;
        std::string description;
        std::string version;
        std::string author;
        std::string path;
    };

    // Render individual sections
    void render_plugin_selector(const char* label, PluginType type, bool enabled = true,
                                const char* disabled_message = nullptr);
    void render_game_plugin_multi_selector();  // Multi-select for game plugins
    void render_emulator_cores_section();
    void render_about_plugin(const PluginSelection* selection);
    void render_buttons(Application& app, bool& visible);

    // Apply selections to plugin manager
    void apply_selections(PluginManager& pm);

    // Build plugin lists from registry
    void build_plugin_lists(PluginManager& pm);

    // State
    bool m_initialized = false;
    bool m_dirty = false;  // Changes pending

    // Plugin lists per type
    std::unordered_map<PluginType, std::vector<PluginSelection>> m_available_plugins;

    // Current selection indices per type
    std::unordered_map<PluginType, int> m_selected_indices;

    // Emulator cores (not selectable, just displayed)
    std::vector<PluginSelection> m_emulator_cores;

    // Track which plugin type to show info for
    PluginType m_focused_type = PluginType::Audio;

    // Track selected emulator core for info display (-1 = none)
    int m_selected_core = -1;

    // Track selected game plugin for info display (-1 = none)
    int m_selected_game_plugin = -1;

    // Game plugin enabled states (for multi-select checkboxes)
    std::unordered_map<std::string, bool> m_game_plugin_enabled;

    // Reference to plugin manager (set during render)
    PluginManager* m_plugin_manager = nullptr;
};

} // namespace emu
