#pragma once

#include "emu/game_plugin.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>

namespace emu {

class PluginManager;

// Split time data
struct SplitTime {
    std::string name;
    uint64_t split_time_ms = 0;      // Time when this split was hit
    uint64_t segment_time_ms = 0;    // Time for this segment only
    bool completed = false;
};

// Personal best data
struct PersonalBest {
    std::string category;
    uint64_t total_time_ms = 0;
    std::vector<uint64_t> split_times;  // Time at each split
    std::vector<uint64_t> gold_times;   // Best segment times
};

// Speedrun manager - handles timer, splits, PB tracking
// Now implements IGameHost to work with IGamePlugin
class SpeedrunManager : public IGameHost {
public:
    SpeedrunManager();
    ~SpeedrunManager();

    // Initialize with plugin manager for memory access
    void initialize(PluginManager* plugin_manager);

    // Load game plugin for current ROM (uses PluginManager's game plugin)
    bool load_plugin_for_rom(uint32_t crc32, const std::string& rom_name);
    void unload_plugin();

    // Called each frame
    void update();

    // IGameHost interface
    uint8_t read_memory(uint16_t address) override;
    uint16_t read_memory_16(uint16_t address) override;
    uint32_t read_memory_32(uint16_t address) override;
    void write_memory(uint16_t address, uint8_t value) override;
    void start_timer() override;
    void stop_timer() override;
    void reset_timer() override;
    void split() override;
    void undo_split() override;
    void skip_split() override;
    bool is_timer_running() const override;
    uint64_t get_current_time_ms() const override;
    int get_current_split_index() const override;
    uint64_t get_frame_count() const override;
    const char* get_selected_category() const override;
    void log_message(const char* message) override;

    // Timer info
    uint64_t get_total_time_ms() const;
    const std::vector<SplitTime>& get_splits() const { return m_splits; }
    const PersonalBest* get_personal_best() const { return m_has_pb ? &m_personal_best : nullptr; }
    bool has_active_plugin() const { return m_active_plugin != nullptr; }
    const std::string& get_category() const { return m_category; }
    const std::string& get_game_name() const { return m_game_name; }

    // Comparison helpers
    int64_t get_delta_ms(int split_index) const;  // Delta vs PB
    int64_t get_segment_delta_ms(int split_index) const;  // Segment vs gold
    uint64_t get_sum_of_best_ms() const;

    // PB management
    void save_personal_best();
    void load_personal_best(const std::string& game, const std::string& category);

    // Callbacks for GUI updates
    using SplitCallback = std::function<void(int split_index)>;
    void set_on_split(SplitCallback callback) { m_on_split = callback; }

private:
    void load_splits_from_plugin();
    std::string get_pb_filename() const;

    PluginManager* m_plugin_manager = nullptr;
    IGamePlugin* m_active_plugin = nullptr;

    // Timer state
    bool m_running = false;
    std::chrono::steady_clock::time_point m_start_time;
    uint64_t m_accumulated_time_ms = 0;  // Time from before pause
    int m_current_split = 0;

    // Split data
    std::vector<SplitTime> m_splits;
    std::string m_game_name;
    std::string m_category;

    // Personal best
    PersonalBest m_personal_best;
    bool m_has_pb = false;

    // Callbacks
    SplitCallback m_on_split;
};

} // namespace emu
