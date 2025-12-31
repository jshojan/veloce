#include "plugin_manager.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

// Platform-specific dynamic loading
#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
    #define LOAD_LIBRARY(path) LoadLibraryA(path)
    #define GET_PROC(handle, name) GetProcAddress((HMODULE)handle, name)
    #define CLOSE_LIBRARY(handle) FreeLibrary((HMODULE)handle)
    #define PLUGIN_EXTENSION ".dll"
#else
    #include <dlfcn.h>
    #define LOAD_LIBRARY(path) dlopen(path, RTLD_NOW)
    #define GET_PROC(handle, name) dlsym(handle, name)
    #define CLOSE_LIBRARY(handle) dlclose(handle)
    #ifdef __APPLE__
        #define PLUGIN_EXTENSION ".dylib"
    #else
        #define PLUGIN_EXTENSION ".so"
    #endif
#endif

namespace emu {

namespace fs = std::filesystem;

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() {
    shutdown();
}

bool PluginManager::initialize(const std::string& plugin_dir) {
    m_plugin_directory = plugin_dir;

    // Get executable directory
    fs::path exe_path = fs::current_path();

    // Try multiple search paths
    std::vector<fs::path> search_paths = {
        exe_path / plugin_dir,
        fs::path(plugin_dir),
        exe_path / "bin" / plugin_dir,
    };

    bool found_plugins = false;
    for (const auto& path : search_paths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            std::cout << "Scanning for plugins in: " << path << std::endl;
            if (scan_plugins(path.string())) {
                found_plugins = true;
            }
        }
    }

    if (m_plugins.empty()) {
        std::cout << "No plugins found" << std::endl;
    } else {
        std::cout << "Loaded " << m_plugins.size() << " plugin(s)" << std::endl;
    }

    return true;
}

void PluginManager::shutdown() {
    unload_rom();

    for (auto& plugin : m_plugins) {
        unload_plugin(plugin);
    }
    m_plugins.clear();
    m_active_plugin = nullptr;
    m_active_plugin_info = nullptr;
}

bool PluginManager::scan_plugins(const std::string& directory) {
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == PLUGIN_EXTENSION) {
                    load_plugin(entry.path().string());
                }
            }
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error scanning plugin directory: " << e.what() << std::endl;
        return false;
    }
}

bool PluginManager::load_plugin(const std::string& path) {
    std::cout << "Loading plugin: " << path << std::endl;

    // Load library
    void* handle = LOAD_LIBRARY(path.c_str());
    if (!handle) {
#ifdef _WIN32
        std::cerr << "Failed to load library: " << GetLastError() << std::endl;
#else
        std::cerr << "Failed to load library: " << dlerror() << std::endl;
#endif
        return false;
    }

    // Get function pointers
    auto create_func = reinterpret_cast<PluginInfo::CreateFunc>(
        GET_PROC(handle, "create_emulator_plugin"));
    auto destroy_func = reinterpret_cast<PluginInfo::DestroyFunc>(
        GET_PROC(handle, "destroy_emulator_plugin"));
    auto version_func = reinterpret_cast<PluginInfo::VersionFunc>(
        GET_PROC(handle, "get_plugin_api_version"));

    if (!create_func || !destroy_func) {
        std::cerr << "Plugin missing required functions" << std::endl;
        CLOSE_LIBRARY(handle);
        return false;
    }

    // Check API version
    if (version_func) {
        uint32_t version = version_func();
        if (version != EMU_PLUGIN_API_VERSION) {
            std::cerr << "Plugin API version mismatch: expected "
                      << EMU_PLUGIN_API_VERSION << ", got " << version << std::endl;
            CLOSE_LIBRARY(handle);
            return false;
        }
    }

    // Create plugin instance
    IEmulatorPlugin* instance = create_func();
    if (!instance) {
        std::cerr << "Failed to create plugin instance" << std::endl;
        CLOSE_LIBRARY(handle);
        return false;
    }

    // Get plugin info
    EmulatorInfo info = instance->get_info();

    // Check if a plugin with this name is already loaded
    for (const auto& existing : m_plugins) {
        if (existing.name == info.name) {
            std::cout << "Plugin '" << info.name << "' already loaded, skipping duplicate" << std::endl;
            destroy_func(instance);
            CLOSE_LIBRARY(handle);
            return false;
        }
    }

    PluginInfo plugin_info;
    plugin_info.path = path;
    plugin_info.name = info.name;
    plugin_info.version = info.version;
    plugin_info.handle = handle;
    plugin_info.instance = instance;
    plugin_info.create_func = create_func;
    plugin_info.destroy_func = destroy_func;

    // Copy extensions
    if (info.file_extensions) {
        for (const char** ext = info.file_extensions; *ext; ++ext) {
            plugin_info.extensions.push_back(*ext);
        }
    }

    m_plugins.push_back(std::move(plugin_info));

    std::cout << "Loaded plugin: " << info.name << " v" << info.version << std::endl;
    std::cout << "  Supported extensions: ";
    for (const auto& ext : m_plugins.back().extensions) {
        std::cout << ext << " ";
    }
    std::cout << std::endl;

    return true;
}

