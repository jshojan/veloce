#pragma once

#include "emu/netplay_plugin.hpp"
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
class SavestateManager;
class PathsConfiguration;
class INetplayCapable;

// Main application class - orchestrates all subsystems
// Also implements INetplayHost to provide callbacks to the netplay plugin
class Application : public INetplayHost {
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
    SavestateManager& get_savestate_manager() { return *m_savestate_manager; }
    PathsConfiguration& get_paths_config() { return *m_paths_config; }

    // Emulation control
    void pause();
    void resume();
    void reset();
    void toggle_pause();
    bool is_paused() const { return m_paused; }
    bool is_running() const { return m_running; }
    void request_quit() { m_quit_requested = true; }

    // Focus handling
    void set_pause_on_focus_loss(bool enabled) { m_pause_on_focus_loss = enabled; }
    bool get_pause_on_focus_loss() const { return m_pause_on_focus_loss; }

    // Frame control
    void frame_advance();
    void set_speed(float speed) { m_speed_multiplier = speed; }
    float get_speed() const { return m_speed_multiplier; }

    // Debug mode
    bool is_debug_mode() const { return m_debug_mode; }
    void set_debug_mode(bool enabled) { m_debug_mode = enabled; }
    void toggle_debug_mode() { m_debug_mode = !m_debug_mode; }

    // Screenshot
    bool save_screenshot(const std::string& path = "");
    void request_screenshot() { m_screenshot_requested = true; }

    // Netplay state cache management
    // Called when netplay connects/disconnects to update cached values
    void update_netplay_cache();

    // =========================================================================
    // INetplayHost Implementation
    // =========================================================================

    void pause_emulator() override;
    void resume_emulator() override;
    bool is_emulator_paused() const override;
    void reset_emulator() override;

    uint64_t get_frame_count() const override;
    double get_fps() const override;

    bool is_rom_loaded() const override;
    const char* get_rom_name() const override;
    uint32_t get_rom_crc32() const override;
    const char* get_platform_name() const override;

    bool save_state_to_buffer(std::vector<uint8_t>& buffer) override;
    bool load_state_from_buffer(const std::vector<uint8_t>& buffer) override;

    void set_controller_input(int controller, uint32_t buttons) override;
    uint32_t get_local_input(int controller) const override;

    const char* get_config_directory() const override;
    void show_notification(NetplayNotificationType type, const char* message, float duration = 3.0f) override;

    void on_netplay_connected(int player_id) override;
    void on_netplay_disconnected(const char* reason) override;
    void on_netplay_player_joined(const NetplayPlayer& player) override;
    void on_netplay_player_left(int player_id, const char* reason) override;
    void on_netplay_desync(const DesyncInfo& info) override;
    void on_netplay_chat_message(int player_id, const char* message) override;

private:
    bool parse_command_line(int argc, char* argv[], std::string& rom_path);
    void print_usage(const char* program_name);
    void print_version();

    void process_events();
    void update();
    void render();
    void run_emulation_frame();

    // Get INetplayCapable interface from current emulator if available
    INetplayCapable* get_netplay_capable_emulator() const;

    // Subsystems
    std::unique_ptr<WindowManager> m_window_manager;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<InputManager> m_input_manager;
    std::unique_ptr<AudioManager> m_audio_manager;
    std::unique_ptr<PluginManager> m_plugin_manager;
    std::unique_ptr<GuiManager> m_gui_manager;
    std::unique_ptr<SavestateManager> m_savestate_manager;
    std::unique_ptr<PathsConfiguration> m_paths_config;

    // State
    bool m_running = false;
    bool m_paused = true;  // Start paused until ROM loaded
    bool m_quit_requested = false;
    bool m_frame_advance_requested = false;
    bool m_debug_mode = false;
    bool m_headless_mode = false;  // Run without GUI for testing
    int m_headless_frames = 0;     // Number of frames to run in headless mode (0 = unlimited)
    float m_speed_multiplier = 1.0f;

    // Screenshot
    bool m_screenshot_requested = false;
    int m_screenshot_at_frame = -1;  // Frame number to auto-screenshot (-1 = disabled)
    std::string m_screenshot_output_path;  // Custom output path for screenshot

    // Focus handling
    bool m_pause_on_focus_loss = true;  // Pause emulation when window loses focus
    bool m_focus_paused = false;        // True if currently paused due to focus loss
    bool m_was_paused_before_focus = false;  // Was paused before losing focus?

    // Timing
    uint64_t m_last_frame_time = 0;
    double m_frame_accumulator = 0.0;

    // Netplay optimization: cached state to avoid per-frame overhead when netplay is inactive
    // These are updated when netplay connects/disconnects, not every frame
    bool m_netplay_active_cached = false;
    INetplayCapable* m_netplay_capable_plugin = nullptr;
    std::vector<uint32_t> m_netplay_inputs_buffer;  // Pre-allocated buffer for netplay inputs

    // Cached strings for INetplayHost (to avoid dangling pointers)
    mutable std::string m_cached_rom_name;
    mutable std::string m_cached_platform_name;
    mutable std::string m_cached_config_dir;
};

// Global application instance access
Application& get_application();

} // namespace emu
