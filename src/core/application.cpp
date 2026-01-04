#include "application.hpp"
#include "window_manager.hpp"
#include "renderer.hpp"
#include "input_manager.hpp"
#include "audio_manager.hpp"
#include "plugin_manager.hpp"
#include "savestate_manager.hpp"
#include "paths_config.hpp"
#include "screenshot.hpp"
#include "gui/gui_manager.hpp"
#include "gui/notification_manager.hpp"
#include "emu/controller_layout.hpp"
#include "emu/emulator_plugin.hpp"

#include <SDL.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <filesystem>

namespace emu {

// Global application instance
static Application* g_application = nullptr;

Application& get_application() {
    return *g_application;
}

Application::Application() {
    g_application = this;
}

Application::~Application() {
    if (g_application == this) {
        g_application = nullptr;
    }
}

void Application::print_usage(const char* program_name) {
    std::cout << "Veloce - A plugin-based emulator framework for speedrunners\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] [ROM_FILE]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help       Show this help message and exit\n";
    std::cout << "  -v, --version    Show version information and exit\n";
    std::cout << "  -d, --debug      Enable debug mode (show CPU/PPU state)\n";
    std::cout << "\n";
    std::cout << "Environment Variables:\n";
    std::cout << "  DEBUG=1          Enable debug output\n";
    std::cout << "  HEADLESS=1       Run without GUI (for automated testing)\n";
    std::cout << "  FRAMES=N         Run for N frames then exit (requires HEADLESS=1)\n";
    std::cout << "  SAVE_SCREENSHOT=N      Save screenshot at frame N\n";
    std::cout << "  SAVE_SCREENSHOT=path   Save screenshot at exit to specified path\n";
    std::cout << "\n";
    std::cout << "ROM_FILE:\n";
    std::cout << "  Optional path to a ROM file to load on startup.\n";
    std::cout << "  Supported formats: .nes (NES), .sfc/.smc (SNES), .gb/.gbc (GB), .gba (GBA)\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " game.nes                        # Load and run a NES ROM\n";
    std::cout << "  " << program_name << " --debug game.nes                # Load with debug mode\n";
    std::cout << "  " << program_name << "                                 # Start without loading a ROM\n";
    std::cout << "  HEADLESS=1 FRAMES=600 " << program_name << " test.sfc  # Run test ROM headless\n";
}

void Application::print_version() {
    std::cout << "Veloce v0.1.0\n";
    std::cout << "Built for speedrunners with cycle-accurate emulation.\n";
    std::cout << "Supported systems: NES\n";
}

bool Application::parse_command_line(int argc, char* argv[], std::string& rom_path) {
    rom_path.clear();

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return false;  // Signal to exit
        }
        else if (std::strcmp(arg, "-v") == 0 || std::strcmp(arg, "--version") == 0) {
            print_version();
            return false;  // Signal to exit
        }
        else if (std::strcmp(arg, "-d") == 0 || std::strcmp(arg, "--debug") == 0) {
            m_debug_mode = true;
            std::cout << "Debug mode enabled\n";
        }
        else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            return false;
        }
        else {
            // Assume it's a ROM path
            rom_path = arg;
        }
    }

    return true;
}

