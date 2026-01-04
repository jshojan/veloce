#include "emu/netplay_plugin.hpp"
#include "netplay_input_manager.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <string>
#include <deque>
#include <vector>
#include <chrono>
#include <sstream>
#include <random>
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace {

// ============================================================================
// Lobby State - tracks the pre-game flow
// ============================================================================

enum class LobbyState {
    WaitingForPlayers,   // Host created session, waiting for others to join
    WaitingForGame,      // Players connected, waiting for host to select game
    GameSelected,        // Host selected game, waiting for all to load & ready
    AllReady,            // Everyone has matching ROM and is ready
    Playing              // Game is running
};

// ============================================================================
// Game Selection Info - shared between host and clients
// ============================================================================

struct GameInfo {
    std::string name;
    std::string platform;
    uint32_t crc32 = 0;
    bool selected = false;  // Has the host selected a game?
};

// ============================================================================
// Player ROM Status
// ============================================================================

enum class RomStatus {
    NotLoaded,       // Player hasn't loaded the ROM yet
    Loaded,          // ROM loaded
    CrcMatch,        // CRC matches host
    CrcMismatch      // CRC doesn't match host (warning!)
};

// ============================================================================
// Session Code Helper
// ============================================================================

struct SessionCode {
    std::string code;
    bool valid = false;

    static SessionCode generate() {
        SessionCode sc;
        static const char* letters = "ABCDEFGHJKLMNPQRSTUVWXYZ";
        static const char* digits = "0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> letter_dist(0, 23);
        std::uniform_int_distribution<> digit_dist(0, 9);

        char buf[8];
        buf[0] = letters[letter_dist(gen)];
        buf[1] = letters[letter_dist(gen)];
        buf[2] = letters[letter_dist(gen)];
        buf[3] = '-';
        buf[4] = digits[digit_dist(gen)];
        buf[5] = digits[digit_dist(gen)];
        buf[6] = digits[digit_dist(gen)];
        buf[7] = '\0';
        sc.code = buf;
        sc.valid = true;
        return sc;
    }

    static bool validate(const std::string& code) {
        if (code.length() != 7) return false;
        if (code[3] != '-') return false;
        for (int i = 0; i < 3; i++) {
            if (!std::isalpha(static_cast<unsigned char>(code[i]))) return false;
        }
        for (int i = 4; i < 7; i++) {
            if (!std::isdigit(static_cast<unsigned char>(code[i]))) return false;
        }
        return true;
    }
};

// ============================================================================
// Chat Message
// ============================================================================

struct ChatMessage {
    std::string sender;
    std::string message;
    std::chrono::steady_clock::time_point timestamp;
    int player_id;
    bool is_system;
};

// ============================================================================
// Recent Connection
// ============================================================================

struct RecentConnection {
    std::string name;
    std::string ip;
    int port;
};

// ============================================================================
// Extended Player Info (local tracking)
// ============================================================================

struct PlayerInfo {
    emu::NetplayPlayer base;
    RomStatus rom_status = RomStatus::NotLoaded;
    uint32_t rom_crc32 = 0;
};

// ============================================================================
// Default Netplay Plugin
// ============================================================================

class DefaultNetplayPlugin : public emu::INetplayPlugin {
public:
    DefaultNetplayPlugin() {
        std::strncpy(m_host_name, "Player", sizeof(m_host_name) - 1);
        std::strncpy(m_join_name, "Player", sizeof(m_join_name) - 1);
        std::strncpy(m_join_ip, "127.0.0.1", sizeof(m_join_ip) - 1);
    }

    ~DefaultNetplayPlugin() override {
        save_settings();
    }

    // =========================================================================
    // Plugin Info
    // =========================================================================

    emu::NetplayPluginInfo get_info() override {
        return {
            "Default Netplay",
            "1.0.0",
            "Veloce Team",
            "Rollback netplay with lobby-first game selection",
            0,  // capabilities
            4,  // max players
            4   // max spectators
        };
    }

    bool initialize(emu::INetplayHost* host) override {
        m_host = host;
        m_initialized = true;
        load_settings();
        return true;
    }

    void shutdown() override {
        save_settings();
        disconnect();
        m_host = nullptr;
        m_initialized = false;
    }

    // =========================================================================
    // Session Management
    // =========================================================================

    bool host_session(uint16_t port, const char* player_name, bool is_public) override {
        if (!m_host) {
            return false;
        }

        m_player_name = player_name ? player_name : "Player";
        m_port = port;
        m_role = emu::NetplayRole::Host;
        m_connection_state = emu::NetplayConnectionState::Connected;
        m_local_player_id = 0;
        m_session_code = SessionCode::generate();

        // Initialize lobby state
        m_lobby_state = LobbyState::WaitingForPlayers;
        m_game_info = {};  // Clear game selection

        // Initialize player 1 (self)
        m_player_info[0] = {};
        m_player_info[0].base = {0, {}, emu::NetplayRole::Host, 0, true, false};
        std::strncpy(m_player_info[0].base.name, m_player_name.c_str(), 63);
        m_player_info[0].rom_status = RomStatus::NotLoaded;
        m_player_count = 1;

        // Check if ROM is already loaded
        if (m_host->is_rom_loaded()) {
            update_local_rom_status();
        }

        add_system_message("Session started - waiting for players...");
        add_system_message("Tip: Select a game after players join, or load one now.");

        if (m_host) {
            m_host->show_notification(emu::NetplayNotificationType::Success,
                ("Hosting on port " + std::to_string(port)).c_str(), 4.0f);
            m_host->on_netplay_connected(m_local_player_id);
        }

        (void)is_public;
        return true;
    }

    bool join_session(const char* host_addr, uint16_t port, const char* player_name) override {
        if (!m_host) {
            return false;
        }

        m_player_name = player_name ? player_name : "Player";
        m_port = port;
        m_host_address = host_addr ? host_addr : "";
        m_role = emu::NetplayRole::Client;
        m_connection_state = emu::NetplayConnectionState::Connecting;

        // Initialize lobby state - waiting for host's game selection
        m_lobby_state = LobbyState::WaitingForGame;
        m_game_info = {};

        // Simulate connection (in real impl, this would be async)
        m_connection_state = emu::NetplayConnectionState::Connected;
        m_local_player_id = 1;

        // Simulate host player info
        m_player_info[0] = {};
        m_player_info[0].base = {0, {}, emu::NetplayRole::Host, 30, false, false};
        std::strncpy(m_player_info[0].base.name, "Host", 63);
        m_player_info[0].rom_status = RomStatus::NotLoaded;

        // Initialize self (client)
        m_player_info[1] = {};
        m_player_info[1].base = {1, {}, emu::NetplayRole::Client, 0, true, false};
        std::strncpy(m_player_info[1].base.name, m_player_name.c_str(), 63);
        m_player_info[1].rom_status = RomStatus::NotLoaded;
        m_player_count = 2;

        // Add to recent connections
        add_recent_connection(m_player_name, m_host_address, port);

        add_system_message("Connected to session");
        add_system_message("Waiting for host to select a game...");

        if (m_host) {
            m_host->show_notification(emu::NetplayNotificationType::Info,
                ("Connected to " + std::string(host_addr)).c_str(), 3.0f);
            m_host->on_netplay_connected(m_local_player_id);
        }

        return true;
    }

    void disconnect() override {
        if (m_connection_state != emu::NetplayConnectionState::Disconnected) {
            add_system_message("Disconnected from session");
            if (m_host) {
                m_host->show_notification(emu::NetplayNotificationType::Info, "Disconnected from netplay session");
                m_host->on_netplay_disconnected("User disconnected");
            }
        }
        m_connection_state = emu::NetplayConnectionState::Disconnected;
        m_role = emu::NetplayRole::None;
        m_lobby_state = LobbyState::WaitingForPlayers;
        m_game_info = {};
        m_player_count = 0;
        m_is_ready = false;
        m_is_rolling_back = false;
        m_rollback_depth = 0;
        m_input_manager.clear_assignments();
    }

    emu::NetplayConnectionState get_connection_state() const override {
        return m_connection_state;
    }

    emu::NetplayRole get_role() const override {
        return m_role;
    }

    emu::NetplaySessionInfo get_session_info() const override {
        emu::NetplaySessionInfo info = {};
        std::strncpy(info.session_id, m_session_code.code.c_str(), 63);
        std::strncpy(info.host_name, m_player_name.c_str(), 63);

        // Use game selection info if available, otherwise check host
        if (m_game_info.selected) {
            std::strncpy(info.game_name, m_game_info.name.c_str(), 255);
            std::strncpy(info.platform, m_game_info.platform.c_str(), 31);
            info.game_crc32 = m_game_info.crc32;
        } else if (m_host && m_host->is_rom_loaded()) {
            std::strncpy(info.game_name, m_host->get_rom_name(), 255);
            std::strncpy(info.platform, m_host->get_platform_name(), 31);
            info.game_crc32 = m_host->get_rom_crc32();
        } else {
            std::strncpy(info.game_name, "(No game selected)", 255);
            std::strncpy(info.platform, "-", 31);
            info.game_crc32 = 0;
        }

        info.player_count = m_player_count;
        info.max_players = 4;
        info.input_delay = m_input_delay;
        info.rollback_frames = m_rollback_window;
        return info;
    }

    const char* get_session_code() const override {
        return m_session_code.valid ? m_session_code.code.c_str() : nullptr;
    }

    int get_local_player_id() const override { return m_local_player_id; }
    int get_player_count() const override { return m_player_count; }

    emu::NetplayPlayer get_player(int player_id) const override {
        if (player_id >= 0 && player_id < m_player_count) {
            return m_player_info[player_id].base;
        }
        return {};
    }

    void set_ready(bool ready) override {
        // Can only ready up if we have the correct ROM loaded
        if (ready && m_game_info.selected) {
            if (m_player_info[m_local_player_id].rom_status != RomStatus::CrcMatch) {
                if (m_host) {
                    m_host->show_notification(emu::NetplayNotificationType::Warning,
                        "Load the correct ROM before readying up");
                }
                return;
            }
        }

        m_is_ready = ready;
        if (m_local_player_id >= 0 && m_local_player_id < m_player_count) {
            m_player_info[m_local_player_id].base.is_ready = ready;
        }

        // Check if all players are ready
        check_all_ready();
    }

    void send_chat_message(const char* message) override {
        if (message && strlen(message) > 0) {
            add_chat_message(m_player_name, message, m_local_player_id);
            if (m_host) {
                m_host->on_netplay_chat_message(m_local_player_id, message);
            }
        }
    }

    // =========================================================================
    // Game Selection (Host only)
    // =========================================================================

    // Called when host loads/changes ROM - updates game selection
    void on_rom_loaded() {
        if (!m_host || !is_connected()) return;

        update_local_rom_status();

        // If we're the host, broadcast the game selection
        if (m_role == emu::NetplayRole::Host) {
            m_game_info.name = m_host->get_rom_name();
            m_game_info.platform = m_host->get_platform_name();
            m_game_info.crc32 = m_host->get_rom_crc32();
            m_game_info.selected = true;

            m_lobby_state = LobbyState::GameSelected;

            add_system_message("Game selected: " + m_game_info.name);
            add_system_message("CRC32: " + crc32_to_string(m_game_info.crc32));
            add_system_message("Other players: load your copy of this ROM!");

            if (m_host) {
                m_host->show_notification(emu::NetplayNotificationType::Success,
                    ("Game selected: " + m_game_info.name).c_str(), 3.0f);
            }

            // In a real implementation, broadcast game info to all clients here
            // For now, simulate that clients receive the update
            simulate_game_broadcast_to_clients();
        } else {
            // Client loaded a ROM - check if it matches host's selection
            if (m_game_info.selected) {
                uint32_t local_crc = m_host->get_rom_crc32();
                if (local_crc == m_game_info.crc32) {
                    m_player_info[m_local_player_id].rom_status = RomStatus::CrcMatch;
                    add_system_message("ROM loaded - CRC matches!");
                    if (m_host) {
                        m_host->show_notification(emu::NetplayNotificationType::Success,
                            "ROM CRC matches host - ready to play!");
                    }
                } else {
                    m_player_info[m_local_player_id].rom_status = RomStatus::CrcMismatch;
                    add_system_message("WARNING: CRC mismatch! Your ROM differs from host's.");
                    add_system_message("Expected: " + crc32_to_string(m_game_info.crc32) +
                                       ", Got: " + crc32_to_string(local_crc));
                    if (m_host) {
                        m_host->show_notification(emu::NetplayNotificationType::Error,
                            "ROM CRC mismatch! Different ROM version?", 5.0f);
                    }
                }
            }
        }
    }

    // =========================================================================
    // Input Synchronization
    // =========================================================================

    bool begin_frame() override {
        if (!is_connected()) return true;

        // Only run frames if we're in Playing state
        if (m_lobby_state != LobbyState::Playing) {
            return false;  // Don't advance frame during lobby
        }

        return true;
    }

    void send_input(int player, uint32_t buttons, uint64_t frame) override {
        if (player == m_local_player_id) {
            m_input_manager.set_player_input(player, buttons);
        }
        (void)frame;
    }

    bool get_input(int player, uint32_t& buttons, uint64_t frame) override {
        buttons = m_input_manager.get_player_input(player);
        (void)frame;
        return true;
    }

    void end_frame() override {
        if (!is_connected()) return;
        m_is_rolling_back = false;
        m_rollback_depth = 0;
    }

    int get_active_player_count() const override {
        return m_active_player_count;
    }

    void get_synchronized_inputs_fast(std::vector<uint32_t>& out_inputs, uint64_t frame) override {
        if (static_cast<int>(out_inputs.size()) != m_active_player_count) {
            out_inputs.resize(m_active_player_count, 0);
        }

        for (int i = 0; i < m_active_player_count; i++) {
            uint32_t buttons = 0;
            get_input(i, buttons, frame);
            out_inputs[i] = buttons;
        }
    }

    void set_local_input(int player, uint32_t buttons) override {
        if (player >= 0 && player < m_active_player_count) {
            m_input_manager.set_player_input(player, buttons);

            if (is_connected() && m_host) {
                uint64_t frame = m_host->get_frame_count();
                send_input(player, buttons, frame);
            }
        }
    }

    void request_state_sync() override {
        if (m_host) {
            m_host->show_notification(emu::NetplayNotificationType::Info, "Requesting state sync...");
        }
    }

    void send_state(const std::vector<uint8_t>& state, uint64_t frame) override {
        (void)state;
        (void)frame;
    }

    void set_input_delay(int frames) override {
        m_input_delay = std::clamp(frames, 0, emu::NETPLAY_MAX_INPUT_DELAY);
    }

    int get_input_delay() const override { return m_input_delay; }

    void set_rollback_window(int frames) override {
        m_rollback_window = std::clamp(frames, 0, emu::NETPLAY_MAX_ROLLBACK_FRAMES);
    }

    int get_rollback_window() const override { return m_rollback_window; }
    int get_current_rollback_depth() const override { return m_rollback_depth; }
    bool is_rolling_back() const override { return m_is_rolling_back; }

    emu::NetplayStats get_stats() const override {
        emu::NetplayStats stats = {};
        for (int i = 0; i < m_player_count; i++) {
            if (!m_player_info[i].base.is_local) {
                stats.local_ping_ms = m_player_info[i].base.ping_ms;
                break;
            }
        }
        return stats;
    }

    int get_ping(int player_id) const override {
        if (player_id == -1) {
            int total = 0;
            int count = 0;
            for (int i = 0; i < m_player_count; i++) {
                if (!m_player_info[i].base.is_local) {
                    total += m_player_info[i].base.ping_ms;
                    count++;
                }
            }
            return count > 0 ? total / count : 0;
        }
        if (player_id >= 0 && player_id < m_player_count) {
            return m_player_info[player_id].base.ping_ms;
        }
        return 0;
    }

    // =========================================================================
    // GUI Integration
    // =========================================================================

    void set_imgui_context(void* context) override {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context));
    }

    bool render_menu() override {
        if (ImGui::BeginMenu("Netplay")) {
            bool is_conn = is_connected();

            // Host/Join always available when not connected
            if (ImGui::MenuItem("Host Game...", nullptr, false, !is_conn)) {
                m_show_host_dialog = true;
                m_session_code = SessionCode::generate();
            }

            if (ImGui::MenuItem("Join Game...", nullptr, false, !is_conn)) {
                m_show_join_dialog = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Netplay Panel", nullptr, m_show_panel)) {
                m_show_panel = !m_show_panel;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Disconnect", nullptr, false, is_conn)) {
                disconnect();
            }

            ImGui::EndMenu();
            return true;
        }
        return false;
    }

    void render_gui() override {
        // Check if ROM was loaded since last frame
        check_rom_status_change();

        if (m_show_host_dialog) {
            render_host_dialog();
        }

        if (m_show_join_dialog) {
            render_join_dialog();
        }

        if (m_show_panel) {
            render_main_panel();
        }

        if (m_show_overlay && is_connected() && m_lobby_state == LobbyState::Playing) {
            render_status_overlay();
        }
    }

    void show_host_dialog() override {
        m_show_host_dialog = true;
        m_session_code = SessionCode::generate();
    }

    void show_join_dialog() override {
        m_show_join_dialog = true;
    }

    void show_panel(bool show) override {
        m_show_panel = show;
    }

    bool is_panel_visible() const override {
        return m_show_panel;
    }

