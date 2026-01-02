#include "savestate_manager.hpp"
#include "plugin_manager.hpp"
#include "paths_config.hpp"
#include "emu/emulator_plugin.hpp"

#include <fstream>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <cstring>

namespace emu {

// Savestate file format header
// Version history:
// 1 - Initial format
// 2 - Added complete PPU NMI state, sprite state, CPU m_nmi_delayed flag
struct SavestateHeader {
    char magic[4] = {'V', 'E', 'L', 'O'};  // "VELO" - Veloce Savestate
    uint32_t version = 2;
    uint32_t rom_crc32 = 0;
    uint64_t frame_count = 0;
    int64_t timestamp = 0;
    uint32_t data_size = 0;
    char rom_name[256] = {0};
};

SavestateManager::SavestateManager() = default;
SavestateManager::~SavestateManager() = default;

void SavestateManager::initialize(PluginManager* plugin_manager, PathsConfiguration* paths_config) {
    m_plugin_manager = plugin_manager;
    m_paths_config = paths_config;

    // The paths configuration handles directory creation
    if (m_paths_config) {
        m_paths_config->ensure_directories_exist();
    }
}

bool SavestateManager::save_state(int slot) {
    if (slot < 0 || slot >= NUM_SLOTS) {
        std::cerr << "Invalid savestate slot: " << slot << std::endl;
        return false;
    }

    if (!m_plugin_manager) {
        std::cerr << "SavestateManager not initialized" << std::endl;
        return false;
    }

    auto* plugin = m_plugin_manager->get_active_plugin();
    if (!plugin || !plugin->is_rom_loaded()) {
        std::cerr << "No ROM loaded, cannot save state" << std::endl;
        return false;
    }

    // Serialize emulator state
    std::vector<uint8_t> data;
    if (!plugin->save_state(data)) {
        std::cerr << "Failed to serialize emulator state" << std::endl;
        return false;
    }

    // Build savestate info
    SavestateInfo info;
    info.rom_name = m_current_rom_name;
    info.rom_crc32 = plugin->get_rom_crc32();
    info.frame_count = plugin->get_frame_count();
    info.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    info.valid = true;

    // Write to file
    std::string path = get_savestate_path(slot);
    if (!write_savestate_file(path, data, info)) {
        std::cerr << "Failed to write savestate file" << std::endl;
        return false;
    }

    std::cout << "Saved state to slot " << slot << " (" << data.size() << " bytes)" << std::endl;
    return true;
}

bool SavestateManager::load_state(int slot) {
    if (slot < 0 || slot >= NUM_SLOTS) {
        std::cerr << "Invalid savestate slot: " << slot << std::endl;
        return false;
    }

    if (!m_plugin_manager) {
        std::cerr << "SavestateManager not initialized" << std::endl;
        return false;
    }

    auto* plugin = m_plugin_manager->get_active_plugin();
    if (!plugin || !plugin->is_rom_loaded()) {
        std::cerr << "No ROM loaded, cannot load state" << std::endl;
        return false;
    }

    // Read savestate file
    std::string path = get_savestate_path(slot);
    SavestateInfo info;
    auto data = read_savestate_file(path, info);

    if (!data.has_value()) {
        std::cerr << "Failed to read savestate file" << std::endl;
        return false;
    }

    // Verify ROM CRC matches
    if (info.rom_crc32 != plugin->get_rom_crc32()) {
        std::cerr << "Savestate ROM CRC mismatch! Expected: " << std::hex
                  << plugin->get_rom_crc32() << ", got: " << info.rom_crc32 << std::dec << std::endl;
        return false;
    }

    // Load the state
    if (!plugin->load_state(data.value())) {
        std::cerr << "Failed to deserialize emulator state" << std::endl;
        return false;
    }

    std::cout << "Loaded state from slot " << slot << " (frame " << info.frame_count << ")" << std::endl;
    return true;
}

bool SavestateManager::quick_save() {
    return save_state(0);
}

bool SavestateManager::quick_load() {
    return load_state(0);
}

SavestateInfo SavestateManager::get_slot_info(int slot) const {
    SavestateInfo info = {};
    info.valid = false;

    if (slot < 0 || slot >= NUM_SLOTS) {
        return info;
    }

    std::string path = get_savestate_path(slot);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return info;
    }

    SavestateHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (file && std::memcmp(header.magic, "VELO", 4) == 0) {
        info.rom_name = header.rom_name;
        info.rom_crc32 = header.rom_crc32;
        info.frame_count = header.frame_count;
        info.timestamp = header.timestamp;
        info.valid = true;
    }

    return info;
}

bool SavestateManager::is_slot_valid(int slot) const {
    return get_slot_info(slot).valid;
}