bool Application::initialize(int argc, char* argv[]) {
    // Parse command line arguments
    std::string rom_path;
    if (!parse_command_line(argc, argv, rom_path)) {
        m_running = false;
        return true;  // Not an error, just exit gracefully
    }

    // Check for HEADLESS environment variable
    const char* headless_env = std::getenv("HEADLESS");
    if (headless_env && headless_env[0] != '0') {
        m_headless_mode = true;
    }

    // Check for FRAMES environment variable
    const char* frames_env = std::getenv("FRAMES");
    if (frames_env) {
        m_headless_frames = std::atoi(frames_env);
        if (m_headless_frames <= 0) {
            m_headless_frames = 600;  // Default to 10 seconds at 60fps
        }
    }

    // Check for SAVE_SCREENSHOT environment variable (frame number or path)
    const char* screenshot_env = std::getenv("SAVE_SCREENSHOT");
    if (screenshot_env) {
        // Check if it's a number (frame to screenshot) or a path
        int frame_num = std::atoi(screenshot_env);
        if (frame_num > 0) {
            m_screenshot_at_frame = frame_num;
        } else if (screenshot_env[0] != '\0') {
            // Treat as output path, screenshot at last frame
            m_screenshot_output_path = screenshot_env;
            m_screenshot_at_frame = -2;  // -2 means screenshot at exit
        }
    }

    // Headless mode validation
    if (m_headless_mode) {
        if (rom_path.empty()) {
            std::cerr << "Error: HEADLESS=1 requires a ROM file\n";
            return false;
        }
        if (m_headless_frames == 0) {
            m_headless_frames = 600;  // Default to 10 seconds at 60fps
        }
    }

    // In headless mode, skip most GUI/SDL initialization
    if (!m_headless_mode) {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return false;
        }

        // Create subsystems
        m_window_manager = std::make_unique<WindowManager>();
        m_renderer = std::make_unique<Renderer>();
        m_input_manager = std::make_unique<InputManager>();
        m_audio_manager = std::make_unique<AudioManager>();
        m_gui_manager = std::make_unique<GuiManager>();
    }

    m_plugin_manager = std::make_unique<PluginManager>();
    m_savestate_manager = std::make_unique<SavestateManager>();
    m_paths_config = std::make_unique<PathsConfiguration>();

    // Initialize paths configuration first (other subsystems may depend on it)
    std::filesystem::path exe_dir = std::filesystem::current_path();
    m_paths_config->initialize(exe_dir);
    m_paths_config->load();
    m_paths_config->ensure_directories_exist();

    if (!m_headless_mode) {
        // Initialize window
        WindowConfig window_config;
        window_config.title = "Veloce";
        window_config.width = 1024;
        window_config.height = 768;

        if (!m_window_manager->initialize(window_config)) {
            std::cerr << "Failed to initialize window manager" << std::endl;
            return false;
        }

        // Initialize renderer
        if (!m_renderer->initialize(*m_window_manager)) {
            std::cerr << "Failed to initialize renderer" << std::endl;
            return false;
        }

        // Initialize input
        if (!m_input_manager->initialize()) {
            std::cerr << "Failed to initialize input manager" << std::endl;
            return false;
        }

        // Initialize audio
        if (!m_audio_manager->initialize()) {
            std::cerr << "Failed to initialize audio manager" << std::endl;
            return false;
        }
    }

    // Initialize plugin manager
    if (!m_plugin_manager->initialize()) {
        std::cerr << "Failed to initialize plugin manager" << std::endl;
        return false;
    }

    // Set paths configuration for plugin manager (for battery saves)
    m_plugin_manager->set_paths_config(m_paths_config.get());

    // Load plugin configuration from config directory
    std::filesystem::path plugin_config_path = m_paths_config->get_config_directory() / "plugins.json";
    m_plugin_manager->load_config(plugin_config_path.string());

    if (!m_headless_mode) {
        // Initialize GUI
        if (!m_gui_manager->initialize(*m_window_manager)) {
            std::cerr << "Failed to initialize GUI manager" << std::endl;
            return false;
        }
    }

    // Initialize savestate manager with paths configuration
    m_savestate_manager->initialize(m_plugin_manager.get(), m_paths_config.get());

    // Set this application as the netplay host for the plugin manager
    m_plugin_manager->set_netplay_host(this);

    // Initialize the netplay plugin with this application as its host
    if (auto* netplay_plugin = m_plugin_manager->get_netplay_plugin()) {
        netplay_plugin->initialize(this);
    }

    // Load ROM if provided on command line
    if (!rom_path.empty()) {
        if (!load_rom(rom_path)) {
            if (m_headless_mode) {
                std::cerr << "Failed to load ROM: " << rom_path << std::endl;
                return false;
            }
        }
    }

    m_running = true;
    if (!m_headless_mode) {
        m_last_frame_time = WindowManager::get_ticks();
        std::cout << "Veloce initialized successfully" << std::endl;
    }
    return true;
}

