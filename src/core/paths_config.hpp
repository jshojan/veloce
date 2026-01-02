#pragma once

#include <string>
#include <filesystem>

namespace emu {

// Centralized path configuration for the emulator platform.
// Manages all configurable directories: saves, savestates, screenshots, etc.
// Persists configuration to a JSON file in the config directory.
class PathsConfiguration {
public:
    PathsConfiguration();
    ~PathsConfiguration() = default;

    // Initialize with executable directory as base
    void initialize(const std::filesystem::path& exe_directory);

    // Load configuration from file (empty path uses default config path)
    bool load(const std::filesystem::path& config_path);
    bool load();

    // Save configuration to file (empty path uses default config path)
    bool save(const std::filesystem::path& config_path) const;
    bool save() const;

    // Get/set save directory (for battery saves: .sav files)
    std::filesystem::path get_save_directory() const;
    void set_save_directory(const std::filesystem::path& path);

    // Get/set savestate directory (for savestates: .state files)
    std::filesystem::path get_savestate_directory() const;
    void set_savestate_directory(const std::filesystem::path& path);

    // Get/set screenshot directory
    std::filesystem::path get_screenshot_directory() const;
    void set_screenshot_directory(const std::filesystem::path& path);

    // Get/set ROM directory (last used directory for ROM browser)
    std::filesystem::path get_rom_directory() const;
    void set_rom_directory(const std::filesystem::path& path);

    // Get the config directory (where this config file is stored)
    std::filesystem::path get_config_directory() const;

    // Get the base/executable directory
    std::filesystem::path get_base_directory() const { return m_base_directory; }

    // Reset all paths to defaults (relative to exe directory)
    void reset_to_defaults();

    // Check if configuration has been modified since load
    bool is_modified() const { return m_modified; }
    void clear_modified() { m_modified = false; }

    // Ensure all configured directories exist
    void ensure_directories_exist() const;

    // Get a path relative to base if possible, otherwise absolute
    std::string get_display_path(const std::filesystem::path& path) const;

    // Resolve a path (handles relative paths from base directory)
    std::filesystem::path resolve_path(const std::filesystem::path& path) const;

    // Build the full save file path for a ROM
    // Structure: <save_dir>/<rom_stem>.sav
    std::filesystem::path get_battery_save_path(const std::filesystem::path& rom_path) const;

    // Build the full savestate path for a ROM and slot
    // Structure: <savestate_dir>/<rom_crc32>_slot<N>.state
    std::filesystem::path get_savestate_path(uint32_t rom_crc32, int slot) const;

private:
    // Default subdirectory names
    static constexpr const char* DEFAULT_SAVE_DIR = "saves";
    static constexpr const char* DEFAULT_SAVESTATE_DIR = "savestates";
    static constexpr const char* DEFAULT_SCREENSHOT_DIR = "screenshots";
    static constexpr const char* DEFAULT_CONFIG_DIR = "config";
    static constexpr const char* CONFIG_FILENAME = "paths.json";

    // Base directory (where executable is located)
    std::filesystem::path m_base_directory;

    // Config file path
    std::filesystem::path m_config_path;

    // Configured directories (can be absolute or relative to base)
    std::filesystem::path m_save_directory;
    std::filesystem::path m_savestate_directory;
    std::filesystem::path m_screenshot_directory;
    std::filesystem::path m_rom_directory;

    bool m_modified = false;
    bool m_initialized = false;
};

} // namespace emu
