#include "emu/netplay_plugin.hpp"
#include "emu/emulator_plugin.hpp"
#include "peer_connection.hpp"
#include "rollback_session.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
uint64_t hash_bytes(const uint8_t* data, size_t size) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 0x100000001b3ULL;
    return hash;
}
class DefaultNetplayPlugin final : public emu::INetplayPlugin {
public:
    emu::NetplayPluginInfo get_info() override {
        return {"Default Netplay", "2.1.0", "Veloce Team",
                "Direct IPv4 TCP multiplayer with bounded rollback", emu::NetplayCapabilities::Rollback | emu::NetplayCapabilities::Spectators, 2, 0};
    }
    bool initialize(emu::INetplayHost* host) override { m_host = host; return host != nullptr; }
    ~DefaultNetplayPlugin() override { shutdown(); }
    void shutdown() override { disconnect(); m_host = nullptr; }
    bool host_session(uint16_t port, const char* name, bool is_public) override {
        if (is_public) return reject("Public matchmaking is unavailable");
        return start(true, "0.0.0.0", port, name);
    }
    bool join_session(const char* address, uint16_t port, const char* name) override {
        return start(false, address ? address : "", port, name);
    }
    bool join_as_spectator(const char* address,uint16_t port,const char* name) override {
        if(!m_host || !m_host->get_netplay_core() || m_host->get_netplay_core()->get_max_players()!=1)
            return reject("Viewer mode requires a single-controller core");
        return join_session(address,port,name);
    }
    int get_spectator_count() const override {
        return is_connected() && m_controller_count==1 ? 1:0;
    }
    void disconnect() override { stop("Disconnected"); }
    emu::NetplayConnectionState get_connection_state() const override { return m_state; }
    emu::NetplayRole get_role() const override { return m_role; }
    int get_local_player_id() const override { return m_local; }
    int get_player_count() const override { return is_connected() ? 2 : m_peer ? 1 : 0; }
    int get_active_player_count() const override { return m_peer ? m_controller_count : 0; }
    emu::NetplaySessionInfo get_session_info() const override {
        emu::NetplaySessionInfo info{};
        if (m_host) {
            std::strncpy(info.game_name, m_host->get_rom_name(), sizeof(info.game_name) - 1);
            std::strncpy(info.platform, m_host->get_platform_name(), sizeof(info.platform) - 1);
            info.game_crc32 = m_host->get_rom_crc32();
        }
        info.max_players = 2; info.player_count = get_player_count();
        info.rollback_frames = m_window;
        return info;
    }
    emu::NetplayPlayer get_player(int id) const override {
        emu::NetplayPlayer player{};
        player.player_id = id;
        player.role = id == 0 ? emu::NetplayRole::Host : m_controller_count==1 ? emu::NetplayRole::Spectator : emu::NetplayRole::Client;
        player.is_local = id == m_local;
        player.is_ready = is_playing();
        std::strncpy(player.name, player.is_local ? m_name.c_str() : "Peer", sizeof(player.name) - 1);
        return player;
    }
    void set_ready(bool) override {} // Both validated peers start automatically.
    bool owns_frame_execution() const override { return true; }
    uint16_t get_listening_port() const override { return m_peer ? m_peer->port() : 0; }
    uint64_t get_confirmed_remote_frames() const override { return m_received; }
    bool has_pending_output() const override { return m_peer && m_peer->has_pending_output(); }
    bool is_rolling_back() const override { return m_execution && m_execution->is_replaying(); }
    int get_current_rollback_depth() const override {
        return m_execution ? static_cast<int>(m_execution->last_rollback_depth()) : 0;
    }
    emu::NetplayStats get_stats() const override {
        emu::NetplayStats stats{};
        stats.rollback_count = static_cast<int>(m_execution ? m_execution->rollback_count() : m_rollbacks);
        stats.max_rollback_frames = m_max_depth;
        return stats;
    }
    int get_ping(int = -1) const override { return 0; } // No measured RTT yet.
    void set_input_delay(int) override {} // Current implementation has no scheduled delay.
    int get_input_delay() const override { return 0; }
    void set_rollback_window(int frames) override {
        if (!m_peer) m_window = std::clamp(frames, 1, emu::NETPLAY_MAX_ROLLBACK_FRAMES);
    }
    int get_rollback_window() const override { return m_window; }
    bool begin_frame() override { return false; } // Use execute_frame(), avoiding a second core frame.
    void end_frame() override {}
    void send_input(int player, uint32_t buttons, uint64_t) override { set_local_input(player, buttons); }
    void set_local_input(int player, uint32_t buttons) override { if (player == m_local) m_input = m_controller_count==1 && m_local==1 ? 0:buttons; }
    bool get_input(int player, uint32_t& buttons, uint64_t) override {
        buttons = player == m_local ? m_input : 0; return false;
    }
    void request_state_sync() override { reject("State resynchronization is unavailable; reconnect both peers"); }
    void send_state(const std::vector<uint8_t>&, uint64_t) override {}