void PluginManager::unload_plugin(PluginInfo& plugin) {
    if (plugin.instance && plugin.destroy_func) {
        plugin.destroy_func(plugin.instance);
        plugin.instance = nullptr;
    }

    if (plugin.handle) {
        CLOSE_LIBRARY(plugin.handle);
        plugin.handle = nullptr;
    }
}

PluginInfo* PluginManager::find_plugin_for_extension(const std::string& extension) {
    std::string ext = extension;

    // Ensure extension starts with dot
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (auto& plugin : m_plugins) {
        for (const auto& supported_ext : plugin.extensions) {
            std::string lower_supported = supported_ext;
            std::transform(lower_supported.begin(), lower_supported.end(),
                          lower_supported.begin(), ::tolower);
            if (lower_supported == ext) {
                return &plugin;
            }
        }
    }

    return nullptr;
}

PluginInfo* PluginManager::find_plugin_by_name(const std::string& name) {
    for (auto& plugin : m_plugins) {
        if (plugin.name == name) {
            return &plugin;
        }
    }
    return nullptr;
}

bool PluginManager::set_active_plugin(const std::string& name) {
    PluginInfo* plugin = find_plugin_by_name(name);
    if (plugin && plugin->instance) {
        m_active_plugin = plugin->instance;
        m_active_plugin_info = plugin;
        return true;
    }
    return false;
}

bool PluginManager::set_active_plugin_for_file(const std::string& filepath) {
    std::string ext = get_file_extension(filepath);
    PluginInfo* plugin = find_plugin_for_extension(ext);
    if (plugin && plugin->instance) {
        m_active_plugin = plugin->instance;
        m_active_plugin_info = plugin;
        return true;
    }
    return false;
}

bool PluginManager::load_rom(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open ROM file: " << path << std::endl;
        return false;
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "Failed to read ROM file" << std::endl;
        return false;
    }

    return load_rom(data.data(), data.size());
}

bool PluginManager::load_rom(const uint8_t* data, size_t size) {
    if (!m_active_plugin) {
        std::cerr << "No active plugin" << std::endl;
        return false;
    }

    return m_active_plugin->load_rom(data, size);
}

void PluginManager::unload_rom() {
    if (m_active_plugin && m_active_plugin->is_rom_loaded()) {
        m_active_plugin->unload_rom();
    }
}

bool PluginManager::is_rom_loaded() const {
    return m_active_plugin && m_active_plugin->is_rom_loaded();
}

uint32_t PluginManager::get_rom_crc32() const {
    if (m_active_plugin && m_active_plugin->is_rom_loaded()) {
        return m_active_plugin->get_rom_crc32();
    }
    return 0;
}

std::string PluginManager::get_file_extension(const std::string& path) {
    size_t dot_pos = path.rfind('.');
    if (dot_pos != std::string::npos) {
        return path.substr(dot_pos);
    }
    return "";
}

} // namespace emu
