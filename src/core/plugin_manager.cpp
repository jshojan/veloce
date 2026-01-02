#include "plugin_manager.hpp"
#include "paths_config.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

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

    // Search paths for emulator cores (cores/ directory)
    // These contain platform emulators: NES, GBA, SNES, GB, etc.
    std::vector<fs::path> core_search_paths = {
        exe_path / "cores",
        exe_path / ".." / "cores",
        exe_path / "build" / "bin" / "cores",
        exe_path / "bin" / "cores",
    };

    // Search paths for other plugins (plugins/ directory)
    // These contain: audio, input, TAS, speedrun tools, game-specific plugins
    std::vector<fs::path> plugin_search_paths = {
        exe_path / "plugins",
        exe_path / ".." / "plugins",
        exe_path / "build" / "bin" / "plugins",
        exe_path / "build" / plugin_dir,
        exe_path / plugin_dir,
        exe_path / ".." / plugin_dir,
        fs::path(plugin_dir),
        exe_path / "bin" / "plugins",
        exe_path / "bin" / plugin_dir,
    };

    bool found_plugins = false;

    // Scan cores directory first (emulator cores)
    for (const auto& path : core_search_paths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            std::cout << "Scanning for emulator cores in: " << path << std::endl;
            if (m_registry.scan_directory(path)) {
                found_plugins = true;
            }
        }
    }

    // Scan plugins directory (other plugins)
    for (const auto& path : plugin_search_paths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            std::cout << "Scanning for plugins in: " << path << std::endl;
            if (m_registry.scan_directory(path)) {
                found_plugins = true;
            }
        }
    }

    const auto& plugins = m_registry.get_all_plugins();
    if (plugins.empty()) {
        std::cout << "No plugins found" << std::endl;
    } else {
        std::cout << "Found " << plugins.size() << " plugin(s)" << std::endl;

        // Print summary by type
        for (int i = 0; i <= static_cast<int>(PluginType::Netplay); ++i) {
            PluginType type = static_cast<PluginType>(i);
            auto type_plugins = m_registry.get_plugins_of_type(type);
            if (!type_plugins.empty()) {
                std::cout << "  " << plugin_type_to_string(type) << ": ";
                for (size_t j = 0; j < type_plugins.size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << type_plugins[j].name;
                }
                std::cout << std::endl;
            }
        }
    }

    // Build legacy plugin list for backward compatibility
    build_legacy_plugin_list();

    // Activate default/configured plugins
    // Only activate emulator plugin by default; others can be activated later
    auto emulator_plugins = m_registry.get_plugins_of_type(PluginType::Emulator);
    if (!emulator_plugins.empty()) {
        std::string selected = m_config.get_selected_plugin(PluginType::Emulator);
        if (selected.empty() || !activate_emulator_plugin(selected)) {
            // Use first available
            activate_emulator_plugin(emulator_plugins[0].name);
        }
    }

    return true;
}

void PluginManager::shutdown() {
    // Prevent double-shutdown (called from both Application::shutdown() and destructor)
    if (m_shutdown_called) return;
    m_shutdown_called = true;

    unload_rom();

    // Deactivate all plugins
    deactivate_plugin(PluginType::Netplay);
    deactivate_plugin(PluginType::Game);
    deactivate_plugin(PluginType::SpeedrunTools);
    deactivate_plugin(PluginType::TAS);
    deactivate_plugin(PluginType::Input);
    deactivate_plugin(PluginType::Audio);
    deactivate_plugin(PluginType::Video);
    deactivate_plugin(PluginType::Emulator);

    // Clear legacy list
    m_legacy_plugins.clear();
    m_active_plugin_info = nullptr;

    // Unload all libraries
    m_registry.unload_all();
}

bool PluginManager::load_config(const std::string& path) {
    return m_config.load(path);
}

bool PluginManager::save_config(const std::string& path) {
    if (path.empty()) {
        return m_config.save();
    }
    return m_config.save(path);
}

