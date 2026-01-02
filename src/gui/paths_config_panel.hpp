#pragma once

#include <string>
#include <filesystem>

namespace emu {

class Application;
class PathsConfiguration;

// GUI panel for configuring save paths
// Allows users to set directories for saves, savestates, screenshots, etc.
class PathsConfigPanel {
public:
    PathsConfigPanel();
    ~PathsConfigPanel();

    // Render the paths configuration panel as a tab
    // Returns true if changes were applied
    bool render(Application& app);

    // Reset panel state (call when opening settings)
    void reset();

private:
    // Render a path row with label, current value, and browse button
    bool render_path_row(const char* label, const char* id,
                         std::filesystem::path& path,
                         const std::filesystem::path& base_dir);

    // Open a native folder browser dialog
    // Returns selected path, or empty if cancelled
    std::filesystem::path browse_for_folder(const std::filesystem::path& start_path);

    // Current working values (copied from config on reset)
    std::filesystem::path m_save_directory;
    std::filesystem::path m_savestate_directory;
    std::filesystem::path m_screenshot_directory;
    std::filesystem::path m_rom_directory;

    // Track if values have been modified
    bool m_modified = false;

    // Track if we've loaded values from config
    bool m_initialized = false;

    // Input buffer for text editing
    static constexpr size_t PATH_BUFFER_SIZE = 512;
    char m_path_buffer[PATH_BUFFER_SIZE];
};

} // namespace emu
