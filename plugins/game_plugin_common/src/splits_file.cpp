#include "game_plugin_common/splits_file.hpp"
#include "game_plugin_common/timer_core.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace game_plugin_common {

bool SplitsFile::load(const std::string& path, TimerData& data) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        data.game_name = j.value("game", "");
        data.category = j.value("category", "Any%");
        data.attempt_count = j.value("attempts", 0);
        data.completed_count = j.value("completed", 0);

        // Load splits
        data.splits.clear();
        if (j.contains("splits") && j["splits"].is_array()) {
            for (const auto& split_json : j["splits"]) {
                SplitState split;
                split.name = split_json.value("name", "");
                data.splits.push_back(split);
            }
        }

        // Load personal best
        if (j.contains("personal_best")) {
            auto& pb = j["personal_best"];
            data.personal_best.category = pb.value("category", data.category);
            data.personal_best.total_time_ms = pb.value("total_time_ms", 0);
            data.personal_best.split_times = pb.value("split_times", std::vector<uint64_t>{});
            data.personal_best.gold_times = pb.value("gold_times", std::vector<uint64_t>{});
            data.has_pb = !data.personal_best.split_times.empty();
        } else {
            data.has_pb = false;
        }

        data.splits_path = path;
        data.unsaved_changes = false;
        m_path = path;

        std::cout << "[SplitsFile] Loaded: " << path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[SplitsFile] Failed to load: " << e.what() << std::endl;
        return false;
    }
}

bool SplitsFile::save(const std::string& path, const TimerData& data) {
    try {
        nlohmann::json j;
        j["game"] = data.game_name;
        j["category"] = data.category;
        j["attempts"] = data.attempt_count;
        j["completed"] = data.completed_count;

        // Save splits
        nlohmann::json splits_array = nlohmann::json::array();
        for (const auto& split : data.splits) {
            nlohmann::json split_json;
            split_json["name"] = split.name;
            splits_array.push_back(split_json);
        }
        j["splits"] = splits_array;

        // Save personal best
        if (data.has_pb) {
            nlohmann::json pb;
            pb["category"] = data.personal_best.category;
            pb["total_time_ms"] = data.personal_best.total_time_ms;
            pb["split_times"] = data.personal_best.split_times;
            pb["gold_times"] = data.personal_best.gold_times;
            j["personal_best"] = pb;
        }

        // Ensure directory exists
        std::filesystem::path filepath(path);
        if (filepath.has_parent_path()) {
            std::filesystem::create_directories(filepath.parent_path());
        }

        std::ofstream file(path);
        file << j.dump(2);

        m_path = path;

        std::cout << "[SplitsFile] Saved: " << path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[SplitsFile] Failed to save: " << e.what() << std::endl;
        return false;
    }
}

bool SplitsFile::save(const TimerData& data) {
    if (m_path.empty()) return false;
    return save(m_path, data);
}

std::string SplitsFile::generate_default_path(const std::string& game_name,
                                               const std::string& category) {
    std::string filename = sanitize_filename(game_name + "_" + category);
    return "splits/" + filename + ".json";
}

} // namespace game_plugin_common