bool PluginManager::set_active_plugin(PluginType type, const std::string& name) {
    switch (type) {
        case PluginType::Emulator:      return activate_emulator_plugin(name);
        case PluginType::Video:         return activate_video_plugin(name);
        case PluginType::Audio:         return activate_audio_plugin(name);
        case PluginType::Input:         return activate_input_plugin(name);
        case PluginType::TAS:           return activate_tas_plugin(name);
        case PluginType::SpeedrunTools: return activate_speedrun_tools_plugin(name);
        case PluginType::Game:          return activate_game_plugin(name);
        case PluginType::Netplay:       return activate_netplay_plugin(name);
        default: return false;
    }
}

bool PluginManager::activate_game_plugin_for_rom(uint32_t crc32) {
    // Deactivate current game plugin
    deactivate_plugin(PluginType::Game);

    // Find matching game plugins
    auto matching = m_registry.find_game_plugins_for_rom(crc32);
    if (matching.empty()) {
        return false;
    }

    // Use the first matching plugin
    return activate_game_plugin(matching[0].name);
}

std::vector<std::string> PluginManager::get_available_plugins(PluginType type) const {
    std::vector<std::string> names;
    auto plugins = m_registry.get_plugins_of_type(type);
    names.reserve(plugins.size());
    for (const auto& plugin : plugins) {
        names.push_back(plugin.name);
    }
    return names;
}

std::string PluginManager::get_selected_plugin_name(PluginType type) const {
    return m_config.get_selected_plugin(type);
}

void PluginManager::on_plugin_changed(PluginChangedCallback callback) {
    m_change_callbacks.push_back(std::move(callback));
}

// Plugin activation implementations

bool PluginManager::activate_emulator_plugin(const std::string& name) {
    // Find the plugin metadata
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::Emulator, name);
    if (!metadata) {
        std::cerr << "Emulator plugin not found: " << name << std::endl;
        return false;
    }

    // Load the plugin
    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load emulator plugin: " << name << std::endl;
        return false;
    }

    // Create instance
    using CreateFunc = IEmulatorPlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    IEmulatorPlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create emulator plugin instance: " << name << std::endl;
        return false;
    }

    // Deactivate old plugin
    deactivate_plugin(PluginType::Emulator);

    // Set new plugin
    m_active.emulator = instance;
    m_active.emulator_handle = handle;
    m_config.set_selected_plugin(PluginType::Emulator, name);

    // Update legacy list
    build_legacy_plugin_list();

    notify_plugin_changed(PluginType::Emulator, name);
    std::cout << "Activated emulator plugin: " << name << std::endl;
    return true;
}

bool PluginManager::activate_video_plugin(const std::string& name) {
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::Video, name);
    if (!metadata) {
        std::cerr << "Video plugin not found: " << name << std::endl;
        return false;
    }

    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load video plugin: " << name << std::endl;
        return false;
    }

    using CreateFunc = IVideoPlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    IVideoPlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create video plugin instance: " << name << std::endl;
        return false;
    }

    deactivate_plugin(PluginType::Video);

    m_active.video = instance;
    m_active.video_handle = handle;
    m_config.set_selected_plugin(PluginType::Video, name);

    notify_plugin_changed(PluginType::Video, name);
    std::cout << "Activated video plugin: " << name << std::endl;
    return true;
}

bool PluginManager::activate_audio_plugin(const std::string& name) {
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::Audio, name);
    if (!metadata) {
        std::cerr << "Audio plugin not found: " << name << std::endl;
        return false;
    }

    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load audio plugin: " << name << std::endl;
        return false;
    }

    using CreateFunc = IAudioPlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    IAudioPlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create audio plugin instance: " << name << std::endl;
        return false;
    }

    deactivate_plugin(PluginType::Audio);

    m_active.audio = instance;
    m_active.audio_handle = handle;
    m_config.set_selected_plugin(PluginType::Audio, name);

    notify_plugin_changed(PluginType::Audio, name);
    std::cout << "Activated audio plugin: " << name << std::endl;
    return true;
}

