#include "plugin_manager.hpp"
#include "paths_config.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace emu {

namespace fs = std::filesystem;

// ============================================================
// GamePluginHost implementation
// ============================================================

uint8_t GamePluginHost::read_memory(uint16_t address) {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        return m_plugin_manager->get_emulator_plugin()->read_memory(address);
    }
    return 0;
}

uint16_t GamePluginHost::read_memory_16(uint16_t address) {
    // Little-endian read
    uint16_t lo = read_memory(address);
    uint16_t hi = read_memory(address + 1);
    return lo | (hi << 8);
}

uint32_t GamePluginHost::read_memory_32(uint16_t address) {
    // Little-endian read
    uint32_t b0 = read_memory(address);
    uint32_t b1 = read_memory(address + 1);
    uint32_t b2 = read_memory(address + 2);
    uint32_t b3 = read_memory(address + 3);
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

void GamePluginHost::write_memory(uint16_t address, uint8_t value) {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        m_plugin_manager->get_emulator_plugin()->write_memory(address, value);
    }
}

bool GamePluginHost::is_emulator_running() const {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        return m_plugin_manager->get_emulator_plugin()->is_rom_loaded();
    }
    return false;
}

bool GamePluginHost::is_emulator_paused() const {
    return m_paused;
}

uint64_t GamePluginHost::get_frame_count() const {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        return m_plugin_manager->get_emulator_plugin()->get_frame_count();
    }
    return 0;
}

double GamePluginHost::get_fps() const {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        return m_plugin_manager->get_emulator_plugin()->get_info().native_fps;
    }
    return 60.0;
}

const char* GamePluginHost::get_rom_name() const {
    return m_rom_name.c_str();
}

uint32_t GamePluginHost::get_rom_crc32() const {
    return m_rom_crc32;
}

const char* GamePluginHost::get_platform_name() const {
    if (m_plugin_manager && m_plugin_manager->get_emulator_plugin()) {
        return m_plugin_manager->get_emulator_plugin()->get_info().name;
    }
    return "Unknown";
}

const char* GamePluginHost::get_selected_category() const {
    return m_category.c_str();
}

void GamePluginHost::log_message(const char* message) {
    std::cout << "[GamePlugin] " << message << std::endl;
}

void GamePluginHost::on_timer_started() {
    std::cout << "[GamePluginHost] Timer started" << std::endl;
}

void GamePluginHost::on_timer_stopped() {
    std::cout << "[GamePluginHost] Timer stopped" << std::endl;
}

void GamePluginHost::on_split_triggered(int split_index) {
    std::cout << "[GamePluginHost] Split " << split_index << " triggered" << std::endl;
}

void GamePluginHost::on_run_completed(uint64_t final_time_ms) {
    std::cout << "[GamePluginHost] Run completed: " << final_time_ms << "ms" << std::endl;
}

void GamePluginHost::on_run_reset() {
    std::cout << "[GamePluginHost] Run reset" << std::endl;
}

void GamePluginHost::set_rom_info(const std::string& name, uint32_t crc32) {
    m_rom_name = name;
    m_rom_crc32 = crc32;
}

// ============================================================
// PluginManager implementation
// ============================================================

PluginManager::PluginManager()
    : m_game_host(std::make_unique<GamePluginHost>(this)) {
}

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
    // These contain: audio, input, TAS, game plugins (timer, auto-splitters)
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

    // Load all emulator plugins (for configuration access)
    load_all_emulator_plugins();

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

    // Auto-activate netplay plugin if available (for GUI integration)
    auto netplay_plugins = m_registry.get_plugins_of_type(PluginType::Netplay);
    if (!netplay_plugins.empty()) {
        std::string selected = m_config.get_selected_plugin(PluginType::Netplay);
        if (selected.empty() || !activate_netplay_plugin(selected)) {
            // Use first available
            activate_netplay_plugin(netplay_plugins[0].name);
        }
    }

    return true;
}

