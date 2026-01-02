#include "speedrun_manager.hpp"
#include "plugin_manager.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace emu {

SpeedrunManager::SpeedrunManager() = default;

SpeedrunManager::~SpeedrunManager() {
    unload_plugin();
}

void SpeedrunManager::initialize(PluginManager* plugin_manager) {
    m_plugin_manager = plugin_manager;
}

bool SpeedrunManager::load_plugin_for_rom(uint32_t crc32, const std::string& rom_name) {
    unload_plugin();
    reset_timer();

    // Get the game plugin from the plugin manager (it handles discovery)
    if (m_plugin_manager) {
        m_active_plugin = m_plugin_manager->get_game_plugin();
        if (m_active_plugin && m_active_plugin->matches_rom(crc32, rom_name.c_str())) {
            load_splits_from_plugin();
            m_active_plugin->on_rom_loaded(this);
            return true;
        }
        m_active_plugin = nullptr;
    }

    return false;
}

void SpeedrunManager::unload_plugin() {
    if (m_active_plugin) {
        m_active_plugin->on_rom_unloaded();
    }
    // Plugin lifetime is managed by PluginManager, we just clear our reference
    m_active_plugin = nullptr;
}

void SpeedrunManager::update() {
    if (m_active_plugin && m_running) {
        m_active_plugin->on_frame(this);
    }
}

uint8_t SpeedrunManager::read_memory(uint16_t address) {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        return m_plugin_manager->get_emulator_plugin()->read_memory(address);
    }
    return 0;
}

uint16_t SpeedrunManager::read_memory_16(uint16_t address) {
    // Little-endian read
    uint16_t lo = read_memory(address);
    uint16_t hi = read_memory(address + 1);
    return lo | (hi << 8);
}

uint32_t SpeedrunManager::read_memory_32(uint16_t address) {
    // Little-endian read
    uint32_t b0 = read_memory(address);
    uint32_t b1 = read_memory(address + 1);
    uint32_t b2 = read_memory(address + 2);
    uint32_t b3 = read_memory(address + 3);
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

void SpeedrunManager::write_memory(uint16_t address, uint8_t value) {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        m_plugin_manager->get_emulator_plugin()->write_memory(address, value);
    }
}

void SpeedrunManager::start_timer() {
    if (!m_running) {
        m_running = true;
        m_start_time = std::chrono::steady_clock::now();
    }
}

void SpeedrunManager::stop_timer() {
    if (m_running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_start_time).count();
        m_accumulated_time_ms += elapsed;
        m_running = false;
    }
}

void SpeedrunManager::reset_timer() {
    m_running = false;
    m_accumulated_time_ms = 0;
    m_current_split = 0;

    for (auto& split : m_splits) {
        split.split_time_ms = 0;
        split.segment_time_ms = 0;
        split.completed = false;
    }

    if (m_active_plugin) {
        m_active_plugin->on_reset();
    }
}

void SpeedrunManager::split() {
    if (m_current_split >= static_cast<int>(m_splits.size())) return;

    uint64_t current_time = get_current_time_ms();

    // Calculate segment time
    uint64_t prev_split_time = 0;
    if (m_current_split > 0 && m_splits[m_current_split - 1].completed) {
        prev_split_time = m_splits[m_current_split - 1].split_time_ms;
    }

    m_splits[m_current_split].split_time_ms = current_time;
    m_splits[m_current_split].segment_time_ms = current_time - prev_split_time;
    m_splits[m_current_split].completed = true;

    // Update gold if this is a new best segment
    if (m_has_pb && m_current_split < static_cast<int>(m_personal_best.gold_times.size())) {
        if (m_splits[m_current_split].segment_time_ms < m_personal_best.gold_times[m_current_split]) {
            m_personal_best.gold_times[m_current_split] = m_splits[m_current_split].segment_time_ms;
        }
    }

    if (m_on_split) {
        m_on_split(m_current_split);
    }

    m_current_split++;

    // Check if run is complete
    if (m_current_split >= static_cast<int>(m_splits.size())) {
        stop_timer();
        if (m_active_plugin) {
            m_active_plugin->on_run_complete(current_time);
        }

        // Check for new PB
        if (!m_has_pb || current_time < m_personal_best.total_time_ms) {
            save_personal_best();
        }
    }
}

void SpeedrunManager::undo_split() {
    if (m_current_split > 0) {
        m_current_split--;
        m_splits[m_current_split].completed = false;
        m_splits[m_current_split].split_time_ms = 0;
        m_splits[m_current_split].segment_time_ms = 0;
    }
}

void SpeedrunManager::skip_split() {
    if (m_current_split < static_cast<int>(m_splits.size())) {
        m_splits[m_current_split].completed = false;
        m_splits[m_current_split].split_time_ms = 0;
        m_splits[m_current_split].segment_time_ms = 0;
        m_current_split++;
    }
}

bool SpeedrunManager::is_timer_running() const {
    return m_running;
}

uint64_t SpeedrunManager::get_current_time_ms() const {
    if (m_running) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_start_time).count();
        return m_accumulated_time_ms + elapsed;
    }
    return m_accumulated_time_ms;
}