void Application::run() {
    // Headless mode - run without GUI for automated testing
    if (m_headless_mode) {
        auto* active_plugin = m_plugin_manager->get_active_plugin();
        if (!active_plugin || !active_plugin->is_rom_loaded()) {
            std::cerr << "No ROM loaded for headless mode\n";
            return;
        }

        emu::InputState empty_input{};
        int frames_run = 0;

        while (m_running && !m_quit_requested && frames_run < m_headless_frames) {
            active_plugin->run_frame(empty_input);
            frames_run++;

            // Check for screenshot at specific frame
            if (m_screenshot_at_frame > 0 && frames_run == m_screenshot_at_frame) {
                std::string path = m_screenshot_output_path.empty()
                    ? ("screenshot_frame_" + std::to_string(frames_run) + ".png")
                    : m_screenshot_output_path;
                save_screenshot(path);
            }
        }

        // Screenshot at exit (-2) or last frame
        if (m_screenshot_at_frame == -2 || m_screenshot_requested) {
            std::string path = m_screenshot_output_path.empty()
                ? ("screenshot_final.png")
                : m_screenshot_output_path;
            save_screenshot(path);
        }

        std::cerr << "Headless mode: Ran " << frames_run << " frames\n";
        return;
    }

    // Frame timing is determined by the active emulator plugin's native FPS.
    // Examples:
    //   - NES (NTSC): 60.0988 fps (21.477272 MHz / 4 / 262 / 341)
    //   - GB/GBC:     59.7275 fps (4.194304 MHz / 70224 cycles per frame)
    //   - GBA:        59.7275 fps (16.78 MHz / 280896 cycles per frame)
    // Hardware-accurate timing is critical for speedruns and TAS.
    // Default to 60 FPS when no plugin is active.
    double target_fps = 60.0;
    bool audio_started = false;

    // Set audio sync mode - DynamicRate is the default for TAS compatibility
    // It maintains deterministic frame timing while achieving low audio latency
    // through subtle resampling (max +/-0.5%, completely inaudible)
    m_audio_manager->set_sync_mode(AudioSyncMode::DynamicRate);

    while (m_running && !m_quit_requested) {
        uint64_t frame_start = WindowManager::get_ticks();
        double frequency = static_cast<double>(WindowManager::get_performance_frequency());

        // Process events
        process_events();

        // Update input
        m_input_manager->update();

        // Update target FPS from active plugin (may change when loading different ROMs)
        auto* active_plugin = m_plugin_manager->get_active_plugin();
        if (active_plugin && active_plugin->is_rom_loaded()) {
            target_fps = active_plugin->get_info().native_fps;
        }
        double target_frame_time = 1.0 / target_fps;

        // Run emulation if not paused
        if (!m_paused || m_frame_advance_requested) {
            run_emulation_frame();
            m_frame_advance_requested = false;

            // Start audio playback once buffer has enough samples
            // With DynamicRate mode, this threshold is much lower (~24ms vs 139ms)
            if (!audio_started && m_audio_manager->is_buffer_ready()) {
                m_audio_manager->resume();
                audio_started = true;
            }
        } else {
            audio_started = false;  // Reset when paused
        }

        // Render
        render();

        // Hardware-accurate frame timing with precision spin-wait
        // Dynamic rate control in the audio system compensates for any minor
        // timing drift, allowing us to use spin-waiting for precise frame pacing.
        uint64_t frame_end = WindowManager::get_ticks();
        double frame_time = static_cast<double>(frame_end - frame_start) / frequency;
        double adjusted_target = target_frame_time / m_speed_multiplier;

        // Check if the emulator core has fast mode enabled (e.g., "overclock" setting)
        // When fast mode is enabled, skip frame timing entirely
        // Note: Re-fetch active_plugin as it may have changed during render() (e.g., ROM load from GUI)
        auto* current_plugin = m_plugin_manager->get_active_plugin();
        bool core_fast_mode = current_plugin && current_plugin->is_fast_mode_enabled();

        // Sleep to maintain target frame rate
        // Skip timing if: speed multiplier != 1.0, or core requests fast mode
        if (m_speed_multiplier == 1.0f && !core_fast_mode && frame_time < adjusted_target) {
            double sleep_time = (adjusted_target - frame_time) * 1000.0;
            // Use a more accurate sleep by sleeping slightly less and spinning
            if (sleep_time > 2.0) {
                SDL_Delay(static_cast<uint32_t>(sleep_time - 1.0));
            }
            // Spin for remaining time for more accurate timing
            while (true) {
                uint64_t now = WindowManager::get_ticks();
                double elapsed = static_cast<double>(now - frame_start) / frequency;
                if (elapsed >= adjusted_target) break;
            }
        }
    }
}