std::string SavestateManager::get_savestate_path(int slot) const {
    // Use ROM CRC as part of filename to separate saves per game
    if (!m_plugin_manager) return "";

    auto* plugin = m_plugin_manager->get_active_plugin();
    if (!plugin || !plugin->is_rom_loaded()) return "";

    uint32_t crc = plugin->get_rom_crc32();

    // Use paths configuration if available, otherwise fall back to default
    if (m_paths_config) {
        return m_paths_config->get_savestate_path(crc, slot).string();
    }

    // Fallback path if no config is set
    char filename[64];
    std::snprintf(filename, sizeof(filename), "%08X_slot%d.state", crc, slot);
    return std::string("savestates/") + filename;
}

bool SavestateManager::save_state_to_file(const std::string& path) {
    if (!m_plugin_manager) {
        std::cerr << "SavestateManager not initialized" << std::endl;
        return false;
    }

    auto* plugin = m_plugin_manager->get_active_plugin();
    if (!plugin || !plugin->is_rom_loaded()) {
        std::cerr << "No ROM loaded, cannot save state" << std::endl;
        return false;
    }

    // Serialize emulator state
    std::vector<uint8_t> data;
    if (!plugin->save_state(data)) {
        std::cerr << "Failed to serialize emulator state" << std::endl;
        return false;
    }

    // Build savestate info
    SavestateInfo info;
    info.rom_name = m_current_rom_name;
    info.rom_crc32 = plugin->get_rom_crc32();
    info.frame_count = plugin->get_frame_count();
    info.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    info.valid = true;

    // Write to file
    if (!write_savestate_file(path, data, info)) {
        std::cerr << "Failed to write savestate file: " << path << std::endl;
        return false;
    }

    std::cout << "Saved state to file: " << path << " (" << data.size() << " bytes)" << std::endl;
    return true;
}

bool SavestateManager::load_state_from_file(const std::string& path) {
    if (!m_plugin_manager) {
        std::cerr << "SavestateManager not initialized" << std::endl;
        return false;
    }

    auto* plugin = m_plugin_manager->get_active_plugin();
    if (!plugin || !plugin->is_rom_loaded()) {
        std::cerr << "No ROM loaded, cannot load state" << std::endl;
        return false;
    }

    // Read savestate file
    SavestateInfo info;
    auto data = read_savestate_file(path, info);

    if (!data.has_value()) {
        std::cerr << "Failed to read savestate file: " << path << std::endl;
        return false;
    }

    // Verify ROM CRC matches
    if (info.rom_crc32 != plugin->get_rom_crc32()) {
        std::cerr << "Savestate ROM CRC mismatch! Expected: " << std::hex
                  << plugin->get_rom_crc32() << ", got: " << info.rom_crc32 << std::dec << std::endl;
        return false;
    }

    // Load the state
    if (!plugin->load_state(data.value())) {
        std::cerr << "Failed to deserialize emulator state" << std::endl;
        return false;
    }

    std::cout << "Loaded state from file: " << path << " (frame " << info.frame_count << ")" << std::endl;
    return true;
}

bool SavestateManager::write_savestate_file(const std::string& path,
                                             const std::vector<uint8_t>& data,
                                             const SavestateInfo& info) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    // Write header
    SavestateHeader header;
    header.rom_crc32 = info.rom_crc32;
    header.frame_count = info.frame_count;
    header.timestamp = info.timestamp;
    header.data_size = static_cast<uint32_t>(data.size());
    std::strncpy(header.rom_name, info.rom_name.c_str(), sizeof(header.rom_name) - 1);

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write state data
    file.write(reinterpret_cast<const char*>(data.data()), data.size());

    return file.good();
}

std::optional<std::vector<uint8_t>> SavestateManager::read_savestate_file(const std::string& path,
                                                                           SavestateInfo& info) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;

    // Read header
    SavestateHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (!file || std::memcmp(header.magic, "VELO", 4) != 0) {
        return std::nullopt;
    }

    if (header.version < 1 || header.version > 2) {
        std::cerr << "Unsupported savestate version: " << header.version << std::endl;
        return std::nullopt;
    }
    // Note: Version 1 savestates are not compatible with version 2 due to
    // added NMI/sprite state fields. Old savestates will fail to load correctly.

    // Fill info
    info.rom_name = header.rom_name;
    info.rom_crc32 = header.rom_crc32;
    info.frame_count = header.frame_count;
    info.timestamp = header.timestamp;
    info.valid = true;

    // Read state data
    std::vector<uint8_t> data(header.data_size);
    file.read(reinterpret_cast<char*>(data.data()), header.data_size);

    if (!file) return std::nullopt;

    return data;
}

} // namespace emu
