#include "game_plugin_common/timer_core.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>

namespace game_plugin_common {

TimerCore::TimerCore() = default;

void TimerCore::start() {
    if (m_data.state == emu::TimerState::NotRunning ||
        m_data.state == emu::TimerState::Finished) {
        m_data.state = emu::TimerState::Running;
        m_data.start_time = std::chrono::steady_clock::now();
        m_data.accumulated_time_ms = 0;
        m_data.current_split = 0;
        m_data.attempt_count++;

        // Reset all splits
        for (auto& split : m_data.splits) {
            split.split_time_ms = 0;
            split.segment_time_ms = 0;
            split.completed = false;
        }

        if (m_on_timer_started) {
            m_on_timer_started();
        }
    } else if (m_data.state == emu::TimerState::Paused) {
        resume();
    }
}

void TimerCore::stop() {
    if (m_data.state == emu::TimerState::Running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_data.start_time).count();
        m_data.accumulated_time_ms += elapsed;
        m_data.state = emu::TimerState::Finished;

        if (m_on_timer_stopped) {
            m_on_timer_stopped();
        }
    }
}

void TimerCore::reset() {
    m_data.state = emu::TimerState::NotRunning;
    m_data.accumulated_time_ms = 0;
    m_data.current_split = 0;

    for (auto& split : m_data.splits) {
        split.split_time_ms = 0;
        split.segment_time_ms = 0;
        split.completed = false;
    }

    if (m_on_run_reset) {
        m_on_run_reset();
    }
}

void TimerCore::pause() {
    if (m_data.state == emu::TimerState::Running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_data.start_time).count();
        m_data.accumulated_time_ms += elapsed;
        m_data.state = emu::TimerState::Paused;
    }
}

void TimerCore::resume() {
    if (m_data.state == emu::TimerState::Paused) {
        m_data.start_time = std::chrono::steady_clock::now();
        m_data.state = emu::TimerState::Running;
    }
}

void TimerCore::split() {
    if (m_data.current_split >= static_cast<int>(m_data.splits.size())) return;
    if (m_data.state != emu::TimerState::Running) return;

    uint64_t current_time = get_current_time_ms();

    // Calculate segment time
    uint64_t prev_split_time = 0;
    if (m_data.current_split > 0 && m_data.splits[m_data.current_split - 1].completed) {
        prev_split_time = m_data.splits[m_data.current_split - 1].split_time_ms;
    }

    m_data.splits[m_data.current_split].split_time_ms = current_time;
    m_data.splits[m_data.current_split].segment_time_ms = current_time - prev_split_time;
    m_data.splits[m_data.current_split].completed = true;

    // Update gold (best segment) if this is a new best
    if (m_data.has_pb && m_data.current_split < static_cast<int>(m_data.personal_best.gold_times.size())) {
        if (m_data.splits[m_data.current_split].segment_time_ms < m_data.personal_best.gold_times[m_data.current_split]) {
            m_data.personal_best.gold_times[m_data.current_split] = m_data.splits[m_data.current_split].segment_time_ms;
            m_data.unsaved_changes = true;
        }
    }

    if (m_on_split_triggered) {
        m_on_split_triggered(m_data.current_split);
    }

    m_data.current_split++;

    // Check if run is complete
    if (m_data.current_split >= static_cast<int>(m_data.splits.size())) {
        stop();
        m_data.completed_count++;

        // Record in run history
        RunHistoryEntry entry;
        entry.time_ms = current_time;
        entry.completed = true;
        entry.timestamp = std::chrono::system_clock::now();
        m_data.run_history.push_back(entry);

        if (m_on_run_completed) {
            m_on_run_completed(current_time);
        }

        // Check for new PB
        if (!m_data.has_pb || current_time < m_data.personal_best.total_time_ms) {
            save_personal_best();
        }
    }
}

