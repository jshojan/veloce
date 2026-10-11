#include <charconv>
#include <stdexcept>
#include "application.hpp"
#include "window_manager.hpp"
#include "renderer.hpp"
#include "input_manager.hpp"
#include "audio_manager.hpp"
#include "audio_pipeline.hpp"
#include "tas_host.hpp"
#include <algorithm>
#include "plugin_manager.hpp"
#include "savestate_manager.hpp"
#include "paths_config.hpp"
#include "screenshot.hpp"
#include "test_file_sink.hpp"
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
    std::cout << "  VELOCE_TEST_OUT=path   Write the ROM's test-result channel to path\n";
    std::cout << "                         (VELOCE-RESULT/1; implies HEADLESS=1)\n";
    std::cout << "  VELOCE_TEST_EXIT=0|1   Stop at the result terminator (default 1)\n";
    std::cout << "  VELOCE_TEST_RESETS=N   Max ROM-requested resets (default 3)\n";
    std::cout << "\n";
    std::cout << "ROM_FILE:\n";
    std::cout << "  Optional path to a ROM file to load on startup.\n";
    std::cout << "  Supported formats: .nes (NES), .sfc/.smc (SNES), .gb/.gbc (GB), .gba (GBA), .z64/.v64/.n64 (experimental N64 homebrew)\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " game.nes                        # Load and run a NES ROM\n";
    std::cout << "  " << program_name << " --debug game.nes                # Load with debug mode\n";
    std::cout << "  " << program_name << "                                 # Start without loading a ROM\n";
    std::cout << "  HEADLESS=1 FRAMES=600 " << program_name << " test.sfc  # Run test ROM headless\n";
}

void Application::print_version() {
    std::cout << "Veloce v0.1.0\n";
    std::cout << "Plugin-based Nintendo emulation; hardware accuracy varies by core.\n";
    std::cout << "Available systems are discovered from core plugins.\n";
}

Application::CommandLineResult Application::parse_command_line(int argc, char* argv[], std::string& rom_path) {
    rom_path.clear();

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return CommandLineResult::Exit;
        }
        else if (std::strcmp(arg, "-v") == 0 || std::strcmp(arg, "--version") == 0) {
            print_version();
            return CommandLineResult::Exit;
        }
        else if (std::strcmp(arg, "-d") == 0 || std::strcmp(arg, "--debug") == 0) {
            m_debug_mode = true;
            std::cout << "Debug mode enabled\n";
        }
        else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            return CommandLineResult::Error;
        }
        else {
            // Assume it's a ROM path
            rom_path = arg;
        }
    }

    return CommandLineResult::Run;
}

