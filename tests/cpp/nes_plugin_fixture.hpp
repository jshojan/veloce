#pragma once
#include "emu/emulator_plugin.hpp"
#include "emu/netplay_plugin.hpp"
#include <SDL.h>
#include <vector>
#include <stdexcept>
namespace emulator_test {
inline void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct LoadedCore {
    void* handle = nullptr;
    emu::IEmulatorPlugin* plugin = nullptr;
    using Destroy = void (*)(emu::IEmulatorPlugin*);
    Destroy destroy = nullptr;
    LoadedCore(const LoadedCore&) = delete;
    LoadedCore& operator=(const LoadedCore&) = delete;
    LoadedCore(const char* path, const std::vector<uint8_t>& rom) {
        handle = SDL_LoadObject(path);
        require(handle != nullptr, SDL_GetError());
        auto version = reinterpret_cast<uint32_t (*)()>(SDL_LoadFunction(handle, "get_plugin_api_version"));
        require(version && version() == EMU_PLUGIN_API_VERSION, "core plugin ABI mismatch");
        auto create = reinterpret_cast<emu::IEmulatorPlugin* (*)()>(SDL_LoadFunction(handle, "create_emulator_plugin"));
        destroy = reinterpret_cast<Destroy>(SDL_LoadFunction(handle, "destroy_emulator_plugin"));
        require(create && destroy, "missing core factory exports");
        plugin = create();
        require(plugin && plugin->load_rom(rom.data(), rom.size()), "original controller ROM did not load");
    }
    ~LoadedCore() { if (plugin && destroy) destroy(plugin); if (handle) SDL_UnloadObject(handle); }
    void fresh_load(const std::vector<uint8_t>& rom) {
        if (plugin) destroy(plugin);
        plugin = nullptr;
        auto create = reinterpret_cast<emu::IEmulatorPlugin* (*)()>(SDL_LoadFunction(handle, "create_emulator_plugin"));
        require(create, "core factory missing during reload");
        plugin = create();
        require(plugin && plugin->load_rom(rom.data(), rom.size()), "fresh original ROM load failed");
    }
    emu::INetplayCapable& netplay() {
        auto* core = dynamic_cast<emu::INetplayCapable*>(plugin);
        require(core != nullptr, "core plugin lacks netplay interface");
        return *core;
    }
};

inline std::vector<uint8_t> snapshot(emu::INetplayCapable& core) {
    const size_t required=core.save_state_fast(nullptr,0);
    require(required && required<=core.get_max_state_size(),"invalid current state size");
    std::vector<uint8_t> data(required);
    auto size = core.save_state_fast(data.data(), data.size());
    require(size > 0 && size <= data.size(), "final state save failed");
    data.resize(size);
    return data;
}

} // namespace emulator_test
