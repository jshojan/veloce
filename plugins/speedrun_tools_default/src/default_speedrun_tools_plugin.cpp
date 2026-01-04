#include "emu/game_plugin.hpp"
#include "emu/plugin_types.hpp"
#include "game_plugin_common/timer_core.hpp"
#include "game_plugin_common/splits_file.hpp"
#include "game_plugin_common/panels/timer_panel.hpp"

#include <imgui.h>
#include <iostream>
#include <filesystem>

namespace {

// Default Game Plugin - Built-in Timer
// This is a universal game plugin that provides timer/splits/PB functionality
// for any game. It uses the game_plugin_common library for core functionality.
class DefaultGamePlugin : public emu::IGamePlugin {
public:
    DefaultGamePlugin() {
        // Set up timer callbacks
        m_timer.set_on_timer_started([this]() {
            if (m_host) m_host->on_timer_started();
        });
        m_timer.set_on_timer_stopped([this]() {
            if (m_host) m_host->on_timer_stopped();
        });
        m_timer.set_on_run_reset([this]() {
            if (m_host) m_host->on_run_reset();
        });
        m_timer.set_on_split_triggered([this](int index) {
            if (m_host) m_host->on_split_triggered(index);
        });
        m_timer.set_on_run_completed([this](uint64_t final_time) {
            if (m_host) m_host->on_run_completed(final_time);
            // Auto-save if enabled
            if (m_autosave_enabled && m_splits_file.has_path()) {
                m_splits_file.save(m_timer.data());
            }
        });
    }

    ~DefaultGamePlugin() override = default;

    // ============================================================
    // IGamePlugin interface implementation
    // ============================================================

    emu::GamePluginInfo get_info() override {
        return {
            "Built-in Timer",           // name
            "1.0.0",                     // version
            "Veloce Team",              // author
            "Built-in speedrun timer with splits tracking, PB management, "
            "and comparison support. Features sum of best calculation and "
            "segment time tracking.",   // description
            nullptr,                     // game_name (universal)
            nullptr,                     // platform (universal)
            0,                           // game_crc32 (universal - matches any)
            nullptr,                     // alt_crc32s
            0,                           // alt_crc32_count
            nullptr,                     // categories (defined per-game)
            emu::GamePluginCapabilities::Timer |
            emu::GamePluginCapabilities::Autosave |
            emu::GamePluginCapabilities::Comparisons
        };
    }

    bool initialize(emu::IGameHost* host) override {
        m_host = host;
        return true;
    }

    void shutdown() override {
        // Auto-save if enabled and there are unsaved changes
        if (m_autosave_enabled && m_timer.data().unsaved_changes && m_splits_file.has_path()) {
            m_splits_file.save(m_timer.data());
        }
        m_host = nullptr;
    }

    // ============================================================
    // ROM Matching - Universal plugin matches any ROM
    // ============================================================

    bool matches_rom(uint32_t crc32, const char* rom_name) override {
        return true;  // Universal timer plugin - matches any ROM
    }

    // ============================================================
    // Timer control (delegate to TimerCore)
    // ============================================================

    void start_timer() override { m_timer.start(); }
    void stop_timer() override { m_timer.stop(); }
    void reset_timer() override { m_timer.reset(); }
    void pause_timer() override { m_timer.pause(); }
    void resume_timer() override { m_timer.resume(); }

    // ============================================================
    // Split control (delegate to TimerCore)
    // ============================================================

    void split() override { m_timer.split(); }
    void undo_split() override { m_timer.undo_split(); }
    void skip_split() override { m_timer.skip_split(); }

    // ============================================================
    // Timer state (delegate to TimerCore)
    // ============================================================

    emu::TimerState get_timer_state() const override { return m_timer.get_state(); }
    uint64_t get_current_time_ms() const override { return m_timer.get_current_time_ms(); }
    int get_current_split_index() const override { return m_timer.get_current_split_index(); }
    int get_total_splits() const override { return m_timer.get_total_splits(); }

    // ============================================================
    // Split times (delegate to TimerCore)
    // ============================================================

    emu::SplitTiming get_split_timing(int index) const override {
        return m_timer.get_split_timing(index);
    }

    uint64_t get_best_possible_time_ms() const override {
        return m_timer.get_best_possible_time_ms();
    }

    uint64_t get_sum_of_best_ms() const override {
        return m_timer.get_sum_of_best_ms();
    }

    // ============================================================
    // Comparison management (delegate to TimerCore)
    // ============================================================

    emu::ComparisonType get_comparison_type() const override {
        return m_timer.get_comparison_type();
    }

    void set_comparison_type(emu::ComparisonType type) override {
        m_timer.set_comparison_type(type);
    }

    int get_comparison_count() const override {
        return 2;  // PB and Best Segments
    }

    const char* get_comparison_name(int index) const override {
        switch (index) {
            case 0: return "Personal Best";
            case 1: return "Best Segments";
            default: return nullptr;
        }
    }