void PluginManager::load_all_emulator_plugins() {
    auto emulator_plugins = m_registry.get_plugins_of_type(PluginType::Emulator);

    for (const auto& metadata : emulator_plugins) {
        // Load the plugin library
        PluginHandle* handle = m_registry.load_plugin(metadata);
        if (!handle || !handle->create_func) {
            std::cerr << "Failed to load emulator plugin for config: " << metadata.name << std::endl;
            continue;
        }

        // Create instance
        using CreateFunc = IEmulatorPlugin* (*)();
        auto create = reinterpret_cast<CreateFunc>(handle->create_func);
        IEmulatorPlugin* instance = create();
        if (!instance) {
            std::cerr << "Failed to create emulator plugin instance for config: " << metadata.name << std::endl;
            continue;
        }

        // Load configuration for this plugin
        fs::path config_path = get_core_config_path(metadata.name);
        if (instance->load_config(config_path.string().c_str())) {
            if (fs::exists(config_path)) {
                std::cout << "Loaded config for " << metadata.name << " from " << config_path << std::endl;
            }
        }

        // Store in our list
        EmulatorPluginInstance inst;
        inst.plugin = instance;
        inst.handle = handle;
        inst.name = metadata.name;
        inst.library_path = metadata.path.string();
        m_emulator_plugins.push_back(std::move(inst));
    }
}

fs::path PluginManager::get_core_config_path(const std::string& core_name) const {
    // Put core configs in config/cores/<name>.json
    fs::path config_dir = fs::current_path() / "config" / "cores";

    // Create directory if it doesn't exist
    if (!fs::exists(config_dir)) {
        fs::create_directories(config_dir);
    }

    // Normalize name to lowercase for file
    std::string filename = core_name;
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

    return config_dir / (filename + ".json");
}

IEmulatorPlugin* PluginManager::get_emulator_plugin_by_name(const std::string& name) const {
    for (const auto& inst : m_emulator_plugins) {
        if (inst.name == name) {
            return inst.plugin;
        }
    }
    return nullptr;
}

void PluginManager::shutdown() {
    // Prevent double-shutdown (called from both Application::shutdown() and destructor)
    if (m_shutdown_called) return;
    m_shutdown_called = true;

    unload_rom();

    // Deactivate all plugins
    deactivate_plugin(PluginType::Netplay);
    deactivate_all_game_plugins();  // Use new method for multiple game plugins
    deactivate_plugin(PluginType::TAS);
    deactivate_plugin(PluginType::Input);
    deactivate_plugin(PluginType::Audio);
    deactivate_plugin(PluginType::Video);
    deactivate_plugin(PluginType::Emulator);

    // Save and destroy all emulator plugin instances (used for configuration)
    for (auto& inst : m_emulator_plugins) {
        if (inst.plugin) {
            // Save configuration before destroying
            fs::path config_path = get_core_config_path(inst.name);
            if (inst.plugin->save_config(config_path.string().c_str())) {
                std::cout << "Saved config for " << inst.name << " to " << config_path << std::endl;
            }

            if (inst.handle) {
                using DestroyFunc = void (*)(IEmulatorPlugin*);
                auto destroy = reinterpret_cast<DestroyFunc>(inst.handle->destroy_func);
                if (destroy) {
                    destroy(inst.plugin);
                }
            }
        }
    }
    m_emulator_plugins.clear();

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
        case PluginType::Game:          return activate_game_plugin(name);
        case PluginType::Netplay:       return activate_netplay_plugin(name);
        default: return false;
    }
}

