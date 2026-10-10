#include "nes_plugin_fixture.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
using emulator_test::require;
static uint64_t hash_bytes(const std::vector<uint8_t>& data) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (auto byte : data) hash = (hash ^ byte) * 0x100000001b3ULL;
    return hash;
}
static std::vector<uint8_t> read_bytes(const char* path) {
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "fixture file missing");
    return {std::istreambuf_iterator<char>(file), {}};
}
static uint32_t input(int player, uint64_t frame,bool solo=false,bool n64=false) {
    if(n64)return uint32_t(frame*(player?2654435761u:2246822519u)+(player?0x2468ace1u:0x13579bdfu));
    return static_cast<uint32_t>((frame * (player ? 13 : 7) + (player ? 1 : 3)) & (solo ? 4095:255));
}
struct Host final : emu::INetplayHost {
    emulator_test::LoadedCore& fixture;
    uint64_t rom, build;
    std::vector<uint8_t> image;
    bool paused = false, mismatch, replaced = false;
    int connections = 0;
    std::string error;
    Host(emulator_test::LoadedCore& core, const std::vector<uint8_t>& bytes, uint64_t build_id, bool different)
        : fixture(core), rom(hash_bytes(bytes)), build(build_id), image(bytes), mismatch(different) {}
    bool prepare_netplay() override { fixture.fresh_load(image); paused = true; return true; }
    emu::INetplayCapable* get_netplay_core() const override { return replaced ? nullptr : &fixture.netplay(); }
    uint64_t get_full_rom_hash() const override { return rom ^ (mismatch ? 1 : 0); }
    uint64_t get_core_build_hash() const override { return build; }
    void pause_emulator() override { paused = true; }
    void resume_emulator() override { paused = false; }
    bool is_emulator_paused() const override { return paused; }
    void reset_emulator() override { fixture.plugin->reset(); }
    uint64_t get_frame_count() const override { return fixture.plugin->get_frame_count(); }
    double get_fps() const override { return 60; }
    bool is_rom_loaded() const override { return fixture.plugin->is_rom_loaded(); }
    const char* get_rom_name() const override { return "Original controller probe"; }
    uint32_t get_rom_crc32() const override { return fixture.plugin->get_rom_crc32(); }
    const char* get_platform_name() const override { return fixture.plugin->get_info().name; }
    bool save_state_to_buffer(std::vector<uint8_t>& state) override { state = emulator_test::snapshot(fixture.netplay()); return true; }
    bool load_state_from_buffer(const std::vector<uint8_t>& state) override { return fixture.netplay().load_state_fast(state.data(), state.size()); }
    void set_controller_input(int, uint32_t) override {}
    uint32_t get_local_input(int) const override { return 0; }
    const char* get_config_directory() const override { return ""; }
    void show_notification(emu::NetplayNotificationType, const char* text, float) override { error = text; }
    void on_netplay_connected(int) override { ++connections; }
    void on_netplay_disconnected(const char* text) override { error = text; }
    void on_netplay_player_joined(const emu::NetplayPlayer&) override {}
    void on_netplay_player_left(int, const char*) override {}
    void on_netplay_desync(const emu::DesyncInfo&) override {}
    void on_netplay_chat_message(int, const char*) override {}
};
struct LoadedNetplay {
    void* handle = nullptr;
    emu::INetplayPlugin* plugin = nullptr;
    void (*destroy)(emu::INetplayPlugin*) = nullptr;
    LoadedNetplay(const char* path, Host& host) {
        handle = SDL_LoadObject(path); require(handle, SDL_GetError());
        auto version = reinterpret_cast<uint32_t (*)()>(SDL_LoadFunction(handle, "get_netplay_plugin_api_version"));
        require(version && version() == EMU_NETPLAY_PLUGIN_API_VERSION, "netplay ABI mismatch");
        auto create = reinterpret_cast<emu::INetplayPlugin* (*)()>(SDL_LoadFunction(handle, "create_netplay_plugin"));
        destroy = reinterpret_cast<void (*)(emu::INetplayPlugin*)>(SDL_LoadFunction(handle, "destroy_netplay_plugin"));
        require(create && destroy, "netplay factory missing");
        plugin = create(); require(plugin && plugin->initialize(&host), "netplay initialization failed");
    }
    ~LoadedNetplay() { if (plugin) destroy(plugin); if (handle) SDL_UnloadObject(handle); }
};
int main(int argc, char** argv) {
    try {
        require(argc == 9, "expected test arguments and netplay plugin path");
        std::string role = argv[1];
        auto rom = read_bytes(argv[3]);
        emulator_test::LoadedCore fixture(argv[2], rom);
        Host host(fixture, rom, hash_bytes(read_bytes(argv[2])), role == "client" && std::string(argv[7]) == "1");
        const bool n64=std::string(host.get_platform_name())=="N64";
        const bool solo=fixture.netplay().get_max_players()==1;
        LoadedNetplay loaded(argv[8], host);
        auto& plugin = *loaded.plugin;
        uint64_t replay_chunks = 0, live_chunks = 0, rollbacks = 0;
        auto audio_gate = [&](const float*, size_t, int) {
            if (plugin.is_rolling_back()) ++replay_chunks;
            else ++live_chunks;
        };
        if (role == "baseline") {
            host.prepare_netplay();
            fixture.plugin->set_audio_callback(audio_gate);
            for (uint64_t frame = 0; frame < 100; ++frame) fixture.netplay().run_frame_netplay(input(0, frame,solo,n64), solo ? 0:input(1, frame,false,n64));
        } else {
            // The GUI normally ran frames before opening its connection UI.
            // Different prior histories must be removed by preparation.
            for (int i = 0; i < (role == "host" ? 17 : 31); ++i)
                fixture.netplay().run_frame_netplay(255, 127);
            plugin.set_rollback_window(8);
            bool started = role == "host" ? plugin.host_session(0, "Host", false)
                : plugin.join_session("127.0.0.1", static_cast<uint16_t>(std::stoul(argv[6])), "Client");
            require(started, host.error.c_str());
            fixture.plugin->set_audio_callback(audio_gate);
            require(host.paused && host.connections == 0 && !plugin.is_playing(), "start did not freeze core pending actual handshake");
            if (role == "host") {
                std::string temporary = std::string(argv[5]) + ".tmp";
                std::ofstream port(temporary); port << plugin.get_listening_port() << "\n"; port.close();
                require(port.good(), "port publication failed");
                std::filesystem::rename(temporary, argv[5]);
            }
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            uint64_t tick = 0;
            while (std::chrono::steady_clock::now() < deadline) {
                if (++tick % (role == "host" ? 3 : 5) == 0 || !plugin.is_playing()) plugin.poll_network();
                if (host.get_frame_count() == 100 && plugin.get_confirmed_remote_frames() == 100 && !plugin.has_pending_output()) break;
                if (plugin.get_connection_state() == emu::NetplayConnectionState::Disconnected) {
                    if (std::string(argv[7]) == "1" && host.error.find("mismatch") != std::string::npos) {
                        require(host.connections == 0 && host.paused, "mismatched peer was started");
                        std::cout << "EXPECTED_HANDSHAKE_REJECTED " << host.error << "\n"; return 42;
                    }
                    throw std::runtime_error(host.error);
                }
                if (!host.paused && host.get_frame_count() < 100) {
                    auto frame = host.get_frame_count();
                    plugin.execute_frame(input(role == "client", frame,solo,n64));
                    require(host.get_frame_count() == frame || host.get_frame_count() == frame + 1,
                            "owned execution advanced twice in one frontend frame");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            require(host.connections == 1 && host.get_frame_count() == 100 && plugin.get_confirmed_remote_frames() == 100,
                    "plugin session did not complete actual handshake and confirmation drain");
            rollbacks = plugin.get_stats().rollback_count;
            if(!solo) require(rollbacks && replay_chunks && live_chunks,"plugin replay/audio gate not exercised");
            else {
                require(plugin.get_active_player_count()==1 && plugin.get_player_count()==2,"single-controller session counts wrong");
                require(plugin.get_spectator_count()==1 && plugin.get_player(1).role==emu::NetplayRole::Spectator,"single-controller viewer role wrong");
                if(role=="client") require(rollbacks && plugin.get_role()==emu::NetplayRole::Spectator,"viewer did not correct host predictions");
            }
        }
        auto state = emulator_test::snapshot(fixture.netplay());
        std::ofstream state_file(std::string(argv[4]) + ".state", std::ios::binary);
        state_file.write(reinterpret_cast<const char*>(state.data()), state.size()); state_file.close();
        require(state_file.good(), "state persistence failed");
        std::ofstream registers(std::string(argv[4]) + ".result");
        registers << "#VELOCE NETWORK 1 core=" << host.get_platform_name() << " frames=100 role=" << role << "\n"
                  << "INFO rollbacks=" << rollbacks << " replay_audio_chunks=" << replay_chunks << "\n";
        if(!solo && !n64) registers<<"REG p0_sum="<<unsigned(fixture.plugin->read_memory(0x12))<<" p1_sum="<<unsigned(fixture.plugin->read_memory(0x13))<<"\n";
        if(n64) {
            for(unsigned address=0x2000;address<0x2000+100*24;address+=4){
                uint32_t value=0;for(unsigned n=0;n<4;++n)value=value*256+fixture.plugin->read_memory(address+n);
                registers<<"REG address="<<address<<" value="<<value<<"\n";
            }
        }
        if(solo) {
            for(unsigned address=0x1000;address<((std::string(host.get_platform_name())=="3DS" || std::string(host.get_platform_name())=="Wii U")?0x1030u:0x1020u);address+=4) {
                uint32_t value=0; for(unsigned n=0;n<4;++n) { const auto byte=fixture.plugin->read_memory(address+n);if(std::string(host.get_platform_name())=="Wii U")value=value*256+byte;else value|=uint32_t(byte)<<(n*8); }
                registers<<"REG address="<<address<<" value="<<value<<"\n";
            }
        }
        registers.close(); require(registers.good(), "register persistence failed");
        fixture.plugin->set_audio_callback(nullptr);
        if (role == "baseline") {
            // Independent lifecycle checks while listening: no remote peer can
            // close first and mask the mutation being tested.
            const bool listening=plugin.host_session(0, "Lifecycle", false);
            require(listening, host.error.c_str());
            host.replaced = true;
            plugin.poll_network();
            require(plugin.get_connection_state() == emu::NetplayConnectionState::Disconnected &&
                    host.paused && host.error.find("Core or frame changed") != std::string::npos,
                    "core replacement did not stop session");
            host.replaced = false;
            const bool listening_again=plugin.host_session(0, "Lifecycle", false);
            require(listening_again, host.error.c_str());
            fixture.netplay().run_frame_netplay(0, 0);
            plugin.poll_network();
            require(plugin.get_connection_state() == emu::NetplayConnectionState::Disconnected &&
                    host.paused && host.error.find("Core or frame changed") != std::string::npos,
                    "external frame change did not stop session");
        }
        std::cout << "COMPLETE plugin " << role << " frames=100 rollbacks=" << rollbacks << "\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << "\n"; return 1; }
}
