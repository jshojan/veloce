#pragma once

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace emu {

class WindowManager;
class Renderer;
class InputManager;
class AudioManager;
class PluginManager;
class GuiManager;
class SpeedrunManager;
class SavestateManager;
class PathsConfiguration;
class NetplayManager;
class INetplayCapable;

// Main application class - orchestrates all subsystems
class Application {
public:
    Application();
    ~Application();

    // Disable copy
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Initialize all subsystems
    bool initialize(int argc, char* argv[]);

    // Main loop
    void run();

    // Shutdown and cleanup
    void shutdown();

    // Load a ROM file
    bool load_rom(const std::string& path);

    // Accessors for subsystems
    WindowManager& get_window_manager() { return *m_window_manager; }
    Renderer& get_renderer() { return *m_renderer; }
    InputManager& get_input_manager() { return *m_input_manager; }
    AudioManager& get_audio_manager() { return *m_audio_manager; }
    PluginManager& get_plugin_manager() { return *m_plugin_manager; }
    GuiManager& get_gui_manager() { return *m_gui_manager; }
    SpeedrunManager& get_speedrun_manager() { return *m_speedrun_manager; }
    SavestateManager& get_savestate_manager() { return *m_savestate_manager; }
    PathsConfiguration& get_paths_config() { return *m_paths_config; }
    NetplayManager& get_netplay_manager() { return *m_netplay_manager; }

    // Emulation control
    void pause();
    void resume();
    void reset();
    void toggle_pause();
    bool is_paused() const { return m_paused; }
    bool is_running() const { return m_running; }
    void request_quit() { m_quit_requested = true; }

    // Frame control
    void frame_advance();
    void set_speed(float speed) { m_speed_multiplier = speed; }
    float get_speed() const { return m_speed_multiplier; }

    // Debug mode
    bool is_debug_mode() const { return m_debug_mode; }
    void set_debug_mode(bool enabled) { m_debug_mode = enabled; }
    void toggle_debug_mode() { m_debug_mode = !m_debug_mode; }

private:
    bool parse_command_line(int argc, char* argv[], std::string& rom_path);
    void print_usage(const char* program_name);
    void print_version();

    void process_events();
    void update();
    void render();
    void run_emulation_frame();

    // Subsystems
    std::unique_ptr<WindowManager> m_window_manager;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<InputManager> m_input_manager;
    std::unique_ptr<AudioManager> m_audio_manager;
    std::unique_ptr<PluginManager> m_plugin_manager;
    std::unique_ptr<GuiManager> m_gui_manager;
    std::unique_ptr<SpeedrunManager> m_speedrun_manager;
    std::unique_ptr<SavestateManager> m_savestate_manager;
    std::unique_ptr<PathsConfiguration> m_paths_config;
    std::unique_ptr<NetplayManager> m_netplay_manager;

    // State
    bool m_running = false;
    bool m_paused = true;  // Start paused until ROM loaded
    bool m_quit_requested = false;
    bool m_frame_advance_requested = false;
    bool m_debug_mode = false;
    bool m_headless_mode = false;  // Run without GUI for testing
    int m_headless_frames = 0;     // Number of frames to run in headless mode (0 = unlimited)
    float m_speed_multiplier = 1.0f;

    // Timing
    uint64_t m_last_frame_time = 0;
    double m_frame_accumulator = 0.0;

    // Netplay optimization: cached state to avoid per-frame overhead when netplay is inactive
    // These are updated when netplay connects/disconnects, not every frame
    bool m_netplay_active_cached = false;
    INetplayCapable* m_netplay_capable_plugin = nullptr;
    std::vector<uint32_t> m_netplay_inputs_buffer;  // Pre-allocated buffer for netplay inputs

    // Called when netplay state changes to update cached values
    void update_netplay_cache();
    friend class NetplayManager;
};

// Global application instance access
Application& get_application();

} // namespace emu