void Application::shutdown() {
    // Save input config before shutdown (not in headless mode)
    if (m_input_manager) {
        m_input_manager->save_platform_config(m_input_manager->get_current_platform());
    }

    // Save paths configuration
    if (m_paths_config && m_paths_config->is_modified()) {
        m_paths_config->save();
    }

    // Save plugin configuration
    if (m_plugin_manager && m_paths_config) {
        std::filesystem::path plugin_config_path = m_paths_config->get_config_directory() / "plugins.json";
        m_plugin_manager->save_config(plugin_config_path.string());
    }

    // Shutdown and destroy managers in controlled order
    // This prevents issues from the destructor trying to clean up after SDL_Quit
    if (m_gui_manager) {
        m_gui_manager->shutdown();
        m_gui_manager.reset();
    }
    if (m_savestate_manager) {
        m_savestate_manager.reset();
    }
    if (m_plugin_manager) {
        m_plugin_manager->shutdown();
        m_plugin_manager.reset();
    }
    if (m_audio_manager) {
        m_audio_manager->shutdown();
        m_audio_manager.reset();
    }
    if (m_input_manager) {
        m_input_manager->shutdown();
        m_input_manager.reset();
    }
    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
    }
    if (m_paths_config) {
        m_paths_config.reset();
    }
    if (m_window_manager) {
        m_window_manager->shutdown();
        m_window_manager.reset();
    }

    if (!m_headless_mode) {
        SDL_Quit();
        std::cout << "Veloce shutdown complete" << std::endl;
    }
}

