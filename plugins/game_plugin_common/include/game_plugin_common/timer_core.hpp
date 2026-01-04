#pragma once

#include "timer_types.hpp"
#include "emu/game_plugin.hpp"
#include <string>
#include <functional>

namespace game_plugin_common {

// Callback type for timer events
using TimerEventCallback = std::function<void()>;
using SplitEventCallback = std::function<void(int split_index)>;
using RunCompleteCallback = std::function<void(uint64_t final_time_ms)>;

// TimerCore - Handles all timer logic independent of GUI
// This class can be reused by any game plugin that needs timer functionality
class TimerCore {
public:
    TimerCore();
    ~TimerCore() = default;

    // Timer control
    void start();
    void stop();
    void reset();
    void pause();
    void resume();

    // Split control
    void split();
    void undo_split();
    void skip_split();

    // Timer state queries
    emu::TimerState get_state() const { return m_data.state; }
    uint64_t get_current_time_ms() const;
    int get_current_split_index() const { return m_data.current_split; }
    int get_total_splits() const { return static_cast<int>(m_data.splits.size()); }

    // Split timing
    emu::SplitTiming get_split_timing(int index) const;
    uint64_t get_best_possible_time_ms() const;
    uint64_t get_sum_of_best_ms() const;
    const char* get_split_name(int index) const;
    uint64_t get_gold_time(int index) const;

    // Comparison management
    emu::ComparisonType get_comparison_type() const { return m_data.comparison_type; }
    void set_comparison_type(emu::ComparisonType type) { m_data.comparison_type = type; }

    // Run history
    int get_attempt_count() const { return m_data.attempt_count; }
    int get_completed_count() const { return m_data.completed_count; }

    // Data access
    TimerData& data() { return m_data; }
    const TimerData& data() const { return m_data; }

    // Split management
    void set_splits(const std::vector<std::string>& split_names);
    void clear_splits();

    // Personal best
    bool has_personal_best() const { return m_data.has_pb; }
    uint64_t get_personal_best_time() const;
    void save_personal_best();

    // Game info
    const std::string& get_game_name() const { return m_data.game_name; }
    void set_game_name(const std::string& name) { m_data.game_name = name; }
    const std::string& get_category() const { return m_data.category; }
    void set_category(const std::string& category) { m_data.category = category; }

    // Event callbacks
    void set_on_timer_started(TimerEventCallback callback) { m_on_timer_started = callback; }
    void set_on_timer_stopped(TimerEventCallback callback) { m_on_timer_stopped = callback; }
    void set_on_run_reset(TimerEventCallback callback) { m_on_run_reset = callback; }
    void set_on_split_triggered(SplitEventCallback callback) { m_on_split_triggered = callback; }
    void set_on_run_completed(RunCompleteCallback callback) { m_on_run_completed = callback; }

private:
    TimerData m_data;

    // Event callbacks
    TimerEventCallback m_on_timer_started;
    TimerEventCallback m_on_timer_stopped;
    TimerEventCallback m_on_run_reset;
    SplitEventCallback m_on_split_triggered;
    RunCompleteCallback m_on_run_completed;
};

// Time formatting utilities
std::string format_time(uint64_t ms, bool show_ms = true);
std::string format_delta(int64_t ms);

// Filename sanitization
std::string sanitize_filename(const std::string& name);

} // namespace game_plugin_common
