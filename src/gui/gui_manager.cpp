#include "gui_manager.hpp"
#include "debug_panel.hpp"
#include "input_config_panel.hpp"
#include "plugin_config_panel.hpp"
#include "paths_config_panel.hpp"
#include "notification_manager.hpp"
#include "core/application.hpp"
#include "core/window_manager.hpp"
#include "core/renderer.hpp"
#include "core/plugin_manager.hpp"
#include "core/audio_manager.hpp"
#include "core/input_manager.hpp"
#include "core/savestate_manager.hpp"
#include "core/paths_config.hpp"
#include "emu/game_plugin.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <stdexcept>

namespace emu {

GuiManager::GuiManager() = default;

GuiManager::~GuiManager() {
    shutdown();
}

bool GuiManager::initialize(WindowManager& window_manager) {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Enable keyboard navigation
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window_manager.get_window(),
                                  window_manager.get_gl_context());
    ImGui_ImplOpenGL3_Init("#version 330");

    // Set current directory for ROM browser
    m_current_directory = std::filesystem::current_path().string();

    // Create GUI panels
    // Note: SpeedrunPanel has been moved into game plugins - they render their own GUI
    m_debug_panel = std::make_unique<DebugPanel>();
    m_input_config_panel = std::make_unique<InputConfigPanel>();
    m_plugin_config_panel = std::make_unique<PluginConfigPanel>();
    m_paths_config_panel = std::make_unique<PathsConfigPanel>();
    m_notification_manager = std::make_unique<NotificationManager>();
    // Note: NetplayPanel is created lazily since it needs Application reference

    m_initialized = true;
    std::cout << "GUI manager initialized" << std::endl;
    return true;
}

void GuiManager::shutdown() {
    if (m_initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        m_initialized = false;
    }
}

void GuiManager::process_event(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
}

void GuiManager::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void GuiManager::render(Application& app, Renderer& renderer) {
    // Render main menu bar
    render_main_menu(app);

    // Render game view
    render_game_view(app, renderer);

    // Render optional windows
    if (m_show_rom_browser) {
        render_rom_browser(app);
    }

    if (m_show_savestate_browser) {
        render_savestate_file_browser(app);
    }

    // Clear input capture mode by default - only set when Input settings tab is active
    app.get_input_manager().set_input_capture_mode(false);

    if (m_show_settings) {
        render_settings(app);
    }

    // Render all active game plugin GUI panels
    // Each game plugin now renders its own ImGui window via render_gui()
    if (m_show_speedrun_panel) {
        auto& game_plugins = app.get_plugin_manager().get_game_plugins();
        for (auto& inst : game_plugins) {
            if (inst.plugin && inst.enabled) {
                // Pass the ImGui context to the plugin before rendering
                // This is required because plugins link ImGui statically and have
                // their own context pointer that needs to point to our context
                inst.plugin->set_imgui_context(ImGui::GetCurrentContext());
                inst.plugin->render_gui(inst.visible);
            }
        }
    }

    // Render debug panel (also shown when debug mode is enabled)
    if ((m_show_debug_panel || app.is_debug_mode()) && m_debug_panel) {
        bool visible = m_show_debug_panel || app.is_debug_mode();
        m_debug_panel->render(app, visible);
        if (!visible) {
            m_show_debug_panel = false;
            app.set_debug_mode(false);
        }
    }

    // Render plugin configuration panel
    if (m_show_plugin_config && m_plugin_config_panel) {
        m_plugin_config_panel->render(app, m_show_plugin_config);
    }

    // Render core configuration window
    if (m_show_core_config) {
        render_core_config(app);
    }

    // Render netplay GUI - handled by the netplay plugin
    {
        auto* netplay_plugin = app.get_plugin_manager().get_netplay_plugin();
        if (netplay_plugin) {
            netplay_plugin->render_gui();
        }
    }

    // Demo window for development
    if (m_show_demo_window) {
        ImGui::ShowDemoWindow(&m_show_demo_window);
    }

    // Render notifications (always on top)
    if (m_notification_manager) {
        m_notification_manager->render();
    }
}

void GuiManager::end_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool GuiManager::wants_keyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool GuiManager::wants_mouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

