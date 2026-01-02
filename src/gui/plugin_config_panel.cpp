#include "plugin_config_panel.hpp"
#include "core/application.hpp"
#include "core/plugin_manager.hpp"

#include <imgui.h>
#include <algorithm>

namespace emu {

PluginConfigPanel::PluginConfigPanel() = default;
PluginConfigPanel::~PluginConfigPanel() = default;

bool PluginConfigPanel::render(Application& app, bool& visible) {
    m_plugin_manager = &app.get_plugin_manager();

    // Initialize plugin lists on first render or when refreshed
    if (!m_initialized) {
        build_plugin_lists(*m_plugin_manager);
        m_initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(550, 450), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Plugin Configuration", &visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return false;
    }

    // Header text similar to Project64
    ImGui::TextWrapped("Configure which plugins to use for each component. "
                       "Changes will take effect after clicking Apply.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Create two columns - left for selectors, right for info
    float selector_width = ImGui::GetContentRegionAvail().x * 0.65f;

    ImGui::BeginChild("PluginSelectors", ImVec2(selector_width, -40), true);

    // Video Plugin (Coming Soon)
    render_plugin_selector("Video Plugin", PluginType::Video, false,
                           "Video plugin support coming soon. Currently using built-in renderer.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Audio Plugin
    render_plugin_selector("Audio Plugin", PluginType::Audio);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Input Plugin
    render_plugin_selector("Input Plugin", PluginType::Input);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // TAS Plugin
    render_plugin_selector("TAS Plugin", PluginType::TAS);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Speedrun Tools Plugin
    render_plugin_selector("Speedrun Tools", PluginType::SpeedrunTools);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Game Plugins (Auto-splitters)
    render_plugin_selector("Game Plugin (Auto-splitter)", PluginType::Game);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Emulator Cores Section
    render_emulator_cores_section();

    ImGui::EndChild();

    // Plugin info panel on the right
    ImGui::SameLine();
    ImGui::BeginChild("PluginInfo", ImVec2(0, -40), true);

    ImGui::Text("Plugin Information");
    ImGui::Separator();
    ImGui::Spacing();

    // Show info for the currently focused plugin or selected core
    const PluginSelection* selected_plugin = nullptr;

    // First check if a core is selected
    if (m_selected_core >= 0 && m_selected_core < static_cast<int>(m_emulator_cores.size())) {
        selected_plugin = &m_emulator_cores[m_selected_core];
    }
    // Otherwise check for focused plugin type
    else if (m_selected_indices.count(m_focused_type) && m_selected_indices[m_focused_type] > 0) {
        int idx = m_selected_indices[m_focused_type] - 1;  // -1 for "(None)" offset
        if (m_available_plugins.count(m_focused_type) &&
            idx >= 0 && idx < static_cast<int>(m_available_plugins[m_focused_type].size())) {
            selected_plugin = &m_available_plugins[m_focused_type][idx];
        }
    }

    // If nothing selected for focused type, find any valid selection
    if (!selected_plugin) {
        for (const auto& [type, plugins] : m_available_plugins) {
            if (m_selected_indices.count(type) && m_selected_indices[type] > 0) {
                int idx = m_selected_indices[type] - 1;
                if (idx >= 0 && idx < static_cast<int>(plugins.size())) {
                    selected_plugin = &plugins[idx];
                    m_focused_type = type;
                    break;
                }
            }
        }
    }

    if (selected_plugin) {
        render_about_plugin(selected_plugin);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "Select a plugin or core to see details.");
    }

    ImGui::EndChild();

    // Buttons at the bottom
    render_buttons(app, visible);

    ImGui::End();
    return false;
}

void PluginConfigPanel::render_plugin_selector(const char* label, PluginType type, bool enabled,
                                                const char* disabled_message) {
    ImGui::Text("%s", label);

    if (!enabled) {
        ImGui::BeginDisabled();
    }

    // Build combo items
    std::vector<const char*> items;
    items.push_back("(None)");

    if (m_available_plugins.count(type)) {
        for (const auto& plugin : m_available_plugins[type]) {
            items.push_back(plugin.name.c_str());
        }
    }

    // Ensure we have a valid selection index
    if (!m_selected_indices.count(type)) {
        m_selected_indices[type] = 0;
    }

    // Clamp index to valid range
    int& selected = m_selected_indices[type];
    if (selected >= static_cast<int>(items.size())) {
        selected = 0;
    }

    // Create unique ID for this combo
    char combo_id[64];
    snprintf(combo_id, sizeof(combo_id), "##plugin_%d", static_cast<int>(type));

    ImGui::SetNextItemWidth(-1);
    bool combo_changed = ImGui::Combo(combo_id, &selected, items.data(), static_cast<int>(items.size()));

    // Update focused type when combo is interacted with in any way
    if (combo_changed || ImGui::IsItemActivated() || ImGui::IsItemClicked() || ImGui::IsItemFocused()) {
        m_focused_type = type;
        m_selected_core = -1;   // Clear core selection
        if (combo_changed) {
            m_dirty = true;
        }
    }

    if (!enabled) {
        ImGui::EndDisabled();
        if (disabled_message) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f), "%s", disabled_message);
        }
    }
}