    // ============================================================
    // Run history (delegate to TimerCore)
    // ============================================================

    int get_attempt_count() const override { return m_timer.get_attempt_count(); }
    int get_completed_count() const override { return m_timer.get_completed_count(); }

    // ============================================================
    // Splits file management (delegate to SplitsFile)
    // ============================================================

    bool load_splits(const char* path) override {
        return m_splits_file.load(path, m_timer.data());
    }

    bool save_splits(const char* path) override {
        return m_splits_file.save(path, m_timer.data());
    }

    bool save_splits() override {
        return m_splits_file.save(m_timer.data());
    }

    const char* get_splits_path() const override {
        return m_splits_file.get_path().c_str();
    }

    bool has_unsaved_changes() const override {
        return m_timer.data().unsaved_changes;
    }

    // ============================================================
    // Split definitions (universal plugin returns empty)
    // ============================================================

    std::vector<emu::SplitDefinition> get_splits(const char* category) override {
        return {};  // Universal timer doesn't define auto-split conditions
    }

    const char* get_split_name(int index) const override {
        return m_timer.get_split_name(index);
    }

    // ============================================================
    // Frame callback
    // ============================================================

    void on_frame() override {
        // Timer updates are handled by get_current_time_ms() using wall clock
    }

    void on_split_triggered() override {
        split();
    }

    // ============================================================
    // Lifecycle callbacks
    // ============================================================

    void on_rom_loaded() override {
        setup_splits_from_host();
    }

    void on_rom_unloaded() override {
        if (m_autosave_enabled && m_timer.data().unsaved_changes && m_splits_file.has_path()) {
            m_splits_file.save(m_timer.data());
        }
    }

    void on_reset() override {
        reset_timer();
    }

    void on_run_complete(uint64_t final_time_ms) override {
        // Already handled in split() callback
    }

    // ============================================================
    // Autosave configuration
    // ============================================================

    bool get_autosave_enabled() const override { return m_autosave_enabled; }
    void set_autosave_enabled(bool enabled) override { m_autosave_enabled = enabled; }

    // ============================================================
    // Display configuration (delegate to TimerPanel)
    // ============================================================

    bool get_show_timer() const override { return m_panel.show_timer; }
    void set_show_timer(bool show) override { m_panel.show_timer = show; }

    bool get_show_splits() const override { return m_panel.show_splits; }
    void set_show_splits(bool show) override { m_panel.show_splits = show; }

    bool get_show_delta() const override { return m_panel.show_delta; }
    void set_show_delta(bool show) override { m_panel.show_delta = show; }

    // ============================================================
    // GUI Rendering (delegate to TimerPanel)
    // ============================================================

    void set_imgui_context(void* context) override {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
    }

    void render_gui(bool& visible) override {
        m_panel.render(visible, m_timer);
    }

    const char* get_panel_name() const override {
        return m_panel.get_name();
    }

private:
    void setup_splits_from_host() {
        if (!m_host) return;

        // Get ROM info from host
        const char* rom_name = m_host->get_rom_name();
        if (rom_name) {
            m_timer.set_game_name(rom_name);
        }

        // Generate a default splits path
        std::string splits_path = game_plugin_common::SplitsFile::generate_default_path(
            m_timer.get_game_name(), m_timer.get_category());

        // Try to load existing splits
        if (std::filesystem::exists(splits_path)) {
            m_splits_file.load(splits_path, m_timer.data());
        } else {
            m_timer.data().splits_path = splits_path;
        }
    }

    // Host reference
    emu::IGameHost* m_host = nullptr;

    // Core components from game_plugin_common
    game_plugin_common::TimerCore m_timer;
    game_plugin_common::SplitsFile m_splits_file;
    game_plugin_common::TimerPanel m_panel;

    // Configuration
    bool m_autosave_enabled = true;
};

} // anonymous namespace

// ============================================================
// C interface implementation
// ============================================================

extern "C" {

EMU_PLUGIN_EXPORT emu::PluginType get_plugin_type() {
    return emu::PluginType::Game;
}

EMU_PLUGIN_EXPORT emu::BasePluginInfo get_plugin_info() {
    return {
        "Built-in Timer",
        "1.0.0",
        "Veloce Team",
        "Built-in speedrun timer with splits tracking, PB management, "
        "and comparison support.",
        emu::GamePluginCapabilities::Timer |
        emu::GamePluginCapabilities::Autosave |
        emu::GamePluginCapabilities::Comparisons
    };
}

EMU_PLUGIN_EXPORT emu::IGamePlugin* create_game_plugin() {
    return new DefaultGamePlugin();
}

EMU_PLUGIN_EXPORT void destroy_game_plugin(emu::IGamePlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_game_plugin_api_version() {
    return EMU_GAME_PLUGIN_API_VERSION;
}

} // extern "C"