bool PluginManager::activate_game_plugin_for_rom(uint32_t crc32) {
    // For now, load all enabled game plugins and let them check matches_rom()
    // Don't deactivate - keep all enabled game plugins active
    load_enabled_game_plugins();
    return !m_active.game_plugins.empty();
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

bool PluginManager::activate_game_plugin(const std::string& name) {
    // Delegate to the new multi-plugin method
    return activate_game_plugin_by_name(name);
}

bool PluginManager::activate_game_plugin_by_name(const std::string& name) {
    // Check if already active
    if (is_game_plugin_active(name)) {
        return true;
    }

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

    // Add to active game plugins list
    GamePluginInstance plugin_instance;
    plugin_instance.plugin = instance;
    plugin_instance.handle = handle;
    plugin_instance.name = name;
    plugin_instance.enabled = true;
    plugin_instance.visible = true;
    m_active.game_plugins.push_back(std::move(plugin_instance));

    notify_plugin_changed(PluginType::Game, name);
    std::cout << "Activated game plugin: " << name << std::endl;
    return true;
}

bool PluginManager::deactivate_game_plugin_by_name(const std::string& name) {
    auto it = std::find_if(m_active.game_plugins.begin(), m_active.game_plugins.end(),
        [&name](const GamePluginInstance& inst) { return inst.name == name; });

    if (it == m_active.game_plugins.end()) {
        return false;
    }

    // Destroy the plugin instance
    if (it->plugin && it->handle) {
        using DestroyFunc = void (*)(IGamePlugin*);
        auto destroy = reinterpret_cast<DestroyFunc>(it->handle->destroy_func);
        if (destroy) {
            destroy(it->plugin);
        }
    }

    m_active.game_plugins.erase(it);
    std::cout << "Deactivated game plugin: " << name << std::endl;
    return true;
}

bool PluginManager::is_game_plugin_active(const std::string& name) const {
    return std::find_if(m_active.game_plugins.begin(), m_active.game_plugins.end(),
        [&name](const GamePluginInstance& inst) { return inst.name == name; })
        != m_active.game_plugins.end();
}

void PluginManager::load_enabled_game_plugins() {
    // Get list of enabled game plugins from config
    auto enabled = m_config.get_enabled_game_plugins();

    // If no config, load all available game plugins
    if (enabled.empty()) {
        auto available = get_available_plugins(PluginType::Game);
        for (const auto& name : available) {
            if (!is_game_plugin_active(name)) {
                activate_game_plugin_by_name(name);
            }
        }
    } else {
        // Load only enabled plugins
        for (const auto& name : enabled) {
            if (!is_game_plugin_active(name)) {
                activate_game_plugin_by_name(name);
            }
        }
    }
}

void PluginManager::deactivate_all_game_plugins() {
    // Shutdown all game plugin instances before destroying
    for (auto& inst : m_active.game_plugins) {
        if (inst.plugin) {
            inst.plugin->shutdown();
        }
    }

    // Destroy all game plugin instances
    for (auto& inst : m_active.game_plugins) {
        if (inst.plugin && inst.handle) {
            using DestroyFunc = void (*)(IGamePlugin*);
            auto destroy = reinterpret_cast<DestroyFunc>(inst.handle->destroy_func);
            if (destroy) {
                destroy(inst.plugin);
            }
        }
    }
    m_active.game_plugins.clear();
}

void PluginManager::initialize_game_plugins() {
    // Initialize all active game plugins with the host interface
    for (auto& inst : m_active.game_plugins) {
        if (inst.plugin) {
            inst.plugin->initialize(m_game_host.get());
            std::cout << "[PluginManager] Initialized game plugin: " << inst.name << std::endl;
        }
    }
}

void PluginManager::update_game_plugins() {
    // Update all active game plugins (for timer updates and auto-split detection)
    for (auto& inst : m_active.game_plugins) {
        if (inst.plugin && inst.enabled) {
            inst.plugin->on_frame();
        }
    }
}

void PluginManager::notify_game_plugins_rom_loaded() {
    // Get ROM info from emulator and update the host
    if (m_active.emulator && m_active.emulator->is_rom_loaded()) {
        // Extract ROM name from path (filename without extension)
        fs::path rom_path(m_current_rom_path);
        std::string rom_name = rom_path.stem().string();
        uint32_t crc32 = m_active.emulator->get_rom_crc32();

        m_game_host->set_rom_info(rom_name, crc32);

        // Notify all game plugins about ROM load
        for (auto& inst : m_active.game_plugins) {
            if (inst.plugin) {
                // Check if plugin matches this ROM
                if (inst.plugin->matches_rom(crc32, rom_name.c_str())) {
                    inst.plugin->on_rom_loaded();
                }
            }
        }
    }
}

void PluginManager::notify_game_plugins_rom_unloaded() {
    // Notify all game plugins about ROM unload
    for (auto& inst : m_active.game_plugins) {
        if (inst.plugin) {
            inst.plugin->on_rom_unloaded();
        }
    }
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

    // Initialize the plugin with the netplay host if available
    if (m_netplay_host) {
        instance->initialize(m_netplay_host);
    }

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
        case PluginType::Game:
            // Use the multi-plugin deactivation method
            deactivate_all_game_plugins();
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

        // Initialize game plugins with the host interface
        initialize_game_plugins();

        // Notify game plugins about ROM load
        notify_game_plugins_rom_loaded();
    }

    return result;
}

void PluginManager::unload_rom() {
    // Save battery-backed data before unloading
    save_battery_save();

    // Notify game plugins about ROM unload
    notify_game_plugins_rom_unloaded();

    // Note: We don't deactivate game plugins on ROM unload
    // They remain loaded and can be used for the next ROM
    // The plugin visibility state is preserved

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