void PluginConfigPanel::render_emulator_cores_section() {
    ImGui::Text("Loaded Console Cores");
    ImGui::Spacing();

    if (m_emulator_cores.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No console cores loaded.");
    } else {
        // Display as a selectable list showing all loaded cores
        ImGui::BeginChild("CoresList", ImVec2(-1, 80), true);

        for (size_t i = 0; i < m_emulator_cores.size(); ++i) {
            const auto& core = m_emulator_cores[i];
            bool is_selected = (m_selected_core == static_cast<int>(i));

            // Create label with name and version
            char label[256];
            snprintf(label, sizeof(label), "%s v%s", core.name.c_str(), core.version.c_str());

            if (ImGui::Selectable(label, is_selected)) {
                m_selected_core = static_cast<int>(i);
            }
        }

        ImGui::EndChild();
    }

    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                       "Console cores are automatically selected based on ROM type.");
}

void PluginConfigPanel::render_about_plugin(const PluginSelection* selection) {
    if (!selection) {
        return;
    }

    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", selection->name.c_str());

    if (!selection->version.empty()) {
        ImGui::Text("Version:");
        ImGui::SameLine();
        ImGui::Text("%s", selection->version.c_str());
    }

    if (!selection->author.empty()) {
        ImGui::Text("Author:");
        ImGui::SameLine();
        ImGui::Text("%s", selection->author.c_str());
    }

    if (!selection->description.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", selection->description.c_str());
    }

    if (!selection->path.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Path: %s", selection->path.c_str());
    }
}

void PluginConfigPanel::render_buttons(Application& app, bool& visible) {
    ImGui::Separator();
    ImGui::Spacing();

    // Right-align buttons
    float button_width = 80.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button_width * 4 + spacing * 3;
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - total_width + ImGui::GetCursorPosX());

    if (ImGui::Button("Refresh", ImVec2(button_width, 0))) {
        m_plugin_manager->get_registry().refresh();
        build_plugin_lists(*m_plugin_manager);
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply", ImVec2(button_width, 0))) {
        apply_selections(*m_plugin_manager);
        m_dirty = false;
    }

    ImGui::SameLine();
    if (ImGui::Button("OK", ImVec2(button_width, 0))) {
        apply_selections(*m_plugin_manager);
        m_dirty = false;
        visible = false;
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
        // Revert to current selections
        build_plugin_lists(*m_plugin_manager);
        m_dirty = false;
        visible = false;
    }
}

