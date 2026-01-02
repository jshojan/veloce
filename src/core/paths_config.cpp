#include "paths_config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstdio>

namespace emu {

namespace fs = std::filesystem;

PathsConfiguration::PathsConfiguration() = default;

void PathsConfiguration::initialize(const fs::path& exe_directory) {
    m_base_directory = exe_directory;
    m_config_path = m_base_directory / DEFAULT_CONFIG_DIR / CONFIG_FILENAME;

    // Set defaults relative to base directory
    reset_to_defaults();

    m_initialized = true;
}

void PathsConfiguration::reset_to_defaults() {
    m_save_directory = DEFAULT_SAVE_DIR;
    m_savestate_directory = DEFAULT_SAVESTATE_DIR;
    m_screenshot_directory = DEFAULT_SCREENSHOT_DIR;
    m_rom_directory = m_base_directory;
    m_modified = true;
}

bool PathsConfiguration::load(const fs::path& config_path) {
    m_config_path = config_path;
    return load();
}

bool PathsConfiguration::load() {
    if (!fs::exists(m_config_path)) {
        // No config file yet, use defaults
        std::cout << "Paths config not found, using defaults" << std::endl;
        return true;
    }

    try {
        std::ifstream file(m_config_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open paths config: " << m_config_path << std::endl;
            return false;
        }

        nlohmann::json json;
        file >> json;

        // Load directory paths
        if (json.contains("save_directory") && json["save_directory"].is_string()) {
            m_save_directory = json["save_directory"].get<std::string>();
        }
        if (json.contains("savestate_directory") && json["savestate_directory"].is_string()) {
            m_savestate_directory = json["savestate_directory"].get<std::string>();
        }
        if (json.contains("screenshot_directory") && json["screenshot_directory"].is_string()) {
            m_screenshot_directory = json["screenshot_directory"].get<std::string>();
        }
        if (json.contains("rom_directory") && json["rom_directory"].is_string()) {
            m_rom_directory = json["rom_directory"].get<std::string>();
        }

        m_modified = false;
        std::cout << "Loaded paths configuration from: " << m_config_path << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading paths config: " << e.what() << std::endl;
        return false;
    }
}

bool PathsConfiguration::save(const fs::path& config_path) const {
    // Save to the specified path
    try {
        nlohmann::json json;

        // Save as strings - relative paths stay relative, absolute stay absolute
        json["save_directory"] = m_save_directory.string();
        json["savestate_directory"] = m_savestate_directory.string();
        json["screenshot_directory"] = m_screenshot_directory.string();
        json["rom_directory"] = m_rom_directory.string();

        // Create parent directories if needed
        if (config_path.has_parent_path()) {
            fs::create_directories(config_path.parent_path());
        }

        std::ofstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open paths config for writing: " << config_path << std::endl;
            return false;
        }

        file << json.dump(4);
        std::cout << "Saved paths configuration to: " << config_path << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error saving paths config: " << e.what() << std::endl;
        return false;
    }
}

bool PathsConfiguration::save() const {
    return save(m_config_path);
}

fs::path PathsConfiguration::resolve_path(const fs::path& path) const {
    if (path.is_absolute()) {
        return path;
    }
    return m_base_directory / path;
}

fs::path PathsConfiguration::get_save_directory() const {
    return resolve_path(m_save_directory);
}

void PathsConfiguration::set_save_directory(const fs::path& path) {
    m_save_directory = path;
    m_modified = true;
}

fs::path PathsConfiguration::get_savestate_directory() const {
    return resolve_path(m_savestate_directory);
}

void PathsConfiguration::set_savestate_directory(const fs::path& path) {
    m_savestate_directory = path;
    m_modified = true;
}

fs::path PathsConfiguration::get_screenshot_directory() const {
    return resolve_path(m_screenshot_directory);
}

void PathsConfiguration::set_screenshot_directory(const fs::path& path) {
    m_screenshot_directory = path;
    m_modified = true;
}

fs::path PathsConfiguration::get_rom_directory() const {
    return resolve_path(m_rom_directory);
}

void PathsConfiguration::set_rom_directory(const fs::path& path) {
    m_rom_directory = path;
    m_modified = true;
}

fs::path PathsConfiguration::get_config_directory() const {
    return m_base_directory / DEFAULT_CONFIG_DIR;
}

void PathsConfiguration::ensure_directories_exist() const {
    try {
        fs::create_directories(get_save_directory());
        fs::create_directories(get_savestate_directory());
        fs::create_directories(get_screenshot_directory());
        fs::create_directories(get_config_directory());
    }
    catch (const std::exception& e) {
        std::cerr << "Error creating directories: " << e.what() << std::endl;
    }
}

std::string PathsConfiguration::get_display_path(const fs::path& path) const {
    // If it's relative or under the base directory, show relative form
    try {
        fs::path abs_path = fs::absolute(path);
        fs::path base_abs = fs::absolute(m_base_directory);

        // Check if path is under base directory
        auto rel = fs::relative(abs_path, base_abs);
        std::string rel_str = rel.string();

        // If the relative path doesn't start with "..", it's under base
        if (rel_str.find("..") != 0) {
            return rel_str;
        }
    }
    catch (...) {
        // Fall through to return absolute path
    }

    return path.string();
}

fs::path PathsConfiguration::get_battery_save_path(const fs::path& rom_path) const {
    // Get ROM filename without extension and add .sav
    fs::path save_dir = get_save_directory();
    std::string rom_stem = rom_path.stem().string();
    return save_dir / (rom_stem + ".sav");
}

fs::path PathsConfiguration::get_savestate_path(uint32_t rom_crc32, int slot) const {
    fs::path savestate_dir = get_savestate_directory();

    // Format: <CRC32>_slot<N>.state
    char filename[64];
    std::snprintf(filename, sizeof(filename), "%08X_slot%d.state", rom_crc32, slot);

    return savestate_dir / filename;
}

} // namespace emu
