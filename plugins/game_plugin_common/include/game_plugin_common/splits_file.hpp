#pragma once

#include "timer_types.hpp"
#include <string>

namespace game_plugin_common {

// SplitsFile - Handles loading and saving splits files (JSON format)
// This class manages persistence for speedrun splits, personal bests,
// and run history.
class SplitsFile {
public:
    SplitsFile() = default;
    ~SplitsFile() = default;

    // Load splits from a JSON file
    // Returns true on success, false on failure
    bool load(const std::string& path, TimerData& data);

    // Save splits to a JSON file
    // Returns true on success, false on failure
    bool save(const std::string& path, const TimerData& data);

    // Save to the last loaded path
    bool save(const TimerData& data);

    // Get the current file path
    const std::string& get_path() const { return m_path; }

    // Check if there's a valid path set
    bool has_path() const { return !m_path.empty(); }

    // Generate a default splits path for a game
    static std::string generate_default_path(const std::string& game_name,
                                              const std::string& category);

private:
    std::string m_path;
};

} // namespace game_plugin_common