void TimerCore::undo_split() {
    if (m_data.current_split > 0) {
        m_data.current_split--;
        m_data.splits[m_data.current_split].completed = false;
        m_data.splits[m_data.current_split].split_time_ms = 0;
        m_data.splits[m_data.current_split].segment_time_ms = 0;

        // If we were finished, go back to running
        if (m_data.state == emu::TimerState::Finished) {
            m_data.state = emu::TimerState::Running;
            m_data.start_time = std::chrono::steady_clock::now();
        }
    }
}

void TimerCore::skip_split() {
    if (m_data.current_split < static_cast<int>(m_data.splits.size())) {
        m_data.splits[m_data.current_split].completed = false;
        m_data.splits[m_data.current_split].split_time_ms = 0;
        m_data.splits[m_data.current_split].segment_time_ms = 0;
        m_data.current_split++;
    }
}

uint64_t TimerCore::get_current_time_ms() const {
    if (m_data.state == emu::TimerState::Running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_data.start_time).count();
        return m_data.accumulated_time_ms + elapsed;
    }
    return m_data.accumulated_time_ms;
}

emu::SplitTiming TimerCore::get_split_timing(int index) const {
    emu::SplitTiming timing{};

    if (index < 0 || index >= static_cast<int>(m_data.splits.size())) {
        return timing;
    }

    const auto& split = m_data.splits[index];
    timing.time_ms = split.split_time_ms;

    if (split.completed && m_data.has_pb) {
        // Calculate delta based on comparison type
        int64_t delta = 0;
        switch (m_data.comparison_type) {
            case emu::ComparisonType::PersonalBest:
                if (index < static_cast<int>(m_data.personal_best.split_times.size())) {
                    delta = static_cast<int64_t>(split.split_time_ms) -
                            static_cast<int64_t>(m_data.personal_best.split_times[index]);
                }
                break;
            case emu::ComparisonType::BestSegments:
                delta = static_cast<int64_t>(split.segment_time_ms) -
                        static_cast<int64_t>(get_gold_time(index));
                break;
            default:
                if (index < static_cast<int>(m_data.personal_best.split_times.size())) {
                    delta = static_cast<int64_t>(split.split_time_ms) -
                            static_cast<int64_t>(m_data.personal_best.split_times[index]);
                }
                break;
        }
        timing.delta_ms = delta;

        // Check if this is a gold segment
        if (m_data.has_pb && index < static_cast<int>(m_data.personal_best.gold_times.size())) {
            timing.is_gold = split.segment_time_ms < m_data.personal_best.gold_times[index];
        }

        // Check if this is PB pace
        if (index < static_cast<int>(m_data.personal_best.split_times.size())) {
            timing.is_pb = split.split_time_ms < m_data.personal_best.split_times[index];
        }
    }

    return timing;
}

uint64_t TimerCore::get_best_possible_time_ms() const {
    if (!m_data.has_pb || m_data.splits.empty()) return 0;

    uint64_t best_possible = get_current_time_ms();

    // Add sum of best for remaining splits
    for (int i = m_data.current_split; i < static_cast<int>(m_data.splits.size()); i++) {
        if (i < static_cast<int>(m_data.personal_best.gold_times.size())) {
            best_possible += m_data.personal_best.gold_times[i];
        }
    }

    return best_possible;
}

uint64_t TimerCore::get_sum_of_best_ms() const {
    if (!m_data.has_pb) return 0;

    uint64_t sum = 0;
    for (uint64_t gold : m_data.personal_best.gold_times) {
        sum += gold;
    }
    return sum;
}

const char* TimerCore::get_split_name(int index) const {
    if (index >= 0 && index < static_cast<int>(m_data.splits.size())) {
        return m_data.splits[index].name.c_str();
    }
    return nullptr;
}

uint64_t TimerCore::get_gold_time(int index) const {
    if (m_data.has_pb && index >= 0 && index < static_cast<int>(m_data.personal_best.gold_times.size())) {
        return m_data.personal_best.gold_times[index];
    }
    return 0;
}

void TimerCore::set_splits(const std::vector<std::string>& split_names) {
    m_data.splits.clear();
    m_data.splits.reserve(split_names.size());
    for (const auto& name : split_names) {
        SplitState split;
        split.name = name;
        m_data.splits.push_back(split);
    }
}