void GuiManager::render_main_menu(Application& app) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open ROM...", "Ctrl+O")) {
                m_show_rom_browser = true;
            }

            ImGui::Separator();

            // Save State submenu
            render_save_state_menu(app);

            // Load State submenu
            render_load_state_menu(app);

            ImGui::Separator();

            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                app.request_quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Emulation")) {
            bool rom_loaded = app.get_plugin_manager().is_rom_loaded();

            if (ImGui::MenuItem("Reset", "Ctrl+R", false, rom_loaded)) {
                app.reset();
            }
            if (ImGui::MenuItem(app.is_paused() ? "Resume" : "Pause", "Escape", false, rom_loaded)) {
                app.toggle_pause();
            }
            if (ImGui::MenuItem("Frame Advance", "F", false, rom_loaded && app.is_paused())) {
                app.frame_advance();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Speed", rom_loaded)) {
                if (ImGui::MenuItem("50%", nullptr, app.get_speed() == 0.5f)) {
                    app.set_speed(0.5f);
                }
                if (ImGui::MenuItem("100%", nullptr, app.get_speed() == 1.0f)) {
                    app.set_speed(1.0f);
                }
                if (ImGui::MenuItem("200%", nullptr, app.get_speed() == 2.0f)) {
                    app.set_speed(2.0f);
                }
                if (ImGui::MenuItem("Unlimited", nullptr, app.get_speed() == 0.0f)) {
                    app.set_speed(0.0f);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            // Cores configuration - always available
            if (ImGui::MenuItem("Cores...", nullptr, m_show_core_config)) {
                if (!m_show_core_config) {
                    // Opening the window - set initial selection based on active plugin
                    auto& plugin_manager = app.get_plugin_manager();
                    auto* active_plugin = plugin_manager.get_active_plugin();
                    const auto& emu_plugins = plugin_manager.get_all_emulator_plugins();

                    if (active_plugin && active_plugin->is_rom_loaded()) {
                        // Find the index of the active plugin
                        m_selected_core_index = -1;
                        for (size_t i = 0; i < emu_plugins.size(); i++) {
                            if (emu_plugins[i].plugin == active_plugin ||
                                emu_plugins[i].name == active_plugin->get_info().name) {
                                m_selected_core_index = static_cast<int>(i);
                                break;
                            }
                        }
                    } else {
                        // No game running - no initial selection
                        m_selected_core_index = -1;
                    }
                }
                m_show_core_config = !m_show_core_config;
            }

            if (ImGui::MenuItem("Plugins...", nullptr, m_show_plugin_config)) {
                m_show_plugin_config = !m_show_plugin_config;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Video...")) {
                m_show_settings = true;
            }
            if (ImGui::MenuItem("Audio...")) {
                m_show_settings = true;
            }
            if (ImGui::MenuItem("Input...")) {
                m_show_settings = true;
            }
            if (ImGui::MenuItem("Paths...")) {
                m_show_settings = true;
            }
            ImGui::EndMenu();
        }

        // Netplay menu - rendered by the netplay plugin
        {
            auto* netplay_plugin = app.get_plugin_manager().get_netplay_plugin();
            if (netplay_plugin) {
                netplay_plugin->set_imgui_context(ImGui::GetCurrentContext());
                netplay_plugin->render_menu();
            }
        }

        // Window menu - all toggleable windows/panels
        if (ImGui::BeginMenu("Window")) {
            // Game plugins (speedrun timer, etc.)
            auto& game_plugins = app.get_plugin_manager().get_game_plugins();
            if (!game_plugins.empty()) {
                for (auto& inst : game_plugins) {
                    if (inst.plugin) {
                        const char* panel_name = inst.plugin->get_panel_name();
                        bool is_visible = inst.visible && m_show_speedrun_panel;
                        if (ImGui::MenuItem(panel_name, nullptr, is_visible)) {
                            if (is_visible) {
                                // Currently visible - hide it
                                inst.visible = false;
                            } else {
                                // Currently hidden - show it
                                m_show_speedrun_panel = true;
                                inst.visible = true;
                            }
                        }
                    }
                }
                ImGui::Separator();
            }

            // Debug Panel
            if (ImGui::MenuItem("Debug Panel", "F12", m_show_debug_panel || app.is_debug_mode())) {
                m_show_debug_panel = !m_show_debug_panel;
            }

            // RAM Watch
            if (ImGui::MenuItem("RAM Watch", nullptr, m_show_ram_watch)) {
                m_show_ram_watch = !m_show_ram_watch;
            }

            // Netplay Panel (controlled by plugin)
            {
                auto* netplay_plugin = app.get_plugin_manager().get_netplay_plugin();
                if (netplay_plugin) {
                    bool is_visible = netplay_plugin->is_panel_visible();
                    if (ImGui::MenuItem("Netplay Panel", nullptr, is_visible)) {
                        netplay_plugin->show_panel(!is_visible);
                    }
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("ImGui Demo", nullptr, m_show_demo_window)) {
                m_show_demo_window = !m_show_demo_window;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                // TODO: Show about dialog
            }
            ImGui::EndMenu();
        }

        // Show status on the right side of menu bar
        auto& pm = app.get_plugin_manager();
        if (pm.is_rom_loaded()) {
            ImGui::SameLine(ImGui::GetWindowWidth() - 200);
            ImGui::Text("%s | %s",
                       app.is_paused() ? "PAUSED" : "RUNNING",
                       pm.get_active_plugin() ?
                           pm.get_active_plugin()->get_info().name : "");
        }

        ImGui::EndMainMenuBar();
    }
}