bool PluginManager::activate_input_plugin(const std::string& name) {
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::Input, name);
    if (!metadata) {
        std::cerr << "Input plugin not found: " << name << std::endl;
        return false;
    }

    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load input plugin: " << name << std::endl;
        return false;
    }

    using CreateFunc = IInputPlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    IInputPlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create input plugin instance: " << name << std::endl;
        return false;
    }

    deactivate_plugin(PluginType::Input);

    m_active.input = instance;
    m_active.input_handle = handle;
    m_config.set_selected_plugin(PluginType::Input, name);

    notify_plugin_changed(PluginType::Input, name);
    std::cout << "Activated input plugin: " << name << std::endl;
    return true;
}

bool PluginManager::activate_tas_plugin(const std::string& name) {
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::TAS, name);
    if (!metadata) {
        std::cerr << "TAS plugin not found: " << name << std::endl;
        return false;
    }

    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load TAS plugin: " << name << std::endl;
        return false;
    }

    using CreateFunc = ITASPlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    ITASPlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create TAS plugin instance: " << name << std::endl;
        return false;
    }

    deactivate_plugin(PluginType::TAS);

    m_active.tas = instance;
    m_active.tas_handle = handle;
    m_config.set_selected_plugin(PluginType::TAS, name);

    notify_plugin_changed(PluginType::TAS, name);
    std::cout << "Activated TAS plugin: " << name << std::endl;
    return true;
}

bool PluginManager::activate_speedrun_tools_plugin(const std::string& name) {
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::SpeedrunTools, name);
    if (!metadata) {
        std::cerr << "SpeedrunTools plugin not found: " << name << std::endl;
        return false;
    }

    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load SpeedrunTools plugin: " << name << std::endl;
        return false;
    }

    using CreateFunc = ISpeedrunToolsPlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    ISpeedrunToolsPlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create SpeedrunTools plugin instance: " << name << std::endl;
        return false;
    }

    deactivate_plugin(PluginType::SpeedrunTools);

    m_active.speedrun_tools = instance;
    m_active.speedrun_tools_handle = handle;
    m_config.set_selected_plugin(PluginType::SpeedrunTools, name);

    notify_plugin_changed(PluginType::SpeedrunTools, name);
    std::cout << "Activated SpeedrunTools plugin: " << name << std::endl;
    return true;
}

bool PluginManager::activate_game_plugin(const std::string& name) {
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::Game, name);
    if (!metadata) {
        std::cerr << "Game plugin not found: " << name << std::endl;
        return false;
    }

    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load game plugin: " << name << std::endl;
        return false;
    }

    using CreateFunc = IGamePlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    IGamePlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create game plugin instance: " << name << std::endl;
        return false;
    }

    deactivate_plugin(PluginType::Game);

    m_active.game = instance;
    m_active.game_handle = handle;
    m_config.set_selected_plugin(PluginType::Game, name);

    notify_plugin_changed(PluginType::Game, name);
    std::cout << "Activated game plugin: " << name << std::endl;
    return true;
}

bool PluginManager::activate_netplay_plugin(const std::string& name) {
    const PluginMetadata* metadata = m_registry.find_plugin(PluginType::Netplay, name);
    if (!metadata) {
        std::cerr << "Netplay plugin not found: " << name << std::endl;
        return false;
    }

    PluginHandle* handle = m_registry.load_plugin(*metadata);
    if (!handle || !handle->create_func) {
        std::cerr << "Failed to load netplay plugin: " << name << std::endl;
        return false;
    }

    using CreateFunc = INetplayPlugin* (*)();
    auto create = reinterpret_cast<CreateFunc>(handle->create_func);
    INetplayPlugin* instance = create();
    if (!instance) {
        std::cerr << "Failed to create netplay plugin instance: " << name << std::endl;
        return false;
    }

    deactivate_plugin(PluginType::Netplay);

    m_active.netplay = instance;
    m_active.netplay_handle = handle;
    m_config.set_selected_plugin(PluginType::Netplay, name);

    notify_plugin_changed(PluginType::Netplay, name);
    std::cout << "Activated netplay plugin: " << name << std::endl;
    return true;
}

