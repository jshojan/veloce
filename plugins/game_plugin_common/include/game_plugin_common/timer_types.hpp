#pragma once

#include "emu/game_plugin.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace game_plugin_common {

// Personal best data structure
struct PersonalBest {
    std::string category;
    uint64_t total_time_ms = 0;
    std::vector<uint64_t> split_times;  // Time at each split
    std::vector<uint64_t> gold_times;   // Best segment times (sum of best)
};

// Run history entry
struct RunHistoryEntry {
    uint64_t time_ms;
    bool completed;
    std::chrono::system_clock::time_point timestamp;
};

// Split state for current run
struct SplitState {
    std::string name;
    uint64_t split_time_ms = 0;      // Time when this split was hit
    uint64_t segment_time_ms = 0;    // Time for this segment only
    bool completed = false;
};

// Complete timer data structure
// This holds all state needed for a speedrun timer
struct TimerData {
    // Timer state
    emu::TimerState state = emu::TimerState::NotRunning;
    std::chrono::steady_clock::time_point start_time;
    uint64_t accumulated_time_ms = 0;
    int current_split = 0;

    // Split data
    std::vector<SplitState> splits;
    std::string game_name;
    std::string category = "Any%";

    // Personal best
    PersonalBest personal_best;
    bool has_pb = false;

    // Run history
    std::vector<RunHistoryEntry> run_history;
    int attempt_count = 0;
    int completed_count = 0;

    // Comparison
    emu::ComparisonType comparison_type = emu::ComparisonType::PersonalBest;

    // File management
    std::string splits_path;
    bool unsaved_changes = false;
    bool autosave_enabled = true;
};

} // namespace game_plugin_common
