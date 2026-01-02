#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace emu {

class PluginManager;
class PathsConfiguration;

// Metadata stored with each savestate
struct SavestateInfo {
    std::string rom_name;           // Name of the ROM
    uint32_t rom_crc32;             // ROM checksum for validation
    uint64_t frame_count;           // Frame when saved
    int64_t timestamp;              // Unix timestamp when saved
    bool valid;                     // Whether this slot has a valid savestate
};

class SavestateManager {
public:
    static constexpr int NUM_SLOTS = 10;  // Slots 0-9 (F1-F10 hotkeys)

    SavestateManager();
    ~SavestateManager();

    // Initialize with plugin manager and paths configuration
    void initialize(PluginManager* plugin_manager, PathsConfiguration* paths_config);

    // Save current state to slot (0-9)
    bool save_state(int slot);

    // Load state from slot (0-9)
    bool load_state(int slot);

    // Quick save/load (uses slot 0)
    bool quick_save();
    bool quick_load();

    // Get info about a slot
    SavestateInfo get_slot_info(int slot) const;

    // Check if slot has a valid savestate
    bool is_slot_valid(int slot) const;

    // Get current ROM's savestate path for a slot
    std::string get_savestate_path(int slot) const;

    // Save/load to arbitrary file path
    bool save_state_to_file(const std::string& path);
    bool load_state_from_file(const std::string& path);

    // Set the current ROM name (used for organizing saves)
    void set_current_rom_name(const std::string& name) { m_current_rom_name = name; }

private:
    bool write_savestate_file(const std::string& path, const std::vector<uint8_t>& data,
                              const SavestateInfo& info);
    std::optional<std::vector<uint8_t>> read_savestate_file(const std::string& path,
                                                             SavestateInfo& info);

    PluginManager* m_plugin_manager = nullptr;
    PathsConfiguration* m_paths_config = nullptr;
    std::string m_current_rom_name;
};

} // namespace emu