void PluginManager::deactivate_plugin(PluginType type) {
    switch (type) {
        case PluginType::Emulator:
            if (m_active.emulator && m_active.emulator_handle) {
                using DestroyFunc = void (*)(IEmulatorPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.emulator_handle->destroy_func);
                if (destroy) destroy(m_active.emulator);
                m_active.emulator = nullptr;
                m_active.emulator_handle = nullptr;
            }
            break;
        case PluginType::Video:
            if (m_active.video && m_active.video_handle) {
                using DestroyFunc = void (*)(IVideoPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.video_handle->destroy_func);
                if (destroy) destroy(m_active.video);
                m_active.video = nullptr;
                m_active.video_handle = nullptr;
            }
            break;
        case PluginType::Audio:
            if (m_active.audio && m_active.audio_handle) {
                using DestroyFunc = void (*)(IAudioPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.audio_handle->destroy_func);
                if (destroy) destroy(m_active.audio);
                m_active.audio = nullptr;
                m_active.audio_handle = nullptr;
            }
            break;
        case PluginType::Input:
            if (m_active.input && m_active.input_handle) {
                using DestroyFunc = void (*)(IInputPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.input_handle->destroy_func);
                if (destroy) destroy(m_active.input);
                m_active.input = nullptr;
                m_active.input_handle = nullptr;
            }
            break;
        case PluginType::TAS:
            if (m_active.tas && m_active.tas_handle) {
                using DestroyFunc = void (*)(ITASPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.tas_handle->destroy_func);
                if (destroy) destroy(m_active.tas);
                m_active.tas = nullptr;
                m_active.tas_handle = nullptr;
            }
            break;
        case PluginType::SpeedrunTools:
            if (m_active.speedrun_tools && m_active.speedrun_tools_handle) {
                using DestroyFunc = void (*)(ISpeedrunToolsPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.speedrun_tools_handle->destroy_func);
                if (destroy) destroy(m_active.speedrun_tools);
                m_active.speedrun_tools = nullptr;
                m_active.speedrun_tools_handle = nullptr;
            }
            break;
        case PluginType::Game:
            if (m_active.game && m_active.game_handle) {
                using DestroyFunc = void (*)(IGamePlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.game_handle->destroy_func);
                if (destroy) destroy(m_active.game);
                m_active.game = nullptr;
                m_active.game_handle = nullptr;
            }
            break;
        case PluginType::Netplay:
            if (m_active.netplay && m_active.netplay_handle) {
                using DestroyFunc = void (*)(INetplayPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(m_active.netplay_handle->destroy_func);
                if (destroy) destroy(m_active.netplay);
                m_active.netplay = nullptr;
                m_active.netplay_handle = nullptr;
            }
            break;
    }
}

void PluginManager::build_legacy_plugin_list() {
    m_legacy_plugins.clear();

    // Build from emulator plugins for backward compatibility
    auto emulator_plugins = m_registry.get_plugins_of_type(PluginType::Emulator);
    for (const auto& metadata : emulator_plugins) {
        PluginInfo info;
        info.path = metadata.path.string();
        info.name = metadata.name;
        info.version = metadata.version;
        info.extensions = metadata.file_extensions;

        // If this is the active plugin, set the instance
        if (m_active.emulator && metadata.name == m_config.get_selected_plugin(PluginType::Emulator)) {
            info.instance = m_active.emulator;
            info.handle = m_active.emulator_handle ? m_active.emulator_handle->library_handle : nullptr;
            m_active_plugin_info = &m_legacy_plugins.emplace_back(std::move(info));
        } else {
            m_legacy_plugins.push_back(std::move(info));
        }
    }
}

void PluginManager::notify_plugin_changed(PluginType type, const std::string& name) {
    for (const auto& callback : m_change_callbacks) {
        callback(type, name);
    }
}

// Legacy API implementations

PluginInfo* PluginManager::find_plugin_for_extension(const std::string& extension) {
    std::string ext = extension;

    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    for (auto& plugin : m_legacy_plugins) {
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
    for (auto& plugin : m_legacy_plugins) {
        if (plugin.name == name) {
            return &plugin;
        }
    }
    return nullptr;
}

bool PluginManager::set_active_plugin(const std::string& name) {
    return activate_emulator_plugin(name);
}

bool PluginManager::set_active_plugin_for_file(const std::string& filepath) {
    std::string ext = get_file_extension(filepath);
    auto plugins = m_registry.find_plugins_for_extension(ext);
    if (!plugins.empty()) {
        return activate_emulator_plugin(plugins[0].name);
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

    // Store the ROM path for save file support
    m_current_rom_path = path;

    bool result = load_rom(data.data(), data.size());

    // If ROM loaded successfully and has battery save, try to load it
    if (result) {
        load_battery_save();
    }

    return result;
}

bool PluginManager::load_rom(const uint8_t* data, size_t size) {
    if (!m_active.emulator) {
        std::cerr << "No active emulator plugin" << std::endl;
        return false;
    }

    bool result = m_active.emulator->load_rom(data, size);

    // If ROM loaded successfully, try to activate a game plugin for it
    if (result) {
        uint32_t crc32 = m_active.emulator->get_rom_crc32();
        activate_game_plugin_for_rom(crc32);
    }

    return result;
}

void PluginManager::unload_rom() {
    // Save battery-backed data before unloading
    save_battery_save();

    // Deactivate game plugin first
    deactivate_plugin(PluginType::Game);

    if (m_active.emulator && m_active.emulator->is_rom_loaded()) {
        m_active.emulator->unload_rom();
    }

    // Clear the ROM path
    m_current_rom_path.clear();
}

bool PluginManager::is_rom_loaded() const {
    return m_active.emulator && m_active.emulator->is_rom_loaded();
}

uint32_t PluginManager::get_rom_crc32() const {
    if (m_active.emulator && m_active.emulator->is_rom_loaded()) {
        return m_active.emulator->get_rom_crc32();
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

fs::path PluginManager::get_save_file_path() const {
    if (m_current_rom_path.empty()) {
        return "";
    }

    // Use paths configuration if available
    if (m_paths_config) {
        return m_paths_config->get_battery_save_path(m_current_rom_path);
    }

    // Fallback: save in ROM directory with .sav extension
    fs::path rom_path(m_current_rom_path);
    return rom_path.parent_path() / (rom_path.stem().string() + ".sav");
}

bool PluginManager::load_battery_save() {
    if (!m_active.emulator || !m_active.emulator->is_rom_loaded()) {
        return false;
    }

    // Check if ROM has battery-backed save
    if (!m_active.emulator->has_battery_save()) {
        return false;
    }

    fs::path save_path = get_save_file_path();
    if (save_path.empty()) {
        return false;
    }

    // Ensure directory exists
    if (save_path.has_parent_path()) {
        fs::create_directories(save_path.parent_path());
    }

    // Check if save file exists
    if (!fs::exists(save_path)) {
        std::cout << "No save file found: " << save_path << std::endl;
        return false;
    }

    // Read save file
    std::ifstream file(save_path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open save file: " << save_path << std::endl;
        return false;
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "Failed to read save file" << std::endl;
        return false;
    }

    // Load save data into emulator
    if (m_active.emulator->set_battery_save_data(data)) {
        std::cout << "Loaded battery save: " << save_path << " (" << size << " bytes)" << std::endl;
        return true;
    }

    std::cerr << "Failed to load battery save data" << std::endl;
    return false;
}

bool PluginManager::save_battery_save() {
    if (!m_active.emulator || !m_active.emulator->is_rom_loaded()) {
        return false;
    }

    // Check if ROM has battery-backed save
    if (!m_active.emulator->has_battery_save()) {
        return false;
    }

    fs::path save_path = get_save_file_path();
    if (save_path.empty()) {
        return false;
    }

    // Ensure directory exists
    if (save_path.has_parent_path()) {
        fs::create_directories(save_path.parent_path());
    }

    // Get save data from emulator
    std::vector<uint8_t> data = m_active.emulator->get_battery_save_data();
    if (data.empty()) {
        std::cout << "No save data to write" << std::endl;
        return false;
    }

    // Write save file
    std::ofstream file(save_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create save file: " << save_path << std::endl;
        return false;
    }

    if (!file.write(reinterpret_cast<const char*>(data.data()), data.size())) {
        std::cerr << "Failed to write save file" << std::endl;
        return false;
    }

    std::cout << "Saved battery data: " << save_path << " (" << data.size() << " bytes)" << std::endl;
    return true;
}

} // namespace emu