void Application::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Let GUI handle events first
        m_gui_manager->process_event(event);

        // Process input events
        m_input_manager->process_event(event);

        switch (event.type) {
            case SDL_QUIT:
                m_quit_requested = true;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    m_quit_requested = true;
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    // Pause when window loses focus (if enabled and ROM loaded)
                    if (m_pause_on_focus_loss && m_plugin_manager->is_rom_loaded()) {
                        m_was_paused_before_focus = m_paused;
                        if (!m_paused) {
                            m_focus_paused = true;
                            pause();
                        }
                    }
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    // Resume when window regains focus (if we paused due to focus loss)
                    if (m_focus_paused) {
                        m_focus_paused = false;
                        if (!m_was_paused_before_focus) {
                            resume();
                        }
                    }
                }
                break;

            case SDL_KEYDOWN:
                // Handle hotkeys (only if GUI doesn't want keyboard)
                if (!m_gui_manager->wants_keyboard()) {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            toggle_pause();
                            break;
                        case SDLK_r:
                            if (event.key.keysym.mod & KMOD_CTRL) {
                                reset();
                            }
                            break;
                        case SDLK_f:
                            if (!(event.key.keysym.mod & KMOD_CTRL)) {
                                frame_advance();
                            }
                            break;
                        case SDLK_F11:
                            m_window_manager->toggle_fullscreen();
                            break;
                        case SDLK_F12:
                            toggle_debug_mode();
                            std::cout << "Debug mode: " << (m_debug_mode ? "ON" : "OFF") << std::endl;
                            break;

                        case SDLK_PRINTSCREEN:
                            if (save_screenshot()) {
                                m_gui_manager->get_notification_manager().success("Screenshot saved");
                            } else {
                                m_gui_manager->get_notification_manager().error("Failed to save screenshot");
                            }
                            break;

                        // Savestate hotkeys: Shift+F1-F10 to save, F1-F10 to load
                        case SDLK_F1:
                        case SDLK_F2:
                        case SDLK_F3:
                        case SDLK_F4:
                        case SDLK_F5:
                        case SDLK_F6:
                        case SDLK_F7:
                        case SDLK_F8:
                        case SDLK_F9:
                        case SDLK_F10: {
                            int slot = event.key.keysym.sym - SDLK_F1;  // 0-9
                            auto& notifications = m_gui_manager->get_notification_manager();
                            std::ostringstream msg;

                            if (event.key.keysym.mod & KMOD_SHIFT) {
                                // Save state (Shift+F1-F10)
                                if (m_savestate_manager->save_state(slot)) {
                                    msg << "State saved to slot " << (slot + 1);
                                    notifications.success(msg.str());
                                    std::cout << msg.str() << std::endl;
                                } else {
                                    msg << "Failed to save state to slot " << (slot + 1);
                                    notifications.error(msg.str());
                                    std::cout << msg.str() << std::endl;
                                }
                            } else {
                                // Load state (F1-F10)
                                if (m_savestate_manager->load_state(slot)) {
                                    msg << "State loaded from slot " << (slot + 1);
                                    notifications.success(msg.str());
                                    std::cout << msg.str() << std::endl;
                                } else {
                                    msg << "Failed to load state from slot " << (slot + 1);
                                    notifications.error(msg.str());
                                    std::cout << msg.str() << std::endl;
                                }
                            }
                            break;
                        }
                    }
                }
                break;

            case SDL_DROPFILE:
                load_rom(event.drop.file);
                SDL_free(event.drop.file);
                break;
        }
    }
}

void Application::run_emulation_frame() {
    auto* plugin = m_plugin_manager->get_active_plugin();
    if (!plugin || !plugin->is_rom_loaded()) {
        return;
    }

    // Fast path: check cached netplay active status first
    // This avoids expensive is_connected() call which does plugin lookup + virtual calls
    bool netplay_active = m_netplay_active_cached;

    if (netplay_active) {
        auto* netplay = m_plugin_manager->get_netplay_plugin();
        if (netplay) {
            // Netplay mode: use synchronized input
            if (!netplay->begin_frame()) {
                // Waiting for network sync, skip this frame
                return;
            }

            // Get synchronized inputs for all players using pre-allocated buffer
            uint64_t frame = plugin->get_frame_count();
            netplay->get_synchronized_inputs_fast(m_netplay_inputs_buffer, frame);

            // Check if emulator supports netplay multi-player input
            // Cache this check - it doesn't change during gameplay
            if (m_netplay_capable_plugin) {
                // Use N-player method for flexible player count support
                m_netplay_capable_plugin->run_frame_netplay_n(m_netplay_inputs_buffer);
            } else {
                // Fallback: use local player input only
                InputState input;
                int local_id = netplay->get_local_player_id();
                input.buttons = (local_id >= 0 && local_id < static_cast<int>(m_netplay_inputs_buffer.size()))
                              ? m_netplay_inputs_buffer[local_id] : 0;
                plugin->run_frame(input);
            }

            netplay->end_frame();
        }
    } else {
        // Normal single-player mode - zero netplay overhead
        InputState input;
        input.buttons = m_input_manager->get_button_state();
        plugin->run_frame(input);
    }

    // Update game plugins (for timer updates and auto-split detection)
    m_plugin_manager->update_game_plugins();

    // Audio handling: streaming vs legacy path
    // Streaming audio (has_audio_callback) pushes samples during emulation for lowest latency
    // Legacy path batches until frame end (higher latency but compatible with all plugins)
    if (!plugin->has_audio_callback()) {
        // Legacy audio path: get samples at frame end
        auto* netplay = m_plugin_manager->get_netplay_plugin();
        bool skip_audio = netplay_active && netplay && netplay->is_rolling_back();
        if (!skip_audio) {
            AudioBuffer audio = plugin->get_audio();
            if (audio.samples && audio.sample_count > 0) {
                // Resample if source rate differs from output rate
                m_audio_manager->push_samples_resampled(audio.samples, audio.sample_count * 2,
                                                         audio.sample_rate);
                plugin->clear_audio_buffer();
            }
        } else {
            // Discard audio during rollback
            plugin->clear_audio_buffer();
        }
    }
    // Streaming path: samples already pushed during run_frame() via callback

    // Update texture with framebuffer
    FrameBuffer fb = plugin->get_framebuffer();
    if (fb.pixels) {
        m_renderer->update_texture(fb.pixels, fb.width, fb.height);
    }
}