void TimerCore::clear_splits() {
    m_data.splits.clear();
    m_data.current_split = 0;
}

uint64_t TimerCore::get_personal_best_time() const {
    return m_data.has_pb ? m_data.personal_best.total_time_ms : 0;
}

void TimerCore::save_personal_best() {
    if (m_data.splits.empty()) return;

    m_data.personal_best.category = m_data.category;
    m_data.personal_best.total_time_ms = m_data.splits.back().split_time_ms;
    m_data.personal_best.split_times.clear();
    m_data.personal_best.gold_times.clear();

    for (size_t i = 0; i < m_data.splits.size(); i++) {
        m_data.personal_best.split_times.push_back(m_data.splits[i].split_time_ms);

        // Update gold if better
        if (m_data.has_pb && i < m_data.personal_best.gold_times.size()) {
            uint64_t existing_gold = m_data.personal_best.gold_times[i];
            m_data.personal_best.gold_times.push_back(
                std::min(existing_gold, m_data.splits[i].segment_time_ms));
        } else {
            m_data.personal_best.gold_times.push_back(m_data.splits[i].segment_time_ms);
        }
    }

    m_data.has_pb = true;
    m_data.unsaved_changes = true;

    std::cout << "[TimerCore] New personal best: " << m_data.personal_best.total_time_ms << "ms" << std::endl;
}

// Utility functions
std::string format_time(uint64_t ms, bool show_ms) {
    uint64_t total_seconds = ms / 1000;
    uint64_t hours = total_seconds / 3600;
    uint64_t minutes = (total_seconds % 3600) / 60;
    uint64_t seconds = total_seconds % 60;
    uint64_t millis = ms % 1000;

    char buf[32];
    if (hours > 0) {
        if (show_ms) {
            std::snprintf(buf, sizeof(buf), "%llu:%02llu:%02llu.%03llu",
                (unsigned long long)hours, (unsigned long long)minutes,
                (unsigned long long)seconds, (unsigned long long)millis);
        } else {
            std::snprintf(buf, sizeof(buf), "%llu:%02llu:%02llu",
                (unsigned long long)hours, (unsigned long long)minutes,
                (unsigned long long)seconds);
        }
    } else if (minutes > 0) {
        if (show_ms) {
            std::snprintf(buf, sizeof(buf), "%llu:%02llu.%03llu",
                (unsigned long long)minutes, (unsigned long long)seconds,
                (unsigned long long)millis);
        } else {
            std::snprintf(buf, sizeof(buf), "%llu:%02llu",
                (unsigned long long)minutes, (unsigned long long)seconds);
        }
    } else {
        if (show_ms) {
            std::snprintf(buf, sizeof(buf), "%llu.%03llu",
                (unsigned long long)seconds, (unsigned long long)millis);
        } else {
            std::snprintf(buf, sizeof(buf), "%llu",
                (unsigned long long)seconds);
        }
    }

    return buf;
}

std::string format_delta(int64_t ms) {
    bool negative = ms < 0;
    uint64_t abs_ms = negative ? static_cast<uint64_t>(-ms) : static_cast<uint64_t>(ms);

    uint64_t total_seconds = abs_ms / 1000;
    uint64_t minutes = total_seconds / 60;
    uint64_t seconds = total_seconds % 60;
    uint64_t millis = abs_ms % 1000;

    char buf[32];
    if (minutes > 0) {
        std::snprintf(buf, sizeof(buf), "%c%llu:%02llu.%01llu",
            negative ? '-' : '+',
            (unsigned long long)minutes,
            (unsigned long long)seconds,
            (unsigned long long)(millis / 100));
    } else {
        std::snprintf(buf, sizeof(buf), "%c%llu.%01llu",
            negative ? '-' : '+',
            (unsigned long long)seconds,
            (unsigned long long)(millis / 100));
    }

    return buf;
}

std::string sanitize_filename(const std::string& name) {
    std::string result = name;
    for (char& c : result) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }
    return result;
}

} // namespace game_plugin_common