void GuiManager::render_game_view(Application& app, Renderer& renderer) {
    ImGuiIO& io = ImGui::GetIO();

    // Create a window that fills the entire viewport (behind menu bar)
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImVec2 viewport_pos = ImGui::GetMainViewport()->Pos;
    ImVec2 viewport_size = ImGui::GetMainViewport()->Size;

    // Offset for menu bar
    float menu_height = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(ImVec2(viewport_pos.x, viewport_pos.y + menu_height));
    ImGui::SetNextWindowSize(ImVec2(viewport_size.x, viewport_size.y - menu_height));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("GameView", nullptr, window_flags);
    ImGui::PopStyleVar();

    // Get window size for scaling
    ImVec2 window_size = ImGui::GetContentRegionAvail();

    // Calculate scaled size maintaining aspect ratio
    int tex_width = renderer.get_texture_width();
    int tex_height = renderer.get_texture_height();

    if (tex_width > 0 && tex_height > 0) {
        float aspect = static_cast<float>(tex_width) / tex_height;
        float window_aspect = window_size.x / window_size.y;

        float display_width, display_height;
        if (window_aspect > aspect) {
            // Window is wider - fit to height
            display_height = window_size.y;
            display_width = display_height * aspect;
        } else {
            // Window is taller - fit to width
            display_width = window_size.x;
            display_height = display_width / aspect;
        }

        // Center the image
        float offset_x = (window_size.x - display_width) * 0.5f;
        float offset_y = (window_size.y - display_height) * 0.5f;

        ImGui::SetCursorPos(ImVec2(offset_x, offset_y));

        // Render the game texture
        ImGui::Image(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(renderer.get_texture_id())),
            ImVec2(display_width, display_height),
            ImVec2(0, 0), ImVec2(1, 1)
        );
    } else {
        // No texture - show placeholder
        ImGui::SetCursorPos(ImVec2(window_size.x / 2 - 100, window_size.y / 2));
        ImGui::Text("No ROM loaded. File > Open ROM...");
    }

    ImGui::End();
}

void GuiManager::render_rom_browser(Application& app) {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Open ROM", &m_show_rom_browser)) {
        // Current path
        ImGui::Text("Path: %s", m_current_directory.c_str());
        ImGui::Separator();

        // File list
        if (ImGui::BeginChild("FileList", ImVec2(0, -30))) {
            namespace fs = std::filesystem;

            try {
                // Parent directory entry
                if (fs::path(m_current_directory).has_parent_path()) {
                    if (ImGui::Selectable("..")) {
                        m_current_directory = fs::path(m_current_directory).parent_path().string();
                    }
                }

                // Directory entries
                std::vector<fs::directory_entry> entries;
                for (const auto& entry : fs::directory_iterator(m_current_directory)) {
                    entries.push_back(entry);
                }

                // Sort: directories first, then files
                std::sort(entries.begin(), entries.end(),
                    [](const fs::directory_entry& a, const fs::directory_entry& b) {
                        if (a.is_directory() != b.is_directory()) {
                            return a.is_directory();
                        }
                        return a.path().filename() < b.path().filename();
                    });

                for (const auto& entry : entries) {
                    std::string name = entry.path().filename().string();

                    if (entry.is_directory()) {
                        name = "[DIR] " + name;
                        if (ImGui::Selectable(name.c_str())) {
                            m_current_directory = entry.path().string();
                        }
                    } else {
                        // Check if it's a supported ROM file by querying registered plugins
                        std::string ext = entry.path().extension().string();
                        bool is_rom = false;

                        // Query the plugin registry for supported file extensions
                        const auto& registry = app.get_plugin_manager().get_registry();
                        auto emulator_plugins = registry.get_plugins_of_type(emu::PluginType::Emulator);
                        for (const auto& plugin : emulator_plugins) {
                            for (const auto& supported_ext : plugin.file_extensions) {
                                if (ext == supported_ext) {
                                    is_rom = true;
                                    break;
                                }
                            }
                            if (is_rom) break;
                        }

                        if (is_rom) {
                            if (ImGui::Selectable(name.c_str())) {
                                if (app.load_rom(entry.path().string())) {
                                    m_show_rom_browser = false;
                                }
                            }
                        } else {
                            ImGui::TextDisabled("%s", name.c_str());
                        }
                    }
                }
            } catch (const std::exception& e) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", e.what());
            }
        }
        ImGui::EndChild();

        // Buttons
        if (ImGui::Button("Cancel")) {
            m_show_rom_browser = false;
        }
    }
    ImGui::End();
}

