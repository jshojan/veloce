#include "plugin_registry.hpp"
#include "emu/emulator_plugin.hpp"
#include "emu/video_plugin.hpp"
#include "emu/audio_plugin.hpp"
#include "emu/input_plugin.hpp"
#include "emu/tas_plugin.hpp"
#include "emu/game_plugin.hpp"
#include "emu/netplay_plugin.hpp"

#include <iostream>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace emu {

PluginRegistry::PluginRegistry() = default;

PluginRegistry::~PluginRegistry() {
    unload_all();
}

const char* PluginRegistry::get_library_extension() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

void* PluginRegistry::load_library(const std::filesystem::path& path) {
#ifdef _WIN32
    return LoadLibraryW(path.wstring().c_str());
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void PluginRegistry::unload_library(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* PluginRegistry::get_symbol(void* handle, const char* symbol_name) {
    if (!handle) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbol_name));
#else
    return dlsym(handle, symbol_name);
#endif
}

bool PluginRegistry::probe_plugin(const std::filesystem::path& path, PluginMetadata& metadata) {
    // Try to load the library
    void* handle = load_library(path);
    if (!handle) {
#ifndef _WIN32
        std::cerr << "Failed to load plugin " << path << ": " << dlerror() << std::endl;
#endif
        return false;
    }

    // Every plugin must export get_plugin_type()
    using GetPluginTypeFunc = PluginType (*)();
    auto get_type = reinterpret_cast<GetPluginTypeFunc>(get_symbol(handle, "get_plugin_type"));

    if (!get_type) {
        // Fallback: try to detect plugin type by checking for type-specific functions
        // This maintains compatibility with existing plugins that don't export get_plugin_type

        // Check for emulator plugin
        if (get_symbol(handle, "create_emulator_plugin")) {
            metadata.type = PluginType::Emulator;

            // Get emulator-specific info
            using CreateFunc = IEmulatorPlugin* (*)();
            using DestroyFunc = void (*)(IEmulatorPlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_emulator_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_emulator_plugin"));

            if (create && destroy) {
                IEmulatorPlugin* plugin = create();
                if (plugin) {
                    EmulatorInfo info = plugin->get_info();
                    metadata.name = info.name ? info.name : "";
                    metadata.version = info.version ? info.version : "";
                    metadata.author = info.author ? info.author : "";
                    metadata.description = info.description ? info.description : "";

                    // Get file extensions
                    if (info.file_extensions) {
                        for (const char** ext = info.file_extensions; *ext; ++ext) {
                            metadata.file_extensions.push_back(*ext);
                        }
                    }

                    destroy(plugin);
                }
            }

            // Get API version
            using VersionFunc = uint32_t (*)();
            auto get_version = reinterpret_cast<VersionFunc>(get_symbol(handle, "get_plugin_api_version"));
            metadata.api_version = get_version ? get_version() : 0;
        }
        // Check for video plugin
        else if (get_symbol(handle, "create_video_plugin")) {
            metadata.type = PluginType::Video;

            using CreateFunc = IVideoPlugin* (*)();
            using DestroyFunc = void (*)(IVideoPlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_video_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_video_plugin"));

            if (create && destroy) {
                IVideoPlugin* plugin = create();
                if (plugin) {
                    VideoPluginInfo info = plugin->get_info();
                    metadata.name = info.name ? info.name : "";
                    metadata.version = info.version ? info.version : "";
                    metadata.description = info.description ? info.description : "";
                    metadata.author = info.author ? info.author : "";
                    metadata.capabilities = info.capabilities;
                    destroy(plugin);
                }
            }

            using VersionFunc = uint32_t (*)();
            auto get_version = reinterpret_cast<VersionFunc>(get_symbol(handle, "get_video_plugin_api_version"));
            metadata.api_version = get_version ? get_version() : 0;
        }
        // Check for audio plugin
        else if (get_symbol(handle, "create_audio_plugin")) {
            metadata.type = PluginType::Audio;

            using CreateFunc = IAudioPlugin* (*)();
            using DestroyFunc = void (*)(IAudioPlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_audio_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_audio_plugin"));

            if (create && destroy) {
                IAudioPlugin* plugin = create();
                if (plugin) {
                    AudioPluginInfo info = plugin->get_info();
                    metadata.name = info.name ? info.name : "";
                    metadata.version = info.version ? info.version : "";
                    metadata.author = info.author ? info.author : "";
                    metadata.description = info.description ? info.description : "";
                    metadata.capabilities = 0;
                    if (info.supports_recording) metadata.capabilities |= AudioCapabilities::Recording;
                    if (info.supports_effects) metadata.capabilities |= AudioCapabilities::Effects;
                    destroy(plugin);
                }
            }

            using VersionFunc = uint32_t (*)();
            auto get_version = reinterpret_cast<VersionFunc>(get_symbol(handle, "get_audio_plugin_api_version"));
            metadata.api_version = get_version ? get_version() : 0;
        }
        // Check for input plugin
        else if (get_symbol(handle, "create_input_plugin")) {
            metadata.type = PluginType::Input;

            using CreateFunc = IInputPlugin* (*)();
            using DestroyFunc = void (*)(IInputPlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_input_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_input_plugin"));

            if (create && destroy) {
                IInputPlugin* plugin = create();
                if (plugin) {
                    InputPluginInfo info = plugin->get_info();
                    metadata.name = info.name ? info.name : "";
                    metadata.version = info.version ? info.version : "";
                    metadata.author = info.author ? info.author : "";
                    metadata.description = info.description ? info.description : "";
                    metadata.capabilities = 0;
                    if (info.supports_recording) metadata.capabilities |= InputCapabilities::Recording;
                    if (info.supports_playback) metadata.capabilities |= InputCapabilities::Playback;
                    if (info.supports_turbo) metadata.capabilities |= InputCapabilities::Turbo;
                    destroy(plugin);
                }
            }

            using VersionFunc = uint32_t (*)();
            auto get_version = reinterpret_cast<VersionFunc>(get_symbol(handle, "get_input_plugin_api_version"));
            metadata.api_version = get_version ? get_version() : 0;
        }
        // Check for TAS plugin
        else if (get_symbol(handle, "create_tas_plugin")) {
            metadata.type = PluginType::TAS;

            using CreateFunc = ITASPlugin* (*)();
            using DestroyFunc = void (*)(ITASPlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_tas_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_tas_plugin"));

            if (create && destroy) {
                ITASPlugin* plugin = create();
                if (plugin) {
                    TASPluginInfo info = plugin->get_info();
                    metadata.name = info.name ? info.name : "";
                    metadata.version = info.version ? info.version : "";
                    metadata.author = info.author ? info.author : "";
                    metadata.description = info.description ? info.description : "";
                    destroy(plugin);
                }
            }

            using VersionFunc = uint32_t (*)();
            auto get_version = reinterpret_cast<VersionFunc>(get_symbol(handle, "get_tas_plugin_api_version"));
            metadata.api_version = get_version ? get_version() : 0;
        }
        // Check for game plugin (unified interface)
        else if (get_symbol(handle, "create_game_plugin")) {
            metadata.type = PluginType::Game;

            using CreateFunc = IGamePlugin* (*)();
            using DestroyFunc = void (*)(IGamePlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_game_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_game_plugin"));

            if (create && destroy) {
                IGamePlugin* plugin = create();
                if (plugin) {
                    GamePluginInfo info = plugin->get_info();
                    metadata.name = info.name ? info.name : "";
                    metadata.version = info.version ? info.version : "";
                    metadata.description = info.description ? info.description : "";
                    metadata.author = info.author ? info.author : "";
                    metadata.capabilities = info.capabilities;

                    // Store supported ROM CRCs
                    if (info.game_crc32 != 0) {
                        metadata.supported_roms.push_back(info.game_crc32);
                    }
                    if (info.alt_crc32s && info.alt_crc32_count > 0) {
                        for (int i = 0; i < info.alt_crc32_count; ++i) {
                            metadata.supported_roms.push_back(info.alt_crc32s[i]);
                        }
                    }

                    destroy(plugin);
                }
            }

            using VersionFunc = uint32_t (*)();
            auto get_version = reinterpret_cast<VersionFunc>(get_symbol(handle, "get_game_plugin_api_version"));
            metadata.api_version = get_version ? get_version() : 0;
        }
        // Check for netplay plugin
        else if (get_symbol(handle, "create_netplay_plugin")) {
            metadata.type = PluginType::Netplay;

            using CreateFunc = INetplayPlugin* (*)();
            using DestroyFunc = void (*)(INetplayPlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_netplay_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_netplay_plugin"));

            if (create && destroy) {
                INetplayPlugin* plugin = create();
                if (plugin) {
                    NetplayPluginInfo info = plugin->get_info();
                    metadata.name = info.name ? info.name : "";
                    metadata.version = info.version ? info.version : "";
                    metadata.description = info.description ? info.description : "";
                    metadata.author = info.author ? info.author : "";
                    metadata.capabilities = info.capabilities;
                    destroy(plugin);
                }
            }

            using VersionFunc = uint32_t (*)();
            auto get_version = reinterpret_cast<VersionFunc>(get_symbol(handle, "get_netplay_plugin_api_version"));
            metadata.api_version = get_version ? get_version() : 0;
        }
        else {
            // Unknown plugin type
            unload_library(handle);
            return false;
        }
    }
    else {
        // Plugin exports get_plugin_type - use the new interface
        metadata.type = get_type();

        // Get base plugin info
        using GetInfoFunc = BasePluginInfo (*)();
        auto get_info = reinterpret_cast<GetInfoFunc>(get_symbol(handle, "get_plugin_info"));

        if (get_info) {
            BasePluginInfo info = get_info();
            metadata.name = info.name ? info.name : "";
            metadata.version = info.version ? info.version : "";
            metadata.author = info.author ? info.author : "";
            metadata.description = info.description ? info.description : "";
            metadata.capabilities = info.capabilities;
        }

        // For game plugins, also get ROM-specific info
        if (metadata.type == PluginType::Game) {
            using CreateFunc = IGamePlugin* (*)();
            using DestroyFunc = void (*)(IGamePlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_game_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_game_plugin"));

            if (create && destroy) {
                IGamePlugin* plugin = create();
                if (plugin) {
                    GamePluginInfo info = plugin->get_info();

                    // Store supported ROM CRCs
                    if (info.game_crc32 != 0) {
                        metadata.supported_roms.push_back(info.game_crc32);
                    }
                    if (info.alt_crc32s && info.alt_crc32_count > 0) {
                        for (int i = 0; i < info.alt_crc32_count; ++i) {
                            metadata.supported_roms.push_back(info.alt_crc32s[i]);
                        }
                    }

                    destroy(plugin);
                }
            }
        }

        // For emulator plugins, also get file extensions
        if (metadata.type == PluginType::Emulator) {
            using CreateFunc = IEmulatorPlugin* (*)();
            using DestroyFunc = void (*)(IEmulatorPlugin*);
            auto create = reinterpret_cast<CreateFunc>(get_symbol(handle, "create_emulator_plugin"));
            auto destroy = reinterpret_cast<DestroyFunc>(get_symbol(handle, "destroy_emulator_plugin"));

            if (create && destroy) {
                IEmulatorPlugin* plugin = create();
                if (plugin) {
                    EmulatorInfo info = plugin->get_info();

                    // Get file extensions
                    if (info.file_extensions) {
                        for (const char** ext = info.file_extensions; *ext; ++ext) {
                            metadata.file_extensions.push_back(*ext);
                        }
                    }

                    destroy(plugin);
                }
            }
        }
    }

    metadata.path = path;

    // Unload library after probing
    unload_library(handle);
    return true;
}

bool PluginRegistry::scan_directory(const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return false;
    }

    // Track this directory for refresh
    auto it = std::find(m_plugin_directories.begin(), m_plugin_directories.end(), directory);
    if (it == m_plugin_directories.end()) {
        m_plugin_directories.push_back(directory);
    }

    const char* lib_ext = get_library_extension();
    int found_count = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;

        const auto& path = entry.path();
        if (path.extension() != lib_ext) continue;

        // Check if we already have this plugin
        bool already_registered = false;
        for (const auto& existing : m_plugins) {
            if (existing.path == path) {
                already_registered = true;
                break;
            }
        }
        if (already_registered) continue;

        // Probe the plugin
        PluginMetadata metadata;
        if (probe_plugin(path, metadata)) {
            m_plugins.push_back(std::move(metadata));
            ++found_count;
            std::cout << "Found " << plugin_type_to_string(m_plugins.back().type)
                      << " plugin: " << m_plugins.back().name
                      << " (" << path.filename() << ")" << std::endl;
        }
    }

    return found_count > 0;
}

std::vector<PluginMetadata> PluginRegistry::get_plugins_of_type(PluginType type) const {
    std::vector<PluginMetadata> result;
    for (const auto& plugin : m_plugins) {
        if (plugin.type == type) {
            result.push_back(plugin);
        }
    }
    return result;
}

const PluginMetadata* PluginRegistry::find_plugin(PluginType type, const std::string& name) const {
    for (const auto& plugin : m_plugins) {
        if (plugin.type == type && plugin.name == name) {
            return &plugin;
        }
    }
    return nullptr;
}

std::vector<PluginMetadata> PluginRegistry::find_plugins_for_extension(const std::string& extension) const {
    std::vector<PluginMetadata> result;

    std::string ext = extension;
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }

    for (const auto& plugin : m_plugins) {
        if (plugin.type != PluginType::Emulator) continue;

        for (const auto& supported_ext : plugin.file_extensions) {
            if (supported_ext == ext) {
                result.push_back(plugin);
                break;
            }
        }
    }

    return result;
}

std::vector<PluginMetadata> PluginRegistry::find_game_plugins_for_rom(uint32_t crc32) const {
    std::vector<PluginMetadata> result;

    for (const auto& plugin : m_plugins) {
        if (plugin.type != PluginType::Game) continue;

        // Empty supported_roms means universal plugin
        if (plugin.supported_roms.empty()) {
            result.push_back(plugin);
            continue;
        }

        for (uint32_t supported_crc : plugin.supported_roms) {
            if (supported_crc == crc32) {
                result.push_back(plugin);
                break;
            }
        }
    }

    return result;
}

PluginHandle* PluginRegistry::load_plugin(const PluginMetadata& metadata) {
    // Check if already loaded
    for (auto& handle_ptr : m_loaded_plugins) {
        if (handle_ptr->path == metadata.path) {
            return handle_ptr.get();
        }
    }

    // Load the library
    void* lib_handle = load_library(metadata.path);
    if (!lib_handle) {
#ifndef _WIN32
        std::cerr << "Failed to load plugin: " << dlerror() << std::endl;
#endif
        return nullptr;
    }

    auto handle = std::make_unique<PluginHandle>();
    handle->library_handle = lib_handle;
    handle->path = metadata.path;
    handle->metadata = metadata;

    // Get create/destroy functions based on type
    switch (metadata.type) {
        case PluginType::Emulator:
            handle->create_func = get_symbol(lib_handle, "create_emulator_plugin");
            handle->destroy_func = get_symbol(lib_handle, "destroy_emulator_plugin");
            break;
        case PluginType::Video:
            handle->create_func = get_symbol(lib_handle, "create_video_plugin");
            handle->destroy_func = get_symbol(lib_handle, "destroy_video_plugin");
            break;
        case PluginType::Audio:
            handle->create_func = get_symbol(lib_handle, "create_audio_plugin");
            handle->destroy_func = get_symbol(lib_handle, "destroy_audio_plugin");
            break;
        case PluginType::Input:
            handle->create_func = get_symbol(lib_handle, "create_input_plugin");
            handle->destroy_func = get_symbol(lib_handle, "destroy_input_plugin");
            break;
        case PluginType::TAS:
            handle->create_func = get_symbol(lib_handle, "create_tas_plugin");
            handle->destroy_func = get_symbol(lib_handle, "destroy_tas_plugin");
            break;
        case PluginType::Game:
            handle->create_func = get_symbol(lib_handle, "create_game_plugin");
            handle->destroy_func = get_symbol(lib_handle, "destroy_game_plugin");
            break;
        case PluginType::Netplay:
            handle->create_func = get_symbol(lib_handle, "create_netplay_plugin");
            handle->destroy_func = get_symbol(lib_handle, "destroy_netplay_plugin");
            break;
    }

    if (!handle->create_func || !handle->destroy_func) {
        unload_library(lib_handle);
        return nullptr;
    }

    PluginHandle* result = handle.get();
    m_loaded_plugins.push_back(std::move(handle));
    return result;
}

void PluginRegistry::unload_plugin(PluginHandle* handle) {
    if (!handle) return;

    for (auto it = m_loaded_plugins.begin(); it != m_loaded_plugins.end(); ++it) {
        if (it->get() == handle) {
            unload_library((*it)->library_handle);
            m_loaded_plugins.erase(it);
            return;
        }
    }
}

void PluginRegistry::unload_all() {
    for (auto& handle_ptr : m_loaded_plugins) {
        unload_library(handle_ptr->library_handle);
    }
    m_loaded_plugins.clear();
}

PluginHandle* PluginRegistry::find_loaded_plugin(const std::filesystem::path& path) {
    for (auto& handle_ptr : m_loaded_plugins) {
        if (handle_ptr->path == path) {
            return handle_ptr.get();
        }
    }
    return nullptr;
}

void PluginRegistry::refresh() {
    // Clear existing plugin list (but keep loaded plugins)
    m_plugins.clear();

    // Rescan all directories
    for (const auto& dir : m_plugin_directories) {
        scan_directory(dir);
    }
}

void PluginRegistry::add_plugin_directory(const std::filesystem::path& directory) {
    auto it = std::find(m_plugin_directories.begin(), m_plugin_directories.end(), directory);
    if (it == m_plugin_directories.end()) {
        m_plugin_directories.push_back(directory);
    }
}

} // namespace emu