    void poll_network() override {
        if (!m_peer) return;
        // Never dereference a previous core if the host replaced it.
        if (!m_host || !m_host->is_rom_loaded() || m_host->get_netplay_core() != m_core ||
            m_host->get_frame_count() != m_execution->next_frame()) {
            stop("Core or frame changed during netplay"); return;
        }
        m_peer->poll();
        for (const auto& input : m_peer->take_inputs()) {
            if (input.frame != m_first + m_received ||
                m_execution->submit_remote(input.frame, input.buttons) != emu::RollbackSession::Result::Accepted) {
                stop("Remote input order or rollback confirmation failed"); return;
            }
            ++m_received;
            m_max_depth = std::max(m_max_depth, static_cast<int>(m_execution->last_rollback_depth()));
        }
        if (m_peer->state() == emu::PeerConnection::State::Failed ||
            m_peer->state() == emu::PeerConnection::State::Closed) {
            auto error = m_peer->error(); stop(error.c_str()); return;
        }
        if (m_state == emu::NetplayConnectionState::Connecting &&
            m_peer->state() == emu::PeerConnection::State::Ready) {
            m_state = emu::NetplayConnectionState::Playing;
            m_host->on_netplay_connected(m_local);
            m_host->resume_emulator();
        }
    }
    bool execute_frame(uint32_t buttons) override {
        if(m_controller_count==1 && m_local==1) buttons=0;
        if (!is_playing() || !m_execution) return false;
        if (m_host->get_netplay_core() != m_core || m_host->get_frame_count() != m_execution->next_frame()) {
            stop("Core or frame changed during netplay"); return false;
        }
        // Hold the sampled input across a speculation-limit stall.
        if (!m_pinned) m_pinned = buttons;
        auto frame = m_execution->next_frame();
        auto result = m_execution->advance(*m_pinned);
        if (result == emu::RollbackSession::Result::Waiting) return false;
        if (result != emu::RollbackSession::Result::Advanced) { stop("Rollback execution failed"); return false; }
        if (!m_peer->send_input(frame, *m_pinned)) {
            auto error = m_peer->error(); stop(error.c_str()); return false;
        }
        m_pinned.reset();
        return true;
    }
    void set_imgui_context(void* context) override { ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context)); }
    void show_host_dialog() override { m_show_host = true; }
    void show_join_dialog() override { m_show_join = true; }
    void show_panel(bool show) override { m_show_panel = show; }
    bool is_panel_visible() const override { return m_show_panel; }
    bool render_menu() override {
        if (!ImGui::BeginMenu("Netplay")) return false;
        if (ImGui::MenuItem("Host Game...", nullptr, false, !m_peer)) m_show_host = true;
        if (ImGui::MenuItem("Join Game...", nullptr, false, !m_peer)) m_show_join = true;
        ImGui::MenuItem("Netplay Panel", nullptr, &m_show_panel);
        if (ImGui::MenuItem("Disconnect", nullptr, false, bool(m_peer))) disconnect();
        ImGui::EndMenu(); return true;
    }
    void render_gui() override {
        if (m_show_host || m_show_join) {
            bool hosting = m_show_host;
            bool& visible = hosting ? m_show_host : m_show_join;
            if (ImGui::Begin(hosting ? "Host Direct Netplay" : "Join Direct Netplay", &visible)) {
                ImGui::TextWrapped("Load the same ROM in both apps first. Connecting reloads and pauses the game; matching peers start automatically.");
                if(m_host && m_host->get_netplay_core() && m_host->get_netplay_core()->get_max_players()==1)
                    ImGui::TextWrapped("Single controller: the host plays and the joining peer watches.");
                ImGui::InputText("Player name", m_ui_name, sizeof(m_ui_name));
                if (!hosting) ImGui::InputText("Host IPv4 address", m_address, sizeof(m_address));
                ImGui::InputInt("TCP port", &m_port);
                ImGui::BeginDisabled(bool(m_peer));
                ImGui::SliderInt("Rollback frames", &m_window, 1, emu::NETPLAY_MAX_ROLLBACK_FRAMES);
                ImGui::EndDisabled();
                if (ImGui::Button(hosting ? "Listen" : "Connect")) {
                    if (m_port < (hosting ? 0 : 1) || m_port > 65535) reject("Invalid TCP port");
                    else if (hosting ? host_session(static_cast<uint16_t>(m_port), m_ui_name, false)
                                     : join_session(m_address, static_cast<uint16_t>(m_port), m_ui_name)) {
                        visible = false; m_show_panel = true;
                    }
                }
            }
            ImGui::End();
        }
        if (m_show_panel) {
            if (ImGui::Begin("Netplay", &m_show_panel)) {
                ImGui::TextUnformatted(is_playing() ? "Playing" : m_peer ? "Waiting for compatible peer" : "Disconnected");
                if (m_peer && m_local == 0) ImGui::Text("Listening on TCP port %u", unsigned(get_listening_port()));
                if(m_controller_count==1) ImGui::TextUnformatted(m_local==0 ? "Host owns the controller" : "Watching host controller input");
                ImGui::Text("Local player: %d", m_local + 1);
                ImGui::Text("Rollbacks: %d", get_stats().rollback_count);
                ImGui::TextWrapped("Direct IPv4 only. Peer name/chat, relay, session codes, additional spectators, RTT, reconnect, and desync recovery are unavailable.");
                if (!m_error.empty()) ImGui::TextWrapped("%s", m_error.c_str());
                if (m_peer && ImGui::Button("Disconnect")) disconnect();
            }
            ImGui::End();
        }
    }