void GuiManager::render_core_config(Application& app) {
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Core Configuration", &m_show_core_config)) {
        auto& plugin_manager = app.get_plugin_manager();
        bool game_loaded = plugin_manager.is_rom_loaded();

        // Get all loaded emulator plugin instances
        const auto& emu_plugins = plugin_manager.get_all_emulator_plugins();

        if (emu_plugins.empty()) {
            ImGui::TextDisabled("No emulator cores loaded");
            ImGui::End();
            return;
        }

        // Build combo items
        std::vector<const char*> core_names;
        core_names.reserve(emu_plugins.size());
        for (const auto& inst : emu_plugins) {
            core_names.push_back(inst.name.c_str());
        }

        // When game is loaded, lock to active core
        if (game_loaded) {
            auto* active_plugin = plugin_manager.get_active_plugin();
            if (active_plugin) {
                // Find and force-select the active core
                std::string active_name = active_plugin->get_info().name;
                // Match by checking if the registry name starts with the same prefix
                // (handles "GB" vs "GBC" dynamic naming)
                for (size_t i = 0; i < emu_plugins.size(); i++) {
                    // Check if this plugin instance is the active one
                    // by comparing the actual plugin pointers or library paths
                    if (emu_plugins[i].plugin == active_plugin ||
                        emu_plugins[i].name == active_name) {
                        m_selected_core_index = static_cast<int>(i);
                        break;
                    }
                }
            }

            ImGui::Text("Active Core:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s",
                active_plugin ? active_plugin->get_info().name : "Unknown");
            ImGui::TextDisabled("(Core is locked while a game is running)");
        } else {
            // No game loaded - allow selecting any core
            ImGui::Text("Select Core:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);

            const char* preview = (m_selected_core_index >= 0 && m_selected_core_index < static_cast<int>(core_names.size()))
                ? core_names[m_selected_core_index]
                : "Select a core...";

            if (ImGui::BeginCombo("##CoreSelector", preview)) {
                for (int i = 0; i < static_cast<int>(core_names.size()); i++) {
                    bool is_selected = (m_selected_core_index == i);
                    if (ImGui::Selectable(core_names[i], is_selected)) {
                        m_selected_core_index = i;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Separator();

        // Render the selected core's configuration
        if (m_selected_core_index >= 0 && m_selected_core_index < static_cast<int>(emu_plugins.size())) {
            // IMPORTANT: When a game is loaded, use the ACTIVE plugin instance, not the one
            // from the registry. The registry contains separate instances created for browsing,
            // while the active plugin is the one actually running the emulation.
            emu::IEmulatorPlugin* plugin = nullptr;
            std::string plugin_name;

            if (game_loaded) {
                // Use the active emulator plugin (the one running the game)
                plugin = plugin_manager.get_active_plugin();
                plugin_name = plugin ? plugin->get_info().name : "";
            } else {
                // No game loaded - use the registry instance for preview
                const auto& selected_inst = emu_plugins[m_selected_core_index];
                plugin = selected_inst.plugin;
                plugin_name = selected_inst.name;
            }

            if (plugin && plugin->has_config_gui()) {
                // Pass ImGui context to the plugin
                plugin->set_imgui_context(ImGui::GetCurrentContext());

                // Create a child region for the plugin's config
                if (ImGui::BeginChild("CoreConfigContent", ImVec2(0, 0), true)) {
                    // Let the plugin render its config content (without window wrapper)
                    plugin->render_config_gui_content();
                }
                ImGui::EndChild();
            } else if (plugin) {
                ImGui::TextDisabled("%s has no configuration options.", plugin_name.c_str());
            } else {
                ImGui::TextDisabled("Plugin instance not available.");
            }
        } else {
            ImGui::TextDisabled("Select a core from the dropdown above to view its settings.");
        }
    }
    ImGui::End();
}

void GuiManager::render_settings(Application& app) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Settings", &m_show_settings)) {
        if (ImGui::BeginTabBar("SettingsTabs")) {
            if (ImGui::BeginTabItem("General")) {
                // Pause on focus loss
                bool pause_on_focus = app.get_pause_on_focus_loss();
                if (ImGui::Checkbox("Pause when window loses focus", &pause_on_focus)) {
                    app.set_pause_on_focus_loss(pause_on_focus);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Automatically pause emulation when you click outside the window");
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Video")) {
                // Video settings
                static int scale = 2;
                ImGui::SliderInt("Scale", &scale, 1, 5);

                static bool fullscreen = false;
                if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
                    app.get_window_manager().toggle_fullscreen();
                }

                static bool vsync = true;
                if (ImGui::Checkbox("VSync", &vsync)) {
                    app.get_window_manager().set_vsync(vsync);
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Audio")) {
                // Audio settings
                static float volume = 1.0f;
                if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                    app.get_audio_manager().set_volume(volume);
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Input")) {
                // Render the input configuration panel
                if (m_input_config_panel) {
                    if (m_input_config_panel->render(app)) {
                        // Panel requested to close settings window
                        m_show_settings = false;
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Paths")) {
                // Render the paths configuration panel
                if (m_paths_config_panel) {
                    m_paths_config_panel->render(app);
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

NotificationManager& GuiManager::get_notification_manager() {
    return *m_notification_manager;
}

std::string GuiManager::format_savestate_slot_label(int slot, bool has_save, int64_t timestamp) const {
    std::ostringstream label;
    label << "Slot " << (slot + 1);

    if (has_save && timestamp > 0) {
        // Convert timestamp to readable format
        // The timestamp is stored as system_clock duration count
        auto tp = std::chrono::system_clock::time_point(
            std::chrono::system_clock::duration(timestamp));
        std::time_t time = std::chrono::system_clock::to_time_t(tp);
        std::tm* tm = std::localtime(&time);
        if (tm) {
            label << " - " << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        }
    } else if (!has_save) {
        label << " - <empty>";
    }

    return label.str();
}

void GuiManager::render_save_state_menu(Application& app) {
    bool rom_loaded = app.get_plugin_manager().is_rom_loaded();

    if (ImGui::BeginMenu("Save State", rom_loaded)) {
        auto& savestate_mgr = app.get_savestate_manager();
        auto& notifications = get_notification_manager();

        // Slot items (1-10)
        for (int slot = 0; slot < SavestateManager::NUM_SLOTS; ++slot) {
            SavestateInfo info = savestate_mgr.get_slot_info(slot);

            std::string label = format_savestate_slot_label(slot, info.valid, info.timestamp);

            // Hotkey text: Shift+F1 through Shift+F10
            std::string hotkey = "Shift+F" + std::to_string(slot + 1);

            if (ImGui::MenuItem(label.c_str(), hotkey.c_str())) {
                std::ostringstream msg;
                if (savestate_mgr.save_state(slot)) {
                    msg << "State saved to slot " << (slot + 1);
                    notifications.success(msg.str());
                } else {
                    msg << "Failed to save state to slot " << (slot + 1);
                    notifications.error(msg.str());
                }
            }
        }

        ImGui::Separator();

        // Save to file option
        if (ImGui::MenuItem("Save to file...", "Ctrl+S")) {
            m_show_savestate_browser = true;
            m_savestate_browser_is_save = true;
            // Initialize browser directory to savestates path
            auto& paths = app.get_paths_config();
            m_savestate_browser_directory = paths.get_savestate_directory().string();
        }

        ImGui::EndMenu();
    }
}

void GuiManager::render_load_state_menu(Application& app) {
    bool rom_loaded = app.get_plugin_manager().is_rom_loaded();

    if (ImGui::BeginMenu("Load State", rom_loaded)) {
        auto& savestate_mgr = app.get_savestate_manager();
        auto& notifications = get_notification_manager();

        // Slot items (1-10)
        for (int slot = 0; slot < SavestateManager::NUM_SLOTS; ++slot) {
            SavestateInfo info = savestate_mgr.get_slot_info(slot);

            std::string label = format_savestate_slot_label(slot, info.valid, info.timestamp);

            // Hotkey text: F1 through F10
            std::string hotkey = "F" + std::to_string(slot + 1);

            // Disable menu item if slot is empty
            if (ImGui::MenuItem(label.c_str(), hotkey.c_str(), false, info.valid)) {
                std::ostringstream msg;
                if (savestate_mgr.load_state(slot)) {
                    msg << "State loaded from slot " << (slot + 1);
                    notifications.success(msg.str());
                } else {
                    msg << "Failed to load state from slot " << (slot + 1);
                    notifications.error(msg.str());
                }
            }
        }

        ImGui::Separator();

        // Load from file option
        if (ImGui::MenuItem("Load from file...", "Ctrl+L")) {
            m_show_savestate_browser = true;
            m_savestate_browser_is_save = false;
            // Initialize browser directory to savestates path
            auto& paths = app.get_paths_config();
            m_savestate_browser_directory = paths.get_savestate_directory().string();
        }

        ImGui::EndMenu();
    }
}

void GuiManager::render_savestate_file_browser(Application& app) {
    const char* title = m_savestate_browser_is_save ? "Save State to File" : "Load State from File";
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(title, &m_show_savestate_browser)) {
        auto& savestate_mgr = app.get_savestate_manager();
        auto& notifications = get_notification_manager();

        // Current path
        ImGui::Text("Path: %s", m_savestate_browser_directory.c_str());
        ImGui::Separator();

        // Filename input for save mode
        static char filename_buf[256] = "savestate.state";
        if (m_savestate_browser_is_save) {
            ImGui::InputText("Filename", filename_buf, sizeof(filename_buf));
        }

        // File list
        if (ImGui::BeginChild("FileList", ImVec2(0, -30))) {
            namespace fs = std::filesystem;

            try {
                // Ensure directory exists
                if (!fs::exists(m_savestate_browser_directory)) {
                    fs::create_directories(m_savestate_browser_directory);
                }

                // Parent directory entry
                if (fs::path(m_savestate_browser_directory).has_parent_path()) {
                    if (ImGui::Selectable("..")) {
                        m_savestate_browser_directory = fs::path(m_savestate_browser_directory).parent_path().string();
                    }
                }

                // Directory entries
                std::vector<fs::directory_entry> entries;
                for (const auto& entry : fs::directory_iterator(m_savestate_browser_directory)) {
                    entries.push_back(entry);
                }

                // Sort: directories first, then files
                std::sort(entries.begin(), entries.end(),
                    [](const fs::directory_entry& a, const fs::directory_entry& b) {
                        if (a.is_directory() != b.is_directory()) {
                            return a.is_directory();
                        }
                        return a.path().filename() < b.path().filename();
                    });

                for (const auto& entry : entries) {
                    std::string name = entry.path().filename().string();

                    if (entry.is_directory()) {
                        name = "[DIR] " + name;
                        if (ImGui::Selectable(name.c_str())) {
                            m_savestate_browser_directory = entry.path().string();
                        }
                    } else {
                        // Check if it's a savestate file (.state extension)
                        std::string ext = entry.path().extension().string();
                        bool is_savestate = (ext == ".state" || ext == ".sav" || ext == ".ss");

                        if (is_savestate) {
                            if (ImGui::Selectable(name.c_str())) {
                                if (m_savestate_browser_is_save) {
                                    // Copy filename to input
                                    std::strncpy(filename_buf, name.c_str(), sizeof(filename_buf) - 1);
                                } else {
                                    // Load the file
                                    std::string path = entry.path().string();
                                    if (savestate_mgr.load_state_from_file(path)) {
                                        notifications.success("State loaded from " + name);
                                        m_show_savestate_browser = false;
                                    } else {
                                        notifications.error("Failed to load state from " + name);
                                    }
                                }
                            }
                        } else {
                            ImGui::TextDisabled("%s", name.c_str());
                        }
                    }
                }
            } catch (const std::exception& e) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", e.what());
            }
        }
        ImGui::EndChild();

        // Buttons
        if (m_savestate_browser_is_save) {
            if (ImGui::Button("Save")) {
                namespace fs = std::filesystem;
                std::string path = (fs::path(m_savestate_browser_directory) / filename_buf).string();
                if (savestate_mgr.save_state_to_file(path)) {
                    notifications.success("State saved to " + std::string(filename_buf));
                    m_show_savestate_browser = false;
                } else {
                    notifications.error("Failed to save state to " + std::string(filename_buf));
                }
            }
            ImGui::SameLine();
        }

        if (ImGui::Button("Cancel")) {
            m_show_savestate_browser = false;
        }
    }
    ImGui::End();
}

} // namespace emu
