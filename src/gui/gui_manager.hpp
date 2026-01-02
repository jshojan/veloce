#pragma once

#include <string>
#include <memory>

struct SDL_Window;
typedef void* SDL_GLContext;
union SDL_Event;

namespace emu {

class Application;
class WindowManager;
class Renderer;
class SpeedrunPanel;
class DebugPanel;
class InputConfigPanel;
class PluginConfigPanel;
class PathsConfigPanel;
class NotificationManager;
class NetplayPanel;

class GuiManager {
public:
    GuiManager();
    ~GuiManager();

    // Disable copy
    GuiManager(const GuiManager&) = delete;
    GuiManager& operator=(const GuiManager&) = delete;

    // Initialize Dear ImGui
    bool initialize(WindowManager& window_manager);

    // Shutdown
    void shutdown();

    // Process SDL events
    void process_event(const SDL_Event& event);

    // Begin new frame
    void begin_frame();

    // Render all GUI elements
    void render(Application& app, Renderer& renderer);

    // End frame and render
    void end_frame();

    // GUI state
    bool wants_keyboard() const;
    bool wants_mouse() const;

    // Window visibility
    void show_rom_browser(bool show) { m_show_rom_browser = show; }
    void show_settings(bool show) { m_show_settings = show; }
    void show_ram_watch(bool show) { m_show_ram_watch = show; }
    void show_speedrun_panel(bool show) { m_show_speedrun_panel = show; }
    void show_plugin_config(bool show) { m_show_plugin_config = show; }
    void show_netplay_panel(bool show) { m_show_netplay_panel = show; }

    // Netplay panel access for menu commands
    NetplayPanel& get_netplay_panel();

    // Notification system access
    NotificationManager& get_notification_manager();

private:
    void render_main_menu(Application& app);
    void render_game_view(Application& app, Renderer& renderer);
    void render_rom_browser(Application& app);
    void render_settings(Application& app);
    void render_savestate_file_browser(Application& app);

    // Savestate menu helpers
    void render_save_state_menu(Application& app);
    void render_load_state_menu(Application& app);
    std::string format_savestate_slot_label(int slot, bool has_save, int64_t timestamp) const;

    bool m_initialized = false;

    // Window visibility flags
    bool m_show_rom_browser = false;
    bool m_show_settings = false;
    bool m_show_ram_watch = false;
    bool m_show_speedrun_panel = true;  // Show by default
    bool m_show_debug_panel = false;
    bool m_show_plugin_config = false;
    bool m_show_netplay_panel = false;
    bool m_show_demo_window = false;
    bool m_show_savestate_browser = false;
    bool m_savestate_browser_is_save = false;  // true = save mode, false = load mode

    // ROM browser state
    std::string m_current_directory;

    // Savestate file browser state
    std::string m_savestate_browser_directory;

    // Panels
    std::unique_ptr<SpeedrunPanel> m_speedrun_panel;
    std::unique_ptr<DebugPanel> m_debug_panel;
    std::unique_ptr<InputConfigPanel> m_input_config_panel;
    std::unique_ptr<PluginConfigPanel> m_plugin_config_panel;
    std::unique_ptr<PathsConfigPanel> m_paths_config_panel;
    std::unique_ptr<NotificationManager> m_notification_manager;
    std::unique_ptr<NetplayPanel> m_netplay_panel;
};

} // namespace emu