void PluginConfigPanel::apply_selections(PluginManager& pm) {
    // Apply each plugin type selection
    for (auto& [type, index] : m_selected_indices) {
        // Skip video plugin (not implemented yet)
        if (type == PluginType::Video) continue;

        // Skip emulator plugins (auto-selected based on ROM)
        if (type == PluginType::Emulator) continue;

        std::string plugin_name;
        if (index > 0 && m_available_plugins.count(type)) {
            // Index 0 is "(None)", actual plugins start at 1
            int plugin_index = index - 1;
            if (plugin_index >= 0 && plugin_index < static_cast<int>(m_available_plugins[type].size())) {
                plugin_name = m_available_plugins[type][plugin_index].name;
            }
        }

        pm.set_active_plugin(type, plugin_name);
    }

    // Save configuration
    pm.save_config();
}

void PluginConfigPanel::build_plugin_lists(PluginManager& pm) {
    m_available_plugins.clear();
    m_emulator_cores.clear();
    m_selected_indices.clear();

    const auto& registry = pm.get_registry();
    const auto& all_plugins = registry.get_all_plugins();

    // Build lists per type
    for (const auto& metadata : all_plugins) {
        PluginSelection selection;
        selection.name = metadata.name;
        selection.version = metadata.version;
        selection.description = metadata.description;
        selection.author = metadata.author;
        selection.path = metadata.path.string();

        if (metadata.type == PluginType::Emulator) {
            // Emulator cores go to separate list
            // Append file extensions info to the description
            if (!metadata.file_extensions.empty()) {
                std::string ext_info = "Supports: ";
                for (size_t i = 0; i < metadata.file_extensions.size(); ++i) {
                    if (i > 0) ext_info += ", ";
                    ext_info += metadata.file_extensions[i];
                }
                if (!selection.description.empty()) {
                    selection.description += "\n\n" + ext_info;
                } else {
                    selection.description = ext_info;
                }
            }
            m_emulator_cores.push_back(selection);
        } else {
            m_available_plugins[metadata.type].push_back(selection);
        }
    }

    // Sort each list by name
    for (auto& [type, plugins] : m_available_plugins) {
        std::sort(plugins.begin(), plugins.end(),
                  [](const PluginSelection& a, const PluginSelection& b) {
                      return a.name < b.name;
                  });
    }

    std::sort(m_emulator_cores.begin(), m_emulator_cores.end(),
              [](const PluginSelection& a, const PluginSelection& b) {
                  return a.name < b.name;
              });

    // Initialize selection indices based on currently active plugins
    auto init_selection = [&](PluginType type, const std::string& active_name) {
        m_selected_indices[type] = 0;  // Default to "(None)"

        if (m_available_plugins.count(type) && !m_available_plugins[type].empty()) {
            const auto& plugins = m_available_plugins[type];

            // Try to find the configured plugin
            if (!active_name.empty()) {
                for (size_t i = 0; i < plugins.size(); ++i) {
                    if (plugins[i].name == active_name) {
                        m_selected_indices[type] = static_cast<int>(i) + 1;  // +1 for "(None)" offset
                        return;
                    }
                }
            }

            // If no configured plugin or not found, auto-select the first available plugin
            m_selected_indices[type] = 1;  // First plugin (index 0 is "(None)")
        }
    };

    // Get currently selected plugin names from config
    const auto& config = pm.get_config();

    init_selection(PluginType::Video, config.get_selected_plugin(PluginType::Video));
    init_selection(PluginType::Audio, config.get_selected_plugin(PluginType::Audio));
    init_selection(PluginType::Input, config.get_selected_plugin(PluginType::Input));
    init_selection(PluginType::TAS, config.get_selected_plugin(PluginType::TAS));
    init_selection(PluginType::SpeedrunTools, config.get_selected_plugin(PluginType::SpeedrunTools));
    init_selection(PluginType::Game, config.get_selected_plugin(PluginType::Game));
}

void PluginConfigPanel::refresh_plugins(PluginManager& pm) {
    pm.get_registry().refresh();
    build_plugin_lists(pm);
    m_initialized = true;
}

} // namespace emu