bool Application::initialize(int argc, char* argv[]) {
    // Parse command line arguments
    std::string rom_path;
    const auto command_line = parse_command_line(argc, argv, rom_path);
    if (command_line != CommandLineResult::Run) {
        m_running = false;
        return command_line == CommandLineResult::Exit;
    }

    // Test-result channel: VELOCE_TEST_OUT=<path> (see docs/testing/VELOCE-RESULT.md)
    TestRunConfig test_config;
    const bool test_session = TestRunConfig::from_env(test_config);

    // Check for HEADLESS environment variable. VELOCE_TEST_OUT implies
    // HEADLESS=1 unless HEADLESS=0 is given explicitly (test ROM under a window).
    const char* headless_env = std::getenv("HEADLESS");
    if (headless_env && headless_env[0] != '0') {
        m_headless_mode = true;
    } else if (test_session && !headless_env) {
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

    if (m_input_manager) {
        m_plugin_manager->set_input_host(this);
        const auto selected = m_plugin_manager->get_selected_plugin_name(PluginType::Input);
        if (!m_plugin_manager->set_active_plugin(PluginType::Input, selected)) {
            std::cerr << "Input plugin unavailable; using mapped device input\n";
        }
    }

    if (m_audio_manager) {
        m_audio_pipeline = std::make_unique<AudioPipeline>(
            [this](const float* samples, size_t count, int rate) {
                m_audio_manager->push_samples_resampled(samples, count, rate);
            },
            [this] { m_audio_manager->pause(); },
            [this] { m_audio_manager->resume(); });
        m_plugin_manager->set_audio_host(m_audio_pipeline.get());
        const auto selected = m_plugin_manager->get_selected_plugin_name(PluginType::Audio);
        if (!m_plugin_manager->set_active_plugin(PluginType::Audio, selected)) {
            std::cerr << "Audio plugin unavailable; using device passthrough\n";
        }
    }

    if (!m_headless_mode) {
        m_tas_host = std::make_unique<TASHost>(*this);
        m_plugin_manager->set_tas_host(m_tas_host.get());
        const auto selected = m_plugin_manager->get_selected_plugin_name(PluginType::TAS);
        m_plugin_manager->set_active_plugin(PluginType::TAS, selected);
    }

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

    // Re-resolve the cached INetplayCapable pointer whenever the active
    // emulator instance changes (e.g. loading a new ROM destroys the old
    // instance and creates a new one). Without this, m_netplay_capable_plugin
    // keeps pointing at the destroyed instance while netplay stays connected.
    m_plugin_manager->on_plugin_changed([this](PluginType type, const std::string&) {
        if (type == PluginType::Input || type == PluginType::Emulator) m_input_sample_valid = false;
        if (type == PluginType::Emulator) {
            update_netplay_cache();
        }
    });

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

    // Attach the test-result sink once the ROM is loaded.
    if (test_session) {
        auto* plugin = m_plugin_manager->get_active_plugin();
        if (!plugin || !plugin->is_rom_loaded()) {
            std::cerr << "VELOCE_TEST_OUT set but no ROM loaded; test session disabled\n";
        } else {
            m_test_sink = std::make_unique<TestFileSink>(test_config);
            if (!m_test_sink->open()) {
                std::cerr << "Failed to open VELOCE_TEST_OUT file: " << test_config.out_path << std::endl;
                m_test_sink.reset();
                return false;
            }
            m_test_sink->attach(plugin, m_plugin_manager->get_active_emulator_api_version());
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
    // Informational CLI exits do not create managers or initialize SDL.
    if (!m_running) return;
    // Headless mode - run without GUI for automated testing
    if (m_headless_mode) {
        auto* active_plugin = m_plugin_manager->get_active_plugin();
        if (!active_plugin || !active_plugin->is_rom_loaded()) {
            std::cerr << "No ROM loaded for headless mode\n";
            return;
        }

        // Parse INPUT or the bounded INPUT_FILE for button injection
        // Format: "frame:buttons,frame:buttons,..." where buttons is a hex mask
        // Example: "200:40,201:0,400:40,401:0" (press Start at frame 200 and 400)
        // Button bits: A=1, B=2, X=4, Y=8, L=16, R=32, Start=64, Select=128, Up=256, Down=512, Left=1024, Right=2048
        std::unordered_map<int, uint32_t> input_schedule;
        const char* input_env = std::getenv("INPUT");
        std::string input_text = input_env ? input_env : "";
        if (const char* input_file = std::getenv("INPUT_FILE")) {
            if (input_env) throw std::runtime_error("Use either INPUT or INPUT_FILE");
            std::ifstream stream(input_file, std::ios::binary);
            if (!stream || std::filesystem::file_size(input_file) > 8 * 1024 * 1024) {
                throw std::runtime_error("Cannot read input schedule (maximum 8 MiB)");
            }
            input_text.resize(8 * 1024 * 1024 + 1);
            stream.read(input_text.data(), static_cast<std::streamsize>(input_text.size()));
            input_text.resize(static_cast<size_t>(stream.gcount()));
            if (stream.bad() || input_text.size() > 8 * 1024 * 1024)
                throw std::runtime_error("Cannot read input schedule (maximum 8 MiB)");
        }
        if (!input_text.empty()) {
            std::string input_str(input_text);
            size_t pos = 0;
            while (pos < input_str.length()) {
                size_t colon = input_str.find(':', pos);
                if (colon == std::string::npos) throw std::runtime_error("Invalid input schedule: missing colon");
                size_t comma = input_str.find(',', colon);
                if (comma == std::string::npos) comma = input_str.length();

                int frame = 0;
                uint32_t buttons = 0;
                const auto* begin = input_str.data();
                const auto parsed_frame = std::from_chars(begin + pos, begin + colon, frame);
                size_t mask_start = colon + 1;
                if (comma - mask_start >= 2 && input_str[mask_start] == '0' &&
                    (input_str[mask_start + 1] == 'x' || input_str[mask_start + 1] == 'X')) mask_start += 2;
                const auto parsed_buttons = std::from_chars(begin + mask_start, begin + comma, buttons, 16);
                if (parsed_frame.ec != std::errc{} || parsed_frame.ptr != begin + colon || frame < 0 ||
                    parsed_buttons.ec != std::errc{} || parsed_buttons.ptr != begin + comma)
                    throw std::runtime_error("Invalid input schedule: expected nonnegative frame:32-bit hex mask");
                input_schedule[frame] = buttons;

                pos = comma + 1;
            }
            std::cerr << "Headless mode: Loaded " << input_schedule.size() << " input events\n";
        }

        emu::InputState current_input{};
        int frames_run = 0;

        bool test_stop = false;
        while (m_running && !m_quit_requested && !test_stop && frames_run < m_headless_frames) {
            // Check for scheduled input
            auto it = input_schedule.find(frames_run);
            if (it != input_schedule.end()) {
                current_input.buttons = it->second;
            }

            active_plugin->run_frame(current_input);
            frames_run++;

            // Check for screenshot at specific frame
            if (m_screenshot_at_frame > 0 && frames_run == m_screenshot_at_frame) {
                std::string path = m_screenshot_output_path.empty()
                    ? ("screenshot_frame_" + std::to_string(frames_run) + ".png")
                    : m_screenshot_output_path;
                save_screenshot(path);
            }

            // Test-result channel: pending reset / terminator / reset limit
            test_stop = test_session_after_frame();
        }
        end_test_session(frames_run >= m_headless_frames);

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

    // Set audio sync mode - DynamicRate with bsnes-style rate control
    // Adjusts playback rate to keep audio buffer ~50% full, preventing
    // underruns and overruns from timing drift between emulator and audio hardware.
    m_audio_manager->set_sync_mode(AudioSyncMode::DynamicRate);

    while (m_running && !m_quit_requested) {
        uint64_t frame_start = WindowManager::get_ticks();
        double frequency = static_cast<double>(WindowManager::get_performance_frequency());

        // Process events
        process_events();

        // Update input
        m_input_manager->update();
        if (auto* netplay = m_plugin_manager->get_netplay_plugin()) netplay->poll_network();

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

bool Application::test_session_after_frame() {
    if (!m_test_sink) return false;
    ++m_test_frames;
    return m_test_sink->after_frame(m_test_frames);
}

void Application::end_test_session(bool budget_exhausted) {
    if (!m_test_sink) return;
    auto* plugin = m_plugin_manager ? m_plugin_manager->get_active_plugin() : nullptr;
    uint64_t cycles = (plugin && plugin->is_rom_loaded()) ? plugin->get_cycle_count() : 0;
    m_test_sink->close(budget_exhausted ? TestFileSink::EndReason::Frames
                                        : TestFileSink::EndReason::Quit,
                       m_test_frames, cycles);
    m_test_sink.reset();
}

void Application::shutdown() {
    // Close the test-result file while the emulator plugin is still alive
    end_test_session(false);

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
    m_tas_host.reset();
    m_audio_pipeline.reset();
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

                        case SDLK_n:
                            if (event.key.keysym.mod & KMOD_CTRL) {
                                auto* netplay = m_plugin_manager->get_netplay_plugin();
                                if (netplay) {
                                    netplay->show_panel(!netplay->is_panel_visible());
                                }
                            }
                            break;

                        case SDLK_PRINTSCREEN:
                            if (save_screenshot()) {
                                m_gui_manager->get_notification_manager().success("Screenshot saved");
                            } else {
                                m_gui_manager->get_notification_manager().error("Failed to save screenshot");
                            }
                            break;

                        // Savestate hotkeys (matches README / GUI menu labels):
                        // F1-F10 to save, Shift+F1-F10 to load.
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
                                // Load state (Shift+F1-F10)
                                if (m_savestate_manager->load_state(slot)) {
                                    reset_input_timeline();
                                    msg << "State loaded from slot " << (slot + 1);
                                    notifications.success(msg.str());
                                    std::cout << msg.str() << std::endl;
                                    if (m_audio_manager) {
                                        m_audio_manager->clear_buffer();
                                    }
                                    m_plugin_manager->notify_game_plugins_state_loaded();
                                } else {
                                    msg << "Failed to load state from slot " << (slot + 1);
                                    notifications.error(msg.str());
                                    std::cout << msg.str() << std::endl;
                                }
                            } else {
                                // Save state (F1-F10)
                                if (m_savestate_manager->save_state(slot)) {
                                    msg << "State saved to slot " << (slot + 1);
                                    notifications.success(msg.str());
                                    std::cout << msg.str() << std::endl;
                                } else {
                                    msg << "Failed to save state to slot " << (slot + 1);
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

bool Application::run_emulation_frame() {
    auto* plugin = m_plugin_manager->get_active_plugin();
    if (!plugin || !plugin->is_rom_loaded()) {
        return false;
    }
    // A manually resumed app must still freeze the core during handshake.
    auto* session = m_plugin_manager->get_netplay_plugin();
    if (session && session->get_connection_state() != NetplayConnectionState::Disconnected) {
        if (auto* movie = m_plugin_manager->get_tas_plugin()) { movie->stop_recording(); movie->stop_playback(); }
        m_injected_inputs.fill(std::nullopt);
    }
    if (session && session->owns_frame_execution() &&
        session->get_connection_state() == NetplayConnectionState::Connecting) return false;

    if (auto* movie = m_plugin_manager->get_tas_plugin(); movie && movie->get_mode() != TASMode::Stopped && m_input_manager->is_input_capture_mode()) return false;
    sample_input();
    m_audio_pipeline->begin_frame();

    // Cached connection status is updated by session callbacks.
    bool netplay_active = m_netplay_active_cached;

    if (netplay_active) {
        if (auto* tas = m_plugin_manager->get_tas_plugin()) { tas->stop_recording(); tas->stop_playback(); }
        m_injected_inputs.fill(std::nullopt);
        auto* netplay = m_plugin_manager->get_netplay_plugin();
        if (netplay) {
            if (netplay->owns_frame_execution()) {
                if (!netplay->execute_frame(get_local_input(0))) return false;
            } else {
                // Netplay mode: use synchronized input
                if (!netplay->begin_frame()) {
                    // Waiting for network sync, skip this frame
                    return false;
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
        }
    } else {
        auto* tas = m_plugin_manager->get_tas_plugin();
        if (tas && tas->get_mode() != TASMode::Stopped && can_run_tas()) {
            const auto cursor = tas->get_current_frame();
            TASFrameData frame = tas->get_mode() == TASMode::Recording ? TASFrameData{} : tas->get_frame(cursor);
            for (int c = 0; c < 4; ++c) frame.controller_inputs[c] = tas->on_frame(c);
            if (!execute_tas_frame(frame, false)) return false;
            const bool recording = tas->get_mode() == TASMode::Recording;
            tas->on_frame_completed();
            if (!recording && tas->get_mode() == TASMode::Stopped) pause();
        } else {
            TASFrameData frame{};
            for (int c=0;c<4;++c) frame.controller_inputs[c] = get_local_input(c);
            auto* local_ports=dynamic_cast<INetplayCapable*>(plugin);
            if (local_ports && local_ports->get_max_players()>1 &&
                (frame.controller_inputs[1] || frame.controller_inputs[2] || frame.controller_inputs[3])) {
                const int count=std::clamp(local_ports->get_max_players(),1,4);
                local_ports->run_frame_netplay_n(std::vector<uint32_t>(frame.controller_inputs,frame.controller_inputs+count));
            } else plugin->run_frame({frame.controller_inputs[0]});
        }
        m_injected_inputs.fill(std::nullopt);
    }

    // Test-result channel (GUI path): stop cleanly once the ROM is done
    if (m_test_sink && test_session_after_frame()) {
        request_quit();
    }

    // Update game plugins (for timer updates and auto-split detection)
    m_plugin_manager->update_game_plugins();

    // Cores which do not actually stream still receive a frame-end audio path.
    // Merely storing the callback in the base ABI does not prove delivery.
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    const bool skip_audio = netplay_active && netplay && netplay->is_rolling_back();
    AudioBuffer audio = plugin->get_audio();
    if (audio.samples && audio.sample_count > 0) {
        m_audio_pipeline->batch(m_plugin_manager->get_audio_plugin(), audio.samples,
                               static_cast<size_t>(audio.sample_count) * 2,
                               audio.sample_rate, skip_audio);
    }
    plugin->clear_audio_buffer();

    // Update texture with framebuffer
    FrameBuffer fb = plugin->get_framebuffer();
    if (fb.pixels) {
        m_renderer->update_texture(fb.pixels, fb.width, fb.height);
    }
    return true;
}

void Application::render() {
    m_renderer->clear();
    m_gui_manager->begin_frame();
    m_gui_manager->render(*this, *m_renderer);
    m_gui_manager->end_frame();
    m_window_manager->swap_buffers();
}

bool Application::load_rom(const std::string& path, bool preserve_selected_core) {
    std::cout << "Loading ROM: " << path << std::endl;

    // A test session belongs to the ROM it was opened for. Loading another ROM
    // (GUI browser, drag-and-drop) may replace the emulator plugin the sink is
    // attached to, so write the trailer and detach while that plugin is alive.
    if (m_test_sink) {
        end_test_session(false);
    }

    if (auto* tas = m_plugin_manager->get_tas_plugin()) tas->close_movie();
    m_injected_inputs.fill(std::nullopt);

    // Find appropriate plugin for this file type
    const bool activated = preserve_selected_core
        ? m_plugin_manager->set_active_plugin(m_plugin_manager->get_selected_plugin_name(PluginType::Emulator))
        : m_plugin_manager->set_active_plugin_for_file(path);
    if (!activated) {
        std::cerr << "No plugin found for file: " << path << std::endl;
        return false;
    }

    // Load ROM using the path-based method
    // This handles file reading, battery saves, and game plugin initialization
    if (!m_plugin_manager->load_rom(path)) {
        std::cerr << "Failed to load ROM" << std::endl;
        return false;
    }

    // Let the savestate manager tag new saves with the actual ROM name
    // (previously never set, so savestate headers' rom_name field was
    // always empty).
    if (m_savestate_manager) {
        m_savestate_manager->set_current_rom_name(m_plugin_manager->get_current_rom_name());
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
            if (auto* input = m_plugin_manager->get_input_plugin()) input->set_controller_layout(layout);
        }

        // Update window title
        m_window_manager->set_title("Veloce - " + path);
    }

    // Clear any stale audio data from previous ROM
    if (m_audio_manager) {
        m_audio_manager->clear_buffer();
    }

    // Set up streaming audio callback for lowest latency
    // The callback pushes small batches of samples (~16 = 0.5ms) during emulation
    // rather than waiting until frame end (~16.67ms)
    if (!m_headless_mode) {
        auto* active_plugin = m_plugin_manager->get_active_plugin();
        if (active_plugin) {
            active_plugin->set_audio_callback(
                [this](const float* samples, size_t count, int rate) {
                    auto* netplay = m_plugin_manager->get_netplay_plugin();
                    const bool suppressed = m_tas_replaying || (netplay && netplay->is_rolling_back());
                    m_audio_pipeline->stream(m_plugin_manager->get_audio_plugin(),
                                             samples, count * 2, rate, suppressed);
                });
        }
    }

    reset_input_timeline();
    if (m_tas_host) m_tas_host->capture_reset_state();

    // Unpause emulation - audio will start automatically once buffer is ready
    // This prevents initial audio crackling from buffer underruns
    m_paused = false;
    // Note: audio_manager->resume() is called in run() once buffer is ready

    std::cout << "ROM loaded successfully" << std::endl;
    return true;
}

void Application::pause() {
    m_paused = true;
    if (m_audio_manager) m_audio_manager->pause();
    m_plugin_manager->set_paused(true);
}

void Application::resume() {
    m_paused = false;
    if (m_audio_manager) m_audio_manager->resume();
    m_plugin_manager->set_paused(false);
}

void Application::reset() {
    reset_input_timeline();
    if (auto* netplay = m_plugin_manager->get_netplay_plugin()) netplay->disconnect();
    auto* plugin = m_plugin_manager->get_active_plugin();
    if (plugin && plugin->is_rom_loaded()) {
        plugin->reset();
        // m_audio_manager is only constructed in GUI mode (see initialize());
        // headless test/tooling callers of reset() must not crash on it.
        if (m_audio_manager) {
            m_audio_manager->clear_buffer();
        }
        m_plugin_manager->notify_game_plugins_reset();
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
    // The actual ROM/game name (filename stem), not the emulator core name --
    // see get_platform_name() for the latter. Netplay lobby/game-selection
    // messages use this to identify what a peer is playing.
    m_cached_rom_name = m_plugin_manager->get_current_rom_name();
    return m_cached_rom_name.c_str();
}

uint32_t Application::get_rom_crc32() const {
    if (!m_plugin_manager) return 0;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    return emulator ? emulator->get_rom_crc32() : 0;
}

uint64_t Application::get_full_rom_hash() const {
    return m_plugin_manager ? m_plugin_manager->get_full_rom_hash() : 0;
}
uint64_t Application::get_core_build_hash() const {
    return m_plugin_manager ? m_plugin_manager->get_core_build_hash() : 0;
}

bool Application::prepare_netplay() {
    if (!m_plugin_manager) return false;
    const auto path = m_plugin_manager->get_current_rom_path();
    if (path.empty()) return false;
    const bool loaded = load_rom(path, true);
    pause();
    return loaded;
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
        const size_t required = netplay_capable->save_state_fast(nullptr, 0);
        if (required && required <= netplay_capable->get_max_state_size()) {
            buffer.resize(required);
            const size_t actual_size = netplay_capable->save_state_fast(buffer.data(), buffer.size());
            if (actual_size > 0 && actual_size <= required) {
                buffer.resize(actual_size);
                return true;
            }
        }
    }

    // Fall back to standard save state
    return emulator->save_state(buffer);
}

bool Application::load_state_from_buffer(const std::vector<uint8_t>& buffer) {
    if (!m_plugin_manager) return false;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator) return false;
    if (auto* netplay = m_plugin_manager->get_netplay_plugin(); netplay && !netplay->is_rolling_back())
        netplay->disconnect();

    // Try fast path first if emulator supports it
    auto* netplay_capable = get_netplay_capable_emulator();
    if (netplay_capable) {
        if (netplay_capable->load_state_fast(buffer.data(), buffer.size())) {
            if (auto* session = m_plugin_manager->get_netplay_plugin(); !session || !session->is_rolling_back()) reset_input_timeline();
            m_plugin_manager->notify_game_plugins_state_loaded();
            return true;
        }
    }

    // Fall back to standard load state
    if (emulator->load_state(buffer)) {
        if (auto* session = m_plugin_manager->get_netplay_plugin(); !session || !session->is_rolling_back()) reset_input_timeline();
        m_plugin_manager->notify_game_plugins_state_loaded();
        return true;
    }
    return false;
}

void Application::set_controller_input(int controller, uint32_t buttons) {
    if (controller >= 0 && controller < 4 && can_run_tas()) m_injected_inputs[controller] = buttons;
}

bool Application::can_run_tas() const {
    if (!m_plugin_manager || !m_audio_pipeline || !is_rom_loaded() || !m_tas_host || !m_tas_host->has_reset_state()) return false;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return !netplay || netplay->get_connection_state() == NetplayConnectionState::Disconnected;
}

bool Application::step_frame() {
    if (!m_paused || !can_run_tas()) return false;
    return run_emulation_frame();
}

bool Application::execute_tas_frame(const TASFrameData& frame, bool replay) {
    if (!can_run_tas()) return false;
    auto* core = m_plugin_manager->get_active_plugin();
    auto* ports = dynamic_cast<INetplayCapable*>(core);
    const int count = ports ? std::clamp(ports->get_max_players(),1,4) : 1;
    auto reject = [&] {
        if (!replay) {
            pause();
            if (auto* movie=m_plugin_manager->get_tas_plugin()) { movie->stop_recording(); movie->stop_playback(); }
            m_gui_manager->get_notification_manager().error("Movie input or state event is unsupported on this core");
        }
        return false;
    };
    for (int i=count;i<4;++i) if (frame.controller_inputs[i]) return reject();
    if (frame.has_savestate && (frame.savestate_slot<0 || frame.savestate_slot>9)) return reject();
    if (frame.has_reset) { core->reset(); reset_input_timeline(false); m_audio_manager->clear_buffer(); m_plugin_manager->notify_game_plugins_reset(); }
    if (frame.has_savestate && !m_savestate_manager->save_state(frame.savestate_slot)) return reject();
    const auto before = core->get_frame_count();
    const bool was_replaying = m_tas_replaying;
    m_tas_replaying = replay;
    try {
        if (ports) ports->run_frame_netplay_n(std::vector<uint32_t>(frame.controller_inputs,frame.controller_inputs+count));
        else core->run_frame({frame.controller_inputs[0]});
    } catch (...) { m_tas_replaying = was_replaying; throw; }
    m_tas_replaying = was_replaying;
    if (replay) core->clear_audio_buffer();
    return core->get_frame_count() != before;
}

bool Application::run_replay_frame(const TASFrameData& frame) {
    if (!can_run_tas()) return false;
    // A register capture cannot contain unmarked seek/replay observations.
    if (m_test_sink) end_test_session(false);
    if (!execute_tas_frame(frame,true)) return false;
    m_input_sample_valid = false;
    auto fb = m_plugin_manager->get_active_plugin()->get_framebuffer();
    if (fb.pixels) m_renderer->update_texture(fb.pixels,fb.width,fb.height);
    return true;
}

void Application::reset_input_timeline(bool stop_movie) {
    m_injected_inputs.fill(std::nullopt);
    if (stop_movie && m_plugin_manager) {
        if (auto* movie = m_plugin_manager->get_tas_plugin()) { movie->stop_recording(); movie->stop_playback(); }
    }
    m_input_sample_valid = false;
    if (auto* input = m_plugin_manager ? m_plugin_manager->get_input_plugin() : nullptr) {
        input->stop_recording();
        input->stop_playback();
    }
}

void Application::sample_input() {
    const auto frame = get_frame_number();
    if (m_input_sample_valid && m_input_sample_frame == frame) return;
    m_input_sample_frame = frame;
    m_input_sample_valid = true;
    m_input_samples.fill(0);
    auto* input = m_plugin_manager->get_input_plugin();
    if (input) input->begin_frame();
    for (int controller = 0; controller < get_controller_count() && controller < 4; ++controller) {
        m_input_samples[controller] = input ? input->get_input_state(controller) : encode_controller_input(controller,get_mapped_input(controller));
    }
}

uint32_t Application::get_local_input(int controller) const {
    if (!m_input_manager || m_input_manager->is_input_capture_mode() || controller < 0 || controller >= 4) return 0;
    if (m_injected_inputs[controller]) return *m_injected_inputs[controller];
    return m_input_sample_valid ? m_input_samples[controller] : encode_controller_input(controller,get_mapped_input(controller));
}
uint64_t Application::get_frame_number() const { return get_frame_count(); }
int Application::get_controller_count() const {
    auto* core = m_plugin_manager ? m_plugin_manager->get_active_plugin() : nullptr;
    auto* layout = core ? core->get_controller_layout() : nullptr;
    return layout ? std::clamp(layout->controller_count, 1, 4) : 1;
}
bool Application::is_key_pressed(int code) const { return m_input_manager && !m_input_manager->is_input_capture_mode() && m_input_manager->is_key_pressed(code); }
bool Application::is_gamepad_button_pressed(int controller, int button) const { return m_input_manager && !m_input_manager->is_input_capture_mode() && m_input_manager->is_gamepad_button_pressed(controller, button); }
float Application::get_gamepad_axis(int controller, int axis) const { return m_input_manager && !m_input_manager->is_input_capture_mode() ? m_input_manager->get_gamepad_axis(controller, axis) : 0.0f; }
int Application::get_connected_controller_count() const { return m_input_manager ? m_input_manager->get_connected_controller_count() : 0; }
ControllerInfo Application::get_controller_info(int index) const {
    if (index < 0 || index >= get_connected_controller_count()) return {-1,"",false,0,0,0};
    m_controller_names.resize(get_connected_controller_count());
    m_controller_names[index] = m_input_manager->get_controller_name(index);
    return {index,m_controller_names[index].c_str(),true,SDL_CONTROLLER_BUTTON_MAX,SDL_CONTROLLER_AXIS_MAX,0};
}
bool Application::get_mapped_binding(int controller, VirtualButton button, InputBinding& binding) const {
    if (!m_input_manager || controller<0 || controller>=4) return false;
    const auto* mapped = m_input_manager->get_binding(button,controller);
    if (!mapped) return false;
    binding = *mapped;
    if(controller>0 && binding.type!=InputSourceType::Keyboard) {
        binding.device_id=m_input_manager->get_controller_for_port(controller);
        if(binding.device_id<0) binding.device_id=-2;
        return true;
    }
    // GUI profiles persist SDL instance identities; plugin raw polling uses
    // connected-device indices. Translate only this API view of the binding.
    if (binding.type != InputSourceType::Keyboard && binding.device_id >= 0) {
        const int instance = binding.device_id;
        binding.device_id = -2; // disconnected, never a wildcard
        for (int i = 0; i < get_connected_controller_count(); ++i) {
            if (m_input_manager->get_controller_instance_id(i) == instance) { binding.device_id = i; break; }
        }
    }
    return true;
}
uint32_t Application::get_mapped_input(int controller) const { return m_input_manager ? m_input_manager->get_button_state(controller) : 0; }
uint32_t Application::encode_controller_input(int controller, uint32_t buttons) const {
    return m_input_manager ? m_input_manager->encode_controller_input(controller,buttons) : buttons;
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