void Application::render() {
    m_renderer->clear();
    m_gui_manager->begin_frame();
    m_gui_manager->render(*this, *m_renderer);
    m_gui_manager->end_frame();
    m_window_manager->swap_buffers();
}

bool Application::load_rom(const std::string& path) {
    std::cout << "Loading ROM: " << path << std::endl;

    // Find appropriate plugin for this file type
    if (!m_plugin_manager->set_active_plugin_for_file(path)) {
        std::cerr << "No plugin found for file: " << path << std::endl;
        return false;
    }

    // Load ROM using the path-based method
    // This handles file reading, battery saves, and game plugin initialization
    if (!m_plugin_manager->load_rom(path)) {
        std::cerr << "Failed to load ROM" << std::endl;
        return false;
    }

    // Get controller layout from emulator plugin and pass to input manager (not in headless mode)
    if (!m_headless_mode) {
        auto* plugin = m_plugin_manager->get_active_plugin();
        if (plugin) {
            // Get controller layout from the emulator plugin
            // The plugin is responsible for defining its own layout
            const ControllerLayoutInfo* layout = plugin->get_controller_layout();

            // Set the controller layout (this also updates the platform)
            m_input_manager->set_controller_layout(layout);
        }

        // Update window title
        m_window_manager->set_title("Veloce - " + path);
    }

    // Set up streaming audio callback for lowest latency
    // The callback pushes small batches of samples (~64 = 1.5ms) during emulation
    // rather than waiting until frame end (~16.67ms)
    if (!m_headless_mode) {
        auto* active_plugin = m_plugin_manager->get_active_plugin();
        if (active_plugin) {
            // Capture audio manager and netplay plugin pointer for the callback
            AudioManager* audio_mgr = m_audio_manager.get();
            PluginManager* plugin_mgr = m_plugin_manager.get();

            active_plugin->set_audio_callback(
                [audio_mgr, plugin_mgr](const float* samples, size_t count, int rate) {
                    // Skip audio during netplay rollback to avoid artifacts
                    auto* netplay = plugin_mgr->get_netplay_plugin();
                    if (netplay && netplay->is_rolling_back()) {
                        return;
                    }
                    // Push samples with resampling (count is stereo pairs)
                    audio_mgr->push_samples_resampled(samples, count * 2, rate);
                });
        }
    }

    // Unpause emulation - audio will start automatically once buffer is ready
    // This prevents initial audio crackling from buffer underruns
    m_paused = false;
    // Note: audio_manager->resume() is called in run() once buffer is ready

    std::cout << "ROM loaded successfully" << std::endl;
    return true;
}

void Application::pause() {
    m_paused = true;
    m_audio_manager->pause();
    m_plugin_manager->set_paused(true);
}