int SpeedrunManager::get_current_split_index() const {
    return m_current_split;
}

uint64_t SpeedrunManager::get_frame_count() const {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        return m_plugin_manager->get_emulator_plugin()->get_frame_count();
    }
    return 0;
}

const char* SpeedrunManager::get_selected_category() const {
    return m_category.c_str();
}

void SpeedrunManager::log_message(const char* message) {
    std::cout << "[GamePlugin] " << message << std::endl;
}

uint64_t SpeedrunManager::get_total_time_ms() const {
    return get_current_time_ms();
}

int64_t SpeedrunManager::get_delta_ms(int split_index) const {
    if (!m_has_pb || split_index < 0 ||
        split_index >= static_cast<int>(m_splits.size()) ||
        split_index >= static_cast<int>(m_personal_best.split_times.size())) {
        return 0;
    }

    if (!m_splits[split_index].completed) {
        return 0;
    }

    return static_cast<int64_t>(m_splits[split_index].split_time_ms) -
           static_cast<int64_t>(m_personal_best.split_times[split_index]);
}

int64_t SpeedrunManager::get_segment_delta_ms(int split_index) const {
    if (!m_has_pb || split_index < 0 ||
        split_index >= static_cast<int>(m_splits.size()) ||
        split_index >= static_cast<int>(m_personal_best.gold_times.size())) {
        return 0;
    }

    if (!m_splits[split_index].completed) {
        return 0;
    }

    return static_cast<int64_t>(m_splits[split_index].segment_time_ms) -
           static_cast<int64_t>(m_personal_best.gold_times[split_index]);
}

uint64_t SpeedrunManager::get_sum_of_best_ms() const {
    if (!m_has_pb) return 0;

    uint64_t sum = 0;
    for (uint64_t gold : m_personal_best.gold_times) {
        sum += gold;
    }
    return sum;
}

void SpeedrunManager::save_personal_best() {
    if (m_splits.empty()) return;

    m_personal_best.category = m_category;
    m_personal_best.total_time_ms = m_splits.back().split_time_ms;
    m_personal_best.split_times.clear();
    m_personal_best.gold_times.clear();

    for (const auto& split : m_splits) {
        m_personal_best.split_times.push_back(split.split_time_ms);

        // Update gold if better
        if (m_has_pb && m_personal_best.gold_times.size() > m_personal_best.split_times.size() - 1) {
            uint64_t existing_gold = m_personal_best.gold_times[m_personal_best.split_times.size() - 1];
            m_personal_best.gold_times.push_back(std::min(existing_gold, split.segment_time_ms));
        } else {
            m_personal_best.gold_times.push_back(split.segment_time_ms);
        }
    }

    m_has_pb = true;

    // Save to file
    try {
        nlohmann::json j;
        j["game"] = m_game_name;
        j["category"] = m_category;
        j["total_time_ms"] = m_personal_best.total_time_ms;
        j["split_times"] = m_personal_best.split_times;
        j["gold_times"] = m_personal_best.gold_times;

        std::vector<std::string> split_names;
        for (const auto& split : m_splits) {
            split_names.push_back(split.name);
        }
        j["split_names"] = split_names;

        std::filesystem::create_directories("splits");
        std::ofstream file(get_pb_filename());
        file << j.dump(2);

        std::cout << "Personal best saved!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save PB: " << e.what() << std::endl;
    }
}

void SpeedrunManager::load_personal_best(const std::string& game, const std::string& category) {
    m_game_name = game;
    m_category = category;
    m_has_pb = false;

    try {
        std::ifstream file(get_pb_filename());
        if (!file.is_open()) return;

        nlohmann::json j;
        file >> j;

        m_personal_best.category = j.value("category", "");
        m_personal_best.total_time_ms = j.value("total_time_ms", 0);
        m_personal_best.split_times = j.value("split_times", std::vector<uint64_t>{});
        m_personal_best.gold_times = j.value("gold_times", std::vector<uint64_t>{});

        m_has_pb = !m_personal_best.split_times.empty();

        std::cout << "Loaded PB: " << m_personal_best.total_time_ms << "ms" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load PB: " << e.what() << std::endl;
    }
}

void SpeedrunManager::load_splits_from_plugin() {
    if (!m_active_plugin) return;

    auto info = m_active_plugin->get_info();
    m_game_name = info.game_name ? info.game_name : "";
    m_category = info.categories && info.categories[0] ? info.categories[0] : "Any%";

    auto split_defs = m_active_plugin->get_splits(m_category.c_str());
    m_splits.clear();
    for (const auto& def : split_defs) {
        SplitTime split;
        split.name = def.name ? def.name : "";
        m_splits.push_back(split);
    }

    load_personal_best(m_game_name, m_category);
}

std::string SpeedrunManager::get_pb_filename() const {
    // Sanitize game name for filename
    std::string filename = m_game_name + "_" + m_category;
    for (char& c : filename) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            c = '_';
        }
    }
    return "splits/" + filename + ".json";
}

} // namespace emu