private:
    bool reject(const char* error) {
        m_error = error;
        if (m_host) m_host->show_notification(emu::NetplayNotificationType::Error, m_error.c_str());
        return false;
    }
    void stop(const char* reason) {
        if (!m_peer) return;
        m_error = reason;
        m_rollbacks = m_execution ? m_execution->rollback_count() : m_rollbacks;
        m_execution.reset(); m_core = nullptr; m_peer.reset(); m_pinned.reset(); m_controller_count=0;
        m_state = emu::NetplayConnectionState::Disconnected;
        m_role = emu::NetplayRole::None;
        if (m_host) {
            m_host->pause_emulator();
            m_host->on_netplay_disconnected(m_error.c_str());
        }
    }
    bool start(bool hosting, const char* address, uint16_t port, const char* name) {
        if (!m_host || m_peer) return reject("A host is required and an existing session must be disconnected");
        m_core = m_host->get_netplay_core();
        if (!m_host->is_rom_loaded() || !m_core || !m_core->is_deterministic() || m_core->get_max_players() < 1)
            return reject("Load a deterministic netplay core first");
        if (!m_host->get_full_rom_hash() || !m_host->get_core_build_hash())
            return reject("Full ROM and core build identities are required");
        try {
            if (!m_host->prepare_netplay()) return reject("Could not prepare a fresh core/ROM");
            m_core = m_host->get_netplay_core();
            if (!m_core || !m_core->is_deterministic() || m_core->get_max_players() < 1)
                return reject("Prepared core lacks deterministic controller support");
            m_controller_count=std::min(2,m_core->get_max_players());
            std::vector<uint8_t> state;
            if (!m_host->save_state_to_buffer(state) || state.empty()) return reject("Could not save initial state");
            emu::PeerIdentity identity;
            auto platform = m_host->get_platform_name();
            identity.core_id = hash_bytes(reinterpret_cast<const uint8_t*>(platform), std::strlen(platform));
            // Pin controller ownership mode in the existing core identity.
            identity.core_id=(identity.core_id^uint64_t(m_controller_count))*0x100000001b3ULL;
            identity.build_id = m_host->get_core_build_hash(); identity.rom_hash = m_host->get_full_rom_hash();
            identity.rom_crc32 = m_host->get_rom_crc32(); identity.core_abi = EMU_PLUGIN_API_VERSION;
            identity.first_frame = m_host->get_frame_count(); identity.state_hash = hash_bytes(state.data(), state.size());
            identity.rollback_window = m_window;
            m_local = hosting ? 0 : 1; m_first = identity.first_frame;
            m_execution = std::make_unique<emu::RollbackSession>(*m_core, m_local, m_window, m_first);
            m_peer = std::make_unique<emu::PeerConnection>(identity, m_local);
            if (!(hosting ? m_peer->listen(address, port) : m_peer->connect(address, port))) {
                auto error = m_peer->error(); stop(error.c_str()); return reject(error.c_str());
            }
            m_name = name ? name : "Player"; m_error.clear(); m_received = 0; m_input = 0;
            m_rollbacks = 0; m_max_depth = 0;
            m_role = hosting ? emu::NetplayRole::Host : m_controller_count==1 ? emu::NetplayRole::Spectator : emu::NetplayRole::Client;
            m_state = emu::NetplayConnectionState::Connecting;
            return true;
        } catch (const std::exception& error) {
            auto message = std::string(error.what());
            stop(message.c_str()); m_execution.reset(); m_core = nullptr;
            return reject(message.c_str());
        }
    }
    emu::INetplayHost* m_host = nullptr;
    emu::INetplayCapable* m_core = nullptr;
    std::unique_ptr<emu::PeerConnection> m_peer;
    std::unique_ptr<emu::RollbackSession> m_execution;
    std::optional<uint32_t> m_pinned;
    emu::NetplayConnectionState m_state = emu::NetplayConnectionState::Disconnected;
    emu::NetplayRole m_role = emu::NetplayRole::None;
    int m_local = 0, m_window = 8, m_max_depth = 0, m_controller_count = 0;
    uint64_t m_first = 0, m_received = 0, m_rollbacks = 0;
    uint32_t m_input = 0;
    std::string m_name, m_error;
    bool m_show_host = false, m_show_join = false, m_show_panel = false;
    char m_ui_name[64] = "Player", m_address[64] = "127.0.0.1";
    int m_port = 7845;
};
}
extern "C" {
EMU_PLUGIN_EXPORT emu::INetplayPlugin* create_netplay_plugin() { return new DefaultNetplayPlugin; }
EMU_PLUGIN_EXPORT void destroy_netplay_plugin(emu::INetplayPlugin* plugin) { delete plugin; }
EMU_PLUGIN_EXPORT uint32_t get_netplay_plugin_api_version() { return EMU_NETPLAY_PLUGIN_API_VERSION; }
}