void Application::resume() {
    m_paused = false;
    m_audio_manager->resume();
    m_plugin_manager->set_paused(false);
}

void Application::reset() {
    auto* plugin = m_plugin_manager->get_active_plugin();
    if (plugin && plugin->is_rom_loaded()) {
        plugin->reset();
        m_audio_manager->clear_buffer();
    }
}

void Application::toggle_pause() {
    if (m_paused) {
        resume();
    } else {
        pause();
    }
}

void Application::frame_advance() {
    if (m_paused) {
        m_frame_advance_requested = true;
    }
}

void Application::update_netplay_cache() {
    // Update cached netplay state to avoid per-frame overhead
    // This is called when netplay connects/disconnects
    auto* netplay = m_plugin_manager ? m_plugin_manager->get_netplay_plugin() : nullptr;
    m_netplay_active_cached = netplay && netplay->is_connected();

    if (m_netplay_active_cached && netplay) {
        // Cache the INetplayCapable pointer
        auto* plugin = m_plugin_manager->get_active_plugin();
        m_netplay_capable_plugin = plugin ? dynamic_cast<INetplayCapable*>(plugin) : nullptr;

        // Pre-allocate input buffer for the player count
        int player_count = netplay->get_active_player_count();
        if (static_cast<int>(m_netplay_inputs_buffer.size()) != player_count) {
            m_netplay_inputs_buffer.resize(player_count, 0);
        }
    } else {
        m_netplay_capable_plugin = nullptr;
        // Don't deallocate the buffer - just leave it for potential future use
    }
}

// =============================================================================
// INetplayHost Implementation
// =============================================================================

void Application::pause_emulator() {
    pause();
}

void Application::resume_emulator() {
    resume();
}

bool Application::is_emulator_paused() const {
    return m_paused;
}

void Application::reset_emulator() {
    reset();
}

uint64_t Application::get_frame_count() const {
    if (!m_plugin_manager) return 0;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    return emulator ? emulator->get_frame_count() : 0;
}

double Application::get_fps() const {
    if (!m_plugin_manager) return 60.0;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    return emulator ? emulator->get_info().native_fps : 60.0;
}

bool Application::is_rom_loaded() const {
    return m_plugin_manager && m_plugin_manager->is_rom_loaded();
}

const char* Application::get_rom_name() const {
    if (!m_plugin_manager) {
        m_cached_rom_name = "";
        return m_cached_rom_name.c_str();
    }
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (emulator) {
        m_cached_rom_name = emulator->get_info().name;
    } else {
        m_cached_rom_name = "";
    }
    return m_cached_rom_name.c_str();
}

uint32_t Application::get_rom_crc32() const {
    if (!m_plugin_manager) return 0;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    return emulator ? emulator->get_rom_crc32() : 0;
}

const char* Application::get_platform_name() const {
    if (!m_plugin_manager) {
        m_cached_platform_name = "";
        return m_cached_platform_name.c_str();
    }
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (emulator) {
        m_cached_platform_name = emulator->get_info().name;
    } else {
        m_cached_platform_name = "";
    }
    return m_cached_platform_name.c_str();
}

bool Application::save_state_to_buffer(std::vector<uint8_t>& buffer) {
    if (!m_plugin_manager) return false;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator) return false;

    // Try fast path first if emulator supports it
    auto* netplay_capable = get_netplay_capable_emulator();
    if (netplay_capable) {
        size_t max_size = netplay_capable->get_max_state_size();
        buffer.resize(max_size);
        size_t actual_size = netplay_capable->save_state_fast(buffer.data(), buffer.size());
        if (actual_size > 0) {
            buffer.resize(actual_size);
            return true;
        }
    }

    // Fall back to standard save state
    return emulator->save_state(buffer);
}

