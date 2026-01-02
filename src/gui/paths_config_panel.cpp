#include "paths_config_panel.hpp"
#include "core/application.hpp"
#include "core/paths_config.hpp"

#include <imgui.h>
#include <cstring>
#include <iostream>

// Platform-specific includes for folder browser
#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#endif

namespace emu {

namespace fs = std::filesystem;

PathsConfigPanel::PathsConfigPanel() {
    std::memset(m_path_buffer, 0, sizeof(m_path_buffer));
}

PathsConfigPanel::~PathsConfigPanel() = default;

void PathsConfigPanel::reset() {
    m_initialized = false;
    m_modified = false;
}

bool PathsConfigPanel::render(Application& app) {
    auto& paths_config = app.get_paths_config();

    // Load current values on first render
    if (!m_initialized) {
        m_save_directory = paths_config.get_save_directory();
        m_savestate_directory = paths_config.get_savestate_directory();
        m_screenshot_directory = paths_config.get_screenshot_directory();
        m_rom_directory = paths_config.get_rom_directory();
        m_initialized = true;
    }

    fs::path base_dir = paths_config.get_base_directory();

    ImGui::TextWrapped("Configure directories for save files, savestates, and other data. "
                       "Paths can be absolute or relative to the executable directory.");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Save Directory (battery saves)
    ImGui::Text("Battery Saves");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       "SRAM and cartridge save files (.sav)");
    if (render_path_row("##save_dir", "save", m_save_directory, base_dir)) {
        m_modified = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Savestate Directory
    ImGui::Text("Savestates");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       "Emulator savestate files (.state)");
    if (render_path_row("##savestate_dir", "savestate", m_savestate_directory, base_dir)) {
        m_modified = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Screenshot Directory
    ImGui::Text("Screenshots");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       "Screenshot image files (.png)");
    if (render_path_row("##screenshot_dir", "screenshot", m_screenshot_directory, base_dir)) {
        m_modified = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ROM Directory (last used for browser)
    ImGui::Text("ROMs");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                       "Default directory for ROM browser");
    if (render_path_row("##rom_dir", "rom", m_rom_directory, base_dir)) {
        m_modified = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons
    float button_width = 100.0f;

    if (ImGui::Button("Reset Defaults", ImVec2(button_width, 0))) {
        paths_config.reset_to_defaults();
        m_save_directory = paths_config.get_save_directory();
        m_savestate_directory = paths_config.get_savestate_directory();
        m_screenshot_directory = paths_config.get_screenshot_directory();
        m_rom_directory = paths_config.get_rom_directory();
        m_modified = true;
    }

    ImGui::SameLine();

    // Right-align the apply button
    float avail_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_width - button_width);

    bool applied = false;
    if (m_modified) {
        if (ImGui::Button("Apply", ImVec2(button_width, 0))) {
            // Apply changes to configuration
            paths_config.set_save_directory(m_save_directory);
            paths_config.set_savestate_directory(m_savestate_directory);
            paths_config.set_screenshot_directory(m_screenshot_directory);
            paths_config.set_rom_directory(m_rom_directory);

            // Ensure directories exist
            paths_config.ensure_directories_exist();

            // Save to file
            paths_config.save();

            m_modified = false;
            applied = true;

            std::cout << "Paths configuration saved" << std::endl;
        }
    } else {
        ImGui::BeginDisabled();
        ImGui::Button("Apply", ImVec2(button_width, 0));
        ImGui::EndDisabled();
    }

    return applied;
}

bool PathsConfigPanel::render_path_row(const char* label, const char* id,
                                        fs::path& path,
                                        const fs::path& base_dir) {
    bool changed = false;

    // Copy path to buffer for editing
    std::string path_str = path.string();
    std::strncpy(m_path_buffer, path_str.c_str(), PATH_BUFFER_SIZE - 1);
    m_path_buffer[PATH_BUFFER_SIZE - 1] = '\0';

    // Calculate widths
    float browse_button_width = 75.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float input_width = ImGui::GetContentRegionAvail().x - browse_button_width - spacing;

    // Text input for path
    ImGui::SetNextItemWidth(input_width);
    char input_id[64];
    std::snprintf(input_id, sizeof(input_id), "##path_%s", id);

    if (ImGui::InputText(input_id, m_path_buffer, PATH_BUFFER_SIZE,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        path = m_path_buffer;
        changed = true;
    }

    // Check if input was modified (without pressing Enter)
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        path = m_path_buffer;
        changed = true;
    }

    ImGui::SameLine();

    // Browse button
    char browse_id[64];
    std::snprintf(browse_id, sizeof(browse_id), "Browse##%s", id);

    if (ImGui::Button(browse_id, ImVec2(browse_button_width, 0))) {
        // Start from current path if it exists, otherwise from base directory
        fs::path start_path = path;
        if (!start_path.is_absolute()) {
            start_path = base_dir / start_path;
        }
        if (!fs::exists(start_path)) {
            start_path = base_dir;
        }

        fs::path selected = browse_for_folder(start_path);
        if (!selected.empty()) {
            // Try to make path relative to base directory
            try {
                fs::path relative = fs::relative(selected, base_dir);
                // Only use relative if it doesn't go outside base directory
                if (relative.string().find("..") != 0) {
                    path = relative;
                } else {
                    path = selected;
                }
            } catch (...) {
                path = selected;
            }
            changed = true;
        }
    }

    // Show resolved path as tooltip if relative
    if (!path.is_absolute()) {
        fs::path resolved = base_dir / path;
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Resolves to: %s", resolved.string().c_str());
        }
    }

    return changed;
}

fs::path PathsConfigPanel::browse_for_folder(const fs::path& start_path) {
    // Platform-specific folder browser implementation

#ifdef _WIN32
    // Windows implementation using SHBrowseForFolder
    BROWSEINFOW bi = {};
    bi.lpszTitle = L"Select Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            CoTaskMemFree(pidl);
            // Convert wide string to string
            char narrow_path[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow_path, MAX_PATH, nullptr, nullptr);
            return fs::path(narrow_path);
        }
        CoTaskMemFree(pidl);
    }
    return {};

#elif defined(__APPLE__)
    // macOS - would use NSOpenPanel, but that requires Objective-C
    // For now, return empty (user can type path manually)
    (void)start_path;
    std::cout << "Folder browser not implemented on macOS. Please type the path manually." << std::endl;
    return {};

#else
    // Linux - try to use zenity if available
    std::string cmd = "zenity --file-selection --directory";
    if (!start_path.empty() && fs::exists(start_path)) {
        cmd += " --filename=\"" + start_path.string() + "/\"";
    }
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[512];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        int status = pclose(pipe);

        if (status == 0 && !result.empty()) {
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            return fs::path(result);
        }
    }

    // Fallback: try kdialog
    cmd = "kdialog --getexistingdirectory";
    if (!start_path.empty() && fs::exists(start_path)) {
        cmd += " \"" + start_path.string() + "\"";
    }
    cmd += " 2>/dev/null";

    pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[512];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        int status = pclose(pipe);

        if (status == 0 && !result.empty()) {
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            return fs::path(result);
        }
    }

    std::cout << "Folder browser requires zenity or kdialog. Please type the path manually." << std::endl;
    return {};
#endif
}

} // namespace emu