private:
    // =========================================================================
    // Lobby Flow Helpers
    // =========================================================================

    void update_local_rom_status() {
        if (!m_host || m_local_player_id < 0) return;

        if (m_host->is_rom_loaded()) {
            uint32_t local_crc = m_host->get_rom_crc32();
            m_player_info[m_local_player_id].rom_crc32 = local_crc;

            if (m_role == emu::NetplayRole::Host) {
                // Host's ROM is always the reference
                m_player_info[m_local_player_id].rom_status = RomStatus::CrcMatch;
            } else if (m_game_info.selected) {
                // Client: compare to host's selection
                if (local_crc == m_game_info.crc32) {
                    m_player_info[m_local_player_id].rom_status = RomStatus::CrcMatch;
                } else {
                    m_player_info[m_local_player_id].rom_status = RomStatus::CrcMismatch;
                }
            } else {
                m_player_info[m_local_player_id].rom_status = RomStatus::Loaded;
            }
        } else {
            m_player_info[m_local_player_id].rom_status = RomStatus::NotLoaded;
            m_player_info[m_local_player_id].rom_crc32 = 0;
        }
    }

    void check_rom_status_change() {
        // Track ROM load/unload
        bool rom_loaded = m_host && m_host->is_rom_loaded();
        static bool last_rom_loaded = false;
        static uint32_t last_crc = 0;

        if (rom_loaded != last_rom_loaded ||
            (rom_loaded && m_host->get_rom_crc32() != last_crc)) {
            if (rom_loaded) {
                on_rom_loaded();
                last_crc = m_host->get_rom_crc32();
            }
            last_rom_loaded = rom_loaded;
        }
    }

    void simulate_game_broadcast_to_clients() {
        // In a real implementation, this would send game info over network
        // For simulation, just update other players' view of the game info

        // Simulate that clients now see the game selection
        for (int i = 0; i < m_player_count; i++) {
            if (!m_player_info[i].base.is_local) {
                // Simulate client receiving game info and loading ROM
                // In reality this would happen when they load their copy
                m_player_info[i].rom_status = RomStatus::NotLoaded;
            }
        }
    }

    void check_all_ready() {
        if (!is_connected() || !m_game_info.selected) return;

        bool all_ready = true;
        bool all_have_rom = true;

        for (int i = 0; i < m_player_count; i++) {
            if (!m_player_info[i].base.is_ready) {
                all_ready = false;
            }
            if (m_player_info[i].rom_status != RomStatus::CrcMatch) {
                all_have_rom = false;
            }
        }

        if (all_ready && all_have_rom) {
            m_lobby_state = LobbyState::AllReady;
        } else if (m_game_info.selected) {
            m_lobby_state = LobbyState::GameSelected;
        }
    }

    void start_game() {
        if (m_lobby_state != LobbyState::AllReady) {
            if (m_host) {
                m_host->show_notification(emu::NetplayNotificationType::Warning,
                    "Not all players are ready!");
            }
            return;
        }

        m_lobby_state = LobbyState::Playing;
        m_connection_state = emu::NetplayConnectionState::Playing;

        // Setup input manager for gameplay
        setup_input_manager_for_session();

        add_system_message("Game started!");

        if (m_host) {
            m_host->show_notification(emu::NetplayNotificationType::Success,
                "Netplay game started!", 3.0f);
            m_host->resume_emulator();
        }
    }

    void setup_input_manager_for_session() {
        int max_players = 2;
        m_active_player_count = m_player_count > 0 ? m_player_count : 2;

        if (m_active_player_count > max_players) {
            m_active_player_count = max_players;
        }

        m_input_manager.set_max_players(m_active_player_count);
        m_input_manager.clear_assignments();

        if (m_role == emu::NetplayRole::Host) {
            m_input_manager.assign_controller_to_slot(emu::CONTROLLER_KEYBOARD, 0);
            m_input_manager.set_slot_local(0, true);

            for (int i = 1; i < m_active_player_count; i++) {
                m_input_manager.set_slot_local(i, false);
            }
        } else {
            if (m_local_player_id >= 0 && m_local_player_id < m_active_player_count) {
                m_input_manager.assign_controller_to_slot(emu::CONTROLLER_KEYBOARD, m_local_player_id);
                m_input_manager.set_slot_local(m_local_player_id, true);
            }

            for (int i = 0; i < m_active_player_count; i++) {
                if (i != m_local_player_id) {
                    m_input_manager.set_slot_local(i, false);
                }
            }
        }

        std::cout << "NetplayPlugin: Input manager configured for " << m_active_player_count
                  << " players (local: P" << (m_local_player_id + 1) << ")" << std::endl;
    }

    std::string crc32_to_string(uint32_t crc) const {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%08X", crc);
        return buf;
    }

    // =========================================================================
    // GUI Rendering
    // =========================================================================

    void render_host_dialog() {
        ImGui::SetNextWindowSize(ImVec2(400, 320), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Host Game", &m_show_host_dialog, ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("Host a new netplay session");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::InputText("Your Name", m_host_name, sizeof(m_host_name));
            ImGui::InputInt("Port", &m_host_port);
            if (m_host_port < 1024) m_host_port = 1024;
            if (m_host_port > 65535) m_host_port = 65535;

            ImGui::Checkbox("Generate Session Code", &m_use_session_code);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Generate a short code that others can use to join easily");
            }

            if (m_use_session_code && m_session_code.valid) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                ImGui::Text("Session Code: %s", m_session_code.code.c_str());
                ImGui::PopStyleColor();

                ImGui::SameLine();
                if (ImGui::SmallButton("Copy")) {
                    ImGui::SetClipboardText(m_session_code.code.c_str());
                    if (m_host) {
                        m_host->show_notification(emu::NetplayNotificationType::Success,
                            "Session code copied to clipboard");
                    }
                }

                ImGui::TextDisabled("Share this code with your opponent");
            }

            ImGui::Checkbox("Allow Spectators", &m_allow_spectators);

            ImGui::Spacing();

            // Info about lobby-first flow
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "You can select the game after players join.");

            bool rom_loaded = m_host && m_host->is_rom_loaded();
            if (rom_loaded) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                    "Current ROM: %s", m_host->get_rom_name());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float button_width = 100;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float total_width = button_width * 2 + spacing;
            float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
            if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            if (ImGui::Button("Start Host", ImVec2(button_width, 0))) {
                if (host_session(static_cast<uint16_t>(m_host_port), m_host_name, false)) {
                    m_show_host_dialog = false;
                    m_show_panel = true;
                    save_settings();
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
                m_show_host_dialog = false;
            }
        }
        ImGui::End();
    }

    void render_join_dialog() {
        ImGui::SetNextWindowSize(ImVec2(400, 320), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Join Game", &m_show_join_dialog, ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("Join an existing netplay session");
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::InputText("Your Name", m_join_name, sizeof(m_join_name));

            ImGui::Checkbox("Join by Session Code", &m_join_by_code);

            if (m_join_by_code) {
                ImGui::InputText("Session Code", m_join_code, sizeof(m_join_code));
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Enter the session code provided by the host (e.g., ABC-123)");
                }
            } else {
                ImGui::InputText("Host IP", m_join_ip, sizeof(m_join_ip));
                ImGui::InputInt("Port", &m_join_port);
                if (m_join_port < 1024) m_join_port = 1024;
                if (m_join_port > 65535) m_join_port = 65535;
            }

            ImGui::Checkbox("Join as Spectator", &m_join_as_spectator);

            ImGui::Spacing();

            // Info about lobby flow
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "You'll load the game after the host selects it.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float button_width = 100;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float total_width = button_width * 2 + spacing;
            float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
            if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            // Validate session code
            bool code_valid = true;
            if (m_join_by_code && strlen(m_join_code) > 0) {
                std::string code_upper = m_join_code;
                for (char& c : code_upper) c = std::toupper(static_cast<unsigned char>(c));
                code_valid = SessionCode::validate(code_upper);
                if (!code_valid) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                        "Invalid code format (use ABC-123)");
                }
            }

            if (ImGui::Button("Join", ImVec2(button_width, 0))) {
                bool success = false;
                if (m_join_by_code) {
                    if (m_host) {
                        m_host->show_notification(emu::NetplayNotificationType::Warning,
                            "Session code joining requires matchmaking server");
                    }
                } else {
                    success = join_session(m_join_ip, static_cast<uint16_t>(m_join_port), m_join_name);
                }

                if (success) {
                    m_show_join_dialog = false;
                    m_show_panel = true;
                    save_settings();
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
                m_show_join_dialog = false;
            }
        }
        ImGui::End();
    }

    void render_main_panel() {
        ImGui::SetNextWindowSize(ImVec2(450, 650), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Netplay", &m_show_panel)) {
            if (is_connected()) {
                render_connection_status();
                ImGui::Separator();
                render_lobby_status();
                ImGui::Separator();
                render_game_selection();
                ImGui::Separator();
                render_player_list();
                ImGui::Separator();
                render_chat_window();

                if (ImGui::CollapsingHeader("Advanced Settings")) {
                    render_settings();
                }

                ImGui::Spacing();
                ImGui::Spacing();

                render_lobby_buttons();

            } else {
                ImGui::TextWrapped("Create or join a netplay lobby.");
                ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                    "With lobby-first netplay, you can:");
                ImGui::BulletText("Host a session without loading a game first");
                ImGui::BulletText("Wait for players to join");
                ImGui::BulletText("Select the game together");
                ImGui::BulletText("Everyone loads their copy (CRC verified)");
                ImGui::BulletText("Start when all ready!");

                ImGui::Spacing();
                ImGui::Spacing();

                float button_width = 150;
                float spacing = ImGui::GetStyle().ItemSpacing.x;
                float total_width = button_width * 2 + spacing;
                float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
                if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

                if (ImGui::Button("Host Game", ImVec2(button_width, 40))) {
                    m_show_host_dialog = true;
                    m_session_code = SessionCode::generate();
                }

                ImGui::SameLine();

                if (ImGui::Button("Join Game", ImVec2(button_width, 40))) {
                    m_show_join_dialog = true;
                }

                // Recent connections
                if (!m_recent_connections.empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    render_recent_connections();
                }
            }
        }
        ImGui::End();
    }

    void render_connection_status() {
        ImVec4 status_color;
        const char* status_text;

        switch (m_connection_state) {
            case emu::NetplayConnectionState::Connected:
            case emu::NetplayConnectionState::Playing:
                status_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                status_text = m_lobby_state == LobbyState::Playing ? "Playing" : "In Lobby";
                break;
            case emu::NetplayConnectionState::Connecting:
            case emu::NetplayConnectionState::Synchronizing:
                status_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
                status_text = "Connecting...";
                break;
            default:
                status_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
                status_text = "Disconnected";
                break;
        }

        ImGui::TextColored(status_color, "Status: %s", status_text);

        int avg_ping = 0;
        int remote_count = 0;
        for (int i = 0; i < m_player_count; i++) {
            if (!m_player_info[i].base.is_local) {
                avg_ping += m_player_info[i].base.ping_ms;
                remote_count++;
            }
        }
        if (remote_count > 0) {
            avg_ping /= remote_count;
            ImGui::SameLine();
            ImVec4 ping_color = get_ping_color(avg_ping);
            ImGui::TextColored(ping_color, "[%dms %s]", avg_ping, get_ping_quality(avg_ping));
        }

        // Session code
        if (m_role == emu::NetplayRole::Host && m_session_code.valid) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "| Code: %s", m_session_code.code.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy")) {
                ImGui::SetClipboardText(m_session_code.code.c_str());
            }
        }
    }

    void render_lobby_status() {
        const char* lobby_text = "Unknown";
        ImVec4 lobby_color(0.7f, 0.7f, 0.7f, 1.0f);

        switch (m_lobby_state) {
            case LobbyState::WaitingForPlayers:
                lobby_text = "Waiting for players to join...";
                lobby_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
                break;
            case LobbyState::WaitingForGame:
                lobby_text = "Waiting for host to select game...";
                lobby_color = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
                break;
            case LobbyState::GameSelected:
                lobby_text = "Game selected - load your copy and ready up!";
                lobby_color = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
                break;
            case LobbyState::AllReady:
                lobby_text = "All players ready!";
                lobby_color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
                break;
            case LobbyState::Playing:
                lobby_text = "Game in progress";
                lobby_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
                break;
        }

        ImGui::TextColored(lobby_color, "%s", lobby_text);
    }

    void render_game_selection() {
        ImGui::Text("Game:");

        if (m_game_info.selected) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", m_game_info.name.c_str());

            ImGui::Text("Platform: %s | CRC: %s",
                m_game_info.platform.c_str(),
                crc32_to_string(m_game_info.crc32).c_str());
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("(No game selected)");

            if (m_role == emu::NetplayRole::Host) {
                ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f),
                    "Load a ROM (File > Open ROM) to select the game.");
            } else {
                ImGui::TextDisabled("Host will select the game.");
            }
        }

        // Show local ROM status for clients
        if (m_role == emu::NetplayRole::Client && m_game_info.selected) {
            bool rom_loaded = m_host && m_host->is_rom_loaded();

            ImGui::Spacing();
            if (!rom_loaded) {
                ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f),
                    "Load your copy of this ROM to continue.");
            } else {
                RomStatus status = m_player_info[m_local_player_id].rom_status;
                if (status == RomStatus::CrcMatch) {
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f),
                        "Your ROM matches!");
                } else if (status == RomStatus::CrcMismatch) {
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f),
                        "WARNING: Your ROM CRC doesn't match!");
                    ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f),
                        "You may experience desyncs. Load the correct version.");
                }
            }
        }
    }

    void render_player_list() {
        ImGui::Text("Players:");

        if (ImGui::BeginTable("PlayerList", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {

            ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ROM", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Ready", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableHeadersRow();

            for (int i = 0; i < m_player_count; i++) {
                const auto& player = m_player_info[i];

                ImGui::TableNextRow();

                if (player.base.is_local) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                        ImGui::GetColorU32(ImVec4(0.2f, 0.3f, 0.5f, 0.5f)));
                }

                ImGui::TableNextColumn();
                ImGui::Text("P%d", player.base.player_id + 1);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(player.base.name);
                if (player.base.is_local) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(you)");
                }
                if (player.base.role == emu::NetplayRole::Host) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "[H]");
                }

                // ROM status column
                ImGui::TableNextColumn();
                switch (player.rom_status) {
                    case RomStatus::NotLoaded:
                        ImGui::TextDisabled("-");
                        break;
                    case RomStatus::Loaded:
                        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "?");
                        break;
                    case RomStatus::CrcMatch:
                        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "OK");
                        break;
                    case RomStatus::CrcMismatch:
                        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "DIFF");
                        break;
                }

                ImGui::TableNextColumn();
                if (!player.base.is_local) {
                    ImVec4 ping_color = get_ping_color(player.base.ping_ms);
                    ImGui::TextColored(ping_color, "%d", player.base.ping_ms);
                } else {
                    ImGui::TextDisabled("-");
                }

                ImGui::TableNextColumn();
                if (player.base.is_ready) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "YES");
                } else {
                    ImGui::TextDisabled("...");
                }
            }

            ImGui::EndTable();
        }
    }

    void render_lobby_buttons() {
        float button_width = 120;
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        // Different buttons based on lobby state
        if (m_lobby_state == LobbyState::Playing) {
            // During gameplay, just show disconnect
            float offset = (ImGui::GetContentRegionAvail().x - button_width) * 0.5f;
            if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Disconnect", ImVec2(button_width, 0))) {
                disconnect();
            }
            ImGui::PopStyleColor();
        } else {
            // In lobby - show ready and disconnect, plus start for host
            int num_buttons = (m_role == emu::NetplayRole::Host && m_lobby_state == LobbyState::AllReady) ? 3 : 2;
            float total_width = button_width * num_buttons + spacing * (num_buttons - 1);
            float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
            if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            // Ready button
            bool can_ready = m_game_info.selected &&
                             m_player_info[m_local_player_id].rom_status == RomStatus::CrcMatch;

            if (m_is_ready) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                if (ImGui::Button("Ready!", ImVec2(button_width, 0))) {
                    set_ready(false);
                }
                ImGui::PopStyleColor();
            } else {
                if (!can_ready) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Ready", ImVec2(button_width, 0))) {
                    set_ready(true);
                }
                if (!can_ready) {
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("Load the correct ROM first");
                    }
                }
            }

            ImGui::SameLine();

            // Start Game button (host only, when all ready)
            if (m_role == emu::NetplayRole::Host && m_lobby_state == LobbyState::AllReady) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
                if (ImGui::Button("Start Game", ImVec2(button_width, 0))) {
                    start_game();
                }
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            // Disconnect button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Disconnect", ImVec2(button_width, 0))) {
                disconnect();
            }
            ImGui::PopStyleColor();
        }
    }

    void render_chat_window() {
        ImGui::Text("Chat:");

        float chat_height = 100.0f;
        ImGui::BeginChild("ChatHistory", ImVec2(0, chat_height), true,
            ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& msg : m_chat_messages) {
            std::string timestamp = format_timestamp(msg.timestamp);
            ImGui::TextDisabled("[%s]", timestamp.c_str());
            ImGui::SameLine();

            ImVec4 sender_color = msg.is_system ?
                ImVec4(0.7f, 0.7f, 0.7f, 1.0f) :
                get_player_color(msg.player_id);
            ImGui::TextColored(sender_color, "%s:", msg.sender.c_str());
            ImGui::SameLine();

            if (msg.is_system) {
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "%s", msg.message.c_str());
            } else {
                ImGui::TextWrapped("%s", msg.message.c_str());
            }
        }

        if (m_chat_scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            m_chat_scroll_to_bottom = false;
        }

        ImGui::EndChild();

        ImGui::PushItemWidth(-60);
        ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;
        if (ImGui::InputText("##ChatInput", m_chat_input, sizeof(m_chat_input), input_flags)) {
            if (strlen(m_chat_input) > 0) {
                send_chat_message(m_chat_input);
                m_chat_input[0] = '\0';
            }
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Send", ImVec2(50, 0))) {
            if (strlen(m_chat_input) > 0) {
                send_chat_message(m_chat_input);
                m_chat_input[0] = '\0';
            }
        }
    }

    void render_settings() {
        ImGui::SliderInt("Input Delay", &m_input_delay, 0, 10, "%d frames");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Frames of input delay before processing.\n"
                "Higher values reduce rollbacks but increase latency.");
        }

        ImGui::SliderInt("Max Rollback", &m_rollback_window, 0, 15, "%d frames");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Maximum frames to roll back for late inputs.\n"
                "Higher values handle worse connections but use more CPU.");
        }
    }

    void render_recent_connections() {
        ImGui::Text("Recent Connections:");

        for (size_t i = 0; i < m_recent_connections.size() && i < 5; i++) {
            const auto& conn = m_recent_connections[i];

            ImGui::PushID(static_cast<int>(i));

            std::ostringstream label;
            label << conn.name << " @ " << conn.ip << ":" << conn.port;

            if (ImGui::Button(label.str().c_str(), ImVec2(-1, 0))) {
                std::strncpy(m_join_ip, conn.ip.c_str(), sizeof(m_join_ip) - 1);
                m_join_port = conn.port;
                m_join_by_code = false;

                if (join_session(conn.ip.c_str(), static_cast<uint16_t>(conn.port), m_join_name)) {
                    m_show_panel = true;
                }
            }

            ImGui::PopID();
        }
    }

    void render_status_overlay() {
        ImGuiIO& io = ImGui::GetIO();
        float padding = 10.0f;
        ImVec2 window_pos(io.DisplaySize.x - padding, padding);
        ImVec2 pivot(1.0f, 0.0f);

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, pivot);
        ImGui::SetNextWindowBgAlpha(0.7f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("NetplayOverlay", nullptr, flags)) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "NETPLAY");

            for (int i = 0; i < m_player_count; i++) {
                const auto& player = m_player_info[i];
                if (player.base.is_local) {
                    ImGui::Text("P%d: %s (you)", i + 1, player.base.name);
                } else {
                    ImVec4 ping_color = get_ping_color(player.base.ping_ms);
                    ImGui::TextColored(ping_color, "P%d: %s %dms", i + 1, player.base.name, player.base.ping_ms);
                }
            }
        }
        ImGui::End();
    }

    // =========================================================================
    // Helpers
    // =========================================================================

    void add_chat_message(const std::string& sender, const std::string& message, int player_id) {
        ChatMessage msg;
        msg.sender = sender;
        msg.message = message;
        msg.timestamp = std::chrono::steady_clock::now();
        msg.player_id = player_id;
        msg.is_system = false;

        m_chat_messages.push_back(msg);
        while (m_chat_messages.size() > 100) {
            m_chat_messages.pop_front();
        }
        m_chat_scroll_to_bottom = true;
    }

    void add_system_message(const std::string& message) {
        ChatMessage msg;
        msg.sender = "System";
        msg.message = message;
        msg.timestamp = std::chrono::steady_clock::now();
        msg.player_id = -1;
        msg.is_system = true;

        m_chat_messages.push_back(msg);
        while (m_chat_messages.size() > 100) {
            m_chat_messages.pop_front();
        }
        m_chat_scroll_to_bottom = true;
    }

    std::string format_timestamp(const std::chrono::steady_clock::time_point& time) const {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch());
        int hours = static_cast<int>((elapsed.count() / 3600) % 24);
        int minutes = static_cast<int>((elapsed.count() / 60) % 60);
        int seconds = static_cast<int>(elapsed.count() % 60);

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
        return buf;
    }

    ImVec4 get_player_color(int player_id) const {
        static const ImVec4 colors[] = {
            ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
            ImVec4(1.0f, 0.6f, 0.4f, 1.0f),
            ImVec4(0.6f, 1.0f, 0.6f, 1.0f),
            ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
        };
        if (player_id >= 0 && player_id < 4) {
            return colors[player_id];
        }
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    ImVec4 get_ping_color(int ping_ms) const {
        if (ping_ms < 30) return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
        if (ping_ms < 60) return ImVec4(0.5f, 0.9f, 0.2f, 1.0f);
        if (ping_ms < 100) return ImVec4(0.9f, 0.9f, 0.2f, 1.0f);
        if (ping_ms < 150) return ImVec4(0.9f, 0.6f, 0.2f, 1.0f);
        return ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
    }

    const char* get_ping_quality(int ping_ms) const {
        if (ping_ms < 30) return "Excellent";
        if (ping_ms < 60) return "Good";
        if (ping_ms < 100) return "Fair";
        if (ping_ms < 150) return "Poor";
        return "Bad";
    }

    void add_recent_connection(const std::string& name, const std::string& ip, int port) {
        m_recent_connections.erase(
            std::remove_if(m_recent_connections.begin(), m_recent_connections.end(),
                [&](const RecentConnection& rc) {
                    return rc.ip == ip && rc.port == port;
                }),
            m_recent_connections.end());

        RecentConnection conn;
        conn.name = name;
        conn.ip = ip;
        conn.port = port;
        m_recent_connections.insert(m_recent_connections.begin(), conn);

        if (m_recent_connections.size() > 10) {
            m_recent_connections.resize(10);
        }
    }

    // =========================================================================
    // Settings Persistence
    // =========================================================================

    std::string get_settings_path() const {
        std::string config_dir;
        if (m_host) {
            config_dir = m_host->get_config_directory();
        } else {
            config_dir = (std::filesystem::current_path() / "config").string();
        }
        return (std::filesystem::path(config_dir) / "netplay.json").string();
    }

    void load_settings() {
        std::string path = get_settings_path();
        if (!std::filesystem::exists(path)) {
            return;
        }

        try {
            std::ifstream file(path);
            if (!file.is_open()) return;

            nlohmann::json json;
            file >> json;

            if (json.contains("player_name") && json["player_name"].is_string()) {
                std::string name = json["player_name"];
                std::strncpy(m_host_name, name.c_str(), sizeof(m_host_name) - 1);
                std::strncpy(m_join_name, name.c_str(), sizeof(m_join_name) - 1);
            }
            if (json.contains("default_port") && json["default_port"].is_number()) {
                m_host_port = json["default_port"];
                m_join_port = json["default_port"];
            }
            if (json.contains("input_delay") && json["input_delay"].is_number()) {
                m_input_delay = json["input_delay"];
            }
            if (json.contains("rollback_frames") && json["rollback_frames"].is_number()) {
                m_rollback_window = json["rollback_frames"];
            }
            if (json.contains("allow_spectators") && json["allow_spectators"].is_boolean()) {
                m_allow_spectators = json["allow_spectators"];
            }

            if (json.contains("recent_connections") && json["recent_connections"].is_array()) {
                m_recent_connections.clear();
                for (const auto& conn : json["recent_connections"]) {
                    if (conn.contains("name") && conn.contains("ip") && conn.contains("port")) {
                        RecentConnection rc;
                        rc.name = conn["name"];
                        rc.ip = conn["ip"];
                        rc.port = conn["port"];
                        m_recent_connections.push_back(rc);
                    }
                }
            }

            std::cout << "Loaded netplay settings from " << path << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading netplay settings: " << e.what() << std::endl;
        }
    }

    void save_settings() {
        std::string path = get_settings_path();

        try {
            std::filesystem::path config_path(path);
            if (config_path.has_parent_path()) {
                std::filesystem::create_directories(config_path.parent_path());
            }

            nlohmann::json json;
            json["player_name"] = m_host_name;
            json["default_port"] = m_host_port;
            json["input_delay"] = m_input_delay;
            json["rollback_frames"] = m_rollback_window;
            json["allow_spectators"] = m_allow_spectators;

            nlohmann::json recent = nlohmann::json::array();
            for (const auto& conn : m_recent_connections) {
                nlohmann::json c;
                c["name"] = conn.name;
                c["ip"] = conn.ip;
                c["port"] = conn.port;
                recent.push_back(c);
            }
            json["recent_connections"] = recent;

            std::ofstream file(path);
            if (file.is_open()) {
                file << json.dump(4);
                std::cout << "Saved netplay settings to " << path << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error saving netplay settings: " << e.what() << std::endl;
        }
    }

    // =========================================================================
    // Member Variables
    // =========================================================================

    // Host interface
    emu::INetplayHost* m_host = nullptr;
    bool m_initialized = false;

    // Session state
    emu::NetplayConnectionState m_connection_state = emu::NetplayConnectionState::Disconnected;
    emu::NetplayRole m_role = emu::NetplayRole::None;
    LobbyState m_lobby_state = LobbyState::WaitingForPlayers;
    std::string m_player_name = "Player";
    std::string m_host_address;
    uint16_t m_port = 7845;
    int m_local_player_id = 0;
    int m_player_count = 0;
    PlayerInfo m_player_info[4] = {};
    SessionCode m_session_code;
    GameInfo m_game_info;
    bool m_is_ready = false;

    // Settings
    int m_input_delay = 2;
    int m_rollback_window = 7;
    bool m_allow_spectators = false;

    // Input management
    emu::NetplayInputManager m_input_manager;
    int m_active_player_count = 2;

    // Rollback state
    bool m_is_rolling_back = false;
    int m_rollback_depth = 0;

    // GUI state
    bool m_show_host_dialog = false;
    bool m_show_join_dialog = false;
    bool m_show_panel = false;
    bool m_show_overlay = true;

    // Host dialog
    char m_host_name[64] = "Player";
    int m_host_port = 7845;
    bool m_use_session_code = true;

    // Join dialog
    char m_join_name[64] = "Player";
    char m_join_ip[64] = "127.0.0.1";
    char m_join_code[16] = "";
    int m_join_port = 7845;
    bool m_join_by_code = false;
    bool m_join_as_spectator = false;

    // Chat
    std::deque<ChatMessage> m_chat_messages;
    char m_chat_input[256] = "";
    bool m_chat_scroll_to_bottom = false;

    // Recent connections
    std::vector<RecentConnection> m_recent_connections;
};

} // anonymous namespace

// ============================================================================
// C interface
// ============================================================================

extern "C" {

EMU_PLUGIN_EXPORT emu::INetplayPlugin* create_netplay_plugin() {
    return new DefaultNetplayPlugin();
}

EMU_PLUGIN_EXPORT void destroy_netplay_plugin(emu::INetplayPlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_netplay_plugin_api_version() {
    return EMU_NETPLAY_PLUGIN_API_VERSION;
}

} // extern "C"