bool Application::load_state_from_buffer(const std::vector<uint8_t>& buffer) {
    if (!m_plugin_manager) return false;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator) return false;

    // Try fast path first if emulator supports it
    auto* netplay_capable = get_netplay_capable_emulator();
    if (netplay_capable) {
        if (netplay_capable->load_state_fast(buffer.data(), buffer.size())) {
            return true;
        }
    }

    // Fall back to standard load state
    return emulator->load_state(buffer);
}

void Application::set_controller_input(int controller, uint32_t buttons) {
    // This would be used by the netplay plugin to set input for specific controllers
    // For now, we only support single-player input via the input manager
    (void)controller;
    (void)buttons;
}

uint32_t Application::get_local_input(int controller) const {
    if (m_input_manager && controller == 0) {
        return m_input_manager->get_button_state();
    }
    return 0;
}

const char* Application::get_config_directory() const {
    if (m_paths_config) {
        m_cached_config_dir = m_paths_config->get_config_directory().string();
    } else {
        m_cached_config_dir = (std::filesystem::current_path() / "config").string();
    }
    return m_cached_config_dir.c_str();
}

void Application::show_notification(NetplayNotificationType type, const char* message, float duration) {
    if (!m_gui_manager || !message) return;

    auto& notifications = m_gui_manager->get_notification_manager();
    std::string msg = message;

    switch (type) {
        case NetplayNotificationType::Info:
            notifications.info(msg, duration);
            break;
        case NetplayNotificationType::Success:
            notifications.success(msg, duration);
            break;
        case NetplayNotificationType::Warning:
            notifications.warning(msg, duration);
            break;
        case NetplayNotificationType::Error:
            notifications.error(msg, duration);
            break;
    }
}

void Application::on_netplay_connected(int player_id) {
    std::cout << "Application: Netplay connected as player " << player_id << std::endl;
    update_netplay_cache();
}

void Application::on_netplay_disconnected(const char* reason) {
    std::cout << "Application: Netplay disconnected - " << (reason ? reason : "unknown") << std::endl;
    update_netplay_cache();
}

void Application::on_netplay_player_joined(const NetplayPlayer& player) {
    std::cout << "Application: Player " << player.name << " joined" << std::endl;
    update_netplay_cache();
}

void Application::on_netplay_player_left(int player_id, const char* reason) {
    std::cout << "Application: Player " << player_id << " left - " << (reason ? reason : "unknown") << std::endl;
    update_netplay_cache();
}

void Application::on_netplay_desync(const DesyncInfo& info) {
    std::cerr << "Application: DESYNC at frame " << info.frame << std::endl;
    if (m_gui_manager) {
        m_gui_manager->get_notification_manager().error("Netplay desync detected!");
    }
}

void Application::on_netplay_chat_message(int player_id, const char* message) {
    std::cout << "Application: Chat from player " << player_id << ": " << (message ? message : "") << std::endl;
}

INetplayCapable* Application::get_netplay_capable_emulator() const {
    if (!m_plugin_manager) return nullptr;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator) return nullptr;
    return dynamic_cast<INetplayCapable*>(emulator);
}

bool Application::save_screenshot(const std::string& path) {
    auto* plugin = m_plugin_manager ? m_plugin_manager->get_active_plugin() : nullptr;
    if (!plugin || !plugin->is_rom_loaded()) {
        std::cerr << "[Screenshot] No ROM loaded\n";
        return false;
    }

    FrameBuffer fb = plugin->get_framebuffer();
    if (!fb.pixels || fb.width <= 0 || fb.height <= 0) {
        std::cerr << "[Screenshot] No framebuffer available\n";
        return false;
    }

    // Determine output path
    std::filesystem::path output_path;
    if (path.empty()) {
        // Generate timestamped filename in screenshots directory
        auto screenshot_dir = m_paths_config->get_screenshot_directory();
        output_path = screenshot_dir / Screenshot::generate_filename();
    } else {
        output_path = path;
    }

    return Screenshot::save_png(output_path, fb.pixels, fb.width, fb.height);
}

} // namespace emu
