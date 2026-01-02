#include "netplay_panel.hpp"
#include "notification_manager.hpp"
#include "gui_manager.hpp"
#include "core/application.hpp"
#include "core/netplay_manager.hpp"
#include "core/plugin_manager.hpp"
#include "core/paths_config.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace emu {

// ============================================================================
// SessionCode implementation
// ============================================================================

SessionCode SessionCode::generate() {
    SessionCode code;

    // Generate random alphanumeric code: ABC-123 format
    static const char* letters = "ABCDEFGHJKLMNPQRSTUVWXYZ";  // Omit I, O to avoid confusion
    static const char* digits = "0123456789";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> letter_dist(0, 23);  // 24 letters
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

    code.code = buf;
    code.valid = true;
    return code;
}

bool SessionCode::validate(const std::string& code) {
    // Format: AAA-NNN (7 characters with dash)
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

uint16_t SessionCode::to_port_offset() const {
    if (!valid || code.length() != 7) return 0;

    // Convert code to a port offset (0-999 range based on numeric part)
    uint16_t offset = 0;
    offset += (code[4] - '0') * 100;
    offset += (code[5] - '0') * 10;
    offset += (code[6] - '0');
    return offset;
}

// ============================================================================
// NetplayPanel implementation
// ============================================================================

NetplayPanel::NetplayPanel(Application& app)
    : m_app(app)
{
    // Initialize controller assignments for 2 players
    m_controller_assignments.resize(2, -1);  // -1 = none
    m_controller_assignments[0] = -2;  // Default: keyboard for player 1

    // Load settings on startup
    load_settings();
}

NetplayPanel::~NetplayPanel() {
    // Save settings on shutdown
    save_settings();
}

void NetplayPanel::render() {
    auto& netplay = m_app.get_netplay_manager();

    // Register callbacks for notifications if not already done
    if (!m_callbacks_registered) {
        auto& notifications = m_app.get_gui_manager().get_notification_manager();

        netplay.on_connected([this, &notifications](const std::string& msg) {
            notifications.success(msg, 3.0f);
            add_system_message(msg);
        });

        netplay.on_disconnected([this, &notifications](const std::string& msg) {
            std::string friendly_msg = get_user_friendly_error(msg);
            notifications.warning(friendly_msg, 3.0f);
            add_system_message(friendly_msg);
        });

        netplay.on_player_joined([this, &notifications](const std::string& msg) {
            notifications.info(msg, 3.0f);
            add_system_message(msg);
        });

        netplay.on_player_left([this, &notifications](const std::string& msg) {
            notifications.warning(msg, 3.0f);
            add_system_message(msg);
        });

        netplay.on_desync([this, &notifications](const std::string& msg) {
            notifications.error("Desync detected - resyncing...", 4.0f);
            add_system_message(msg);
        });

        netplay.on_chat([this](const std::string& msg) {
            // Parse "Player N: message" format
            size_t colon = msg.find(": ");
            if (colon != std::string::npos) {
                std::string sender = msg.substr(0, colon);
                std::string message = msg.substr(colon + 2);
                // Extract player number from "Player N"
                int player_id = -1;
                if (sender.find("Player ") == 0 && sender.length() > 7) {
                    player_id = std::atoi(sender.c_str() + 7) - 1;
                }
                add_chat_message(sender, message, player_id);
            } else {
                add_system_message(msg);
            }
        });

        m_callbacks_registered = true;
    }

    // Always check for dialogs
    if (m_show_host_dialog) {
        render_host_dialog();
    }

    if (m_show_join_dialog) {
        render_join_dialog();
    }

    // Main panel
    if (m_show_panel) {
        render_main_panel();
    }

    // Status overlay during active netplay
    if (m_show_status_overlay && netplay.is_active()) {
        render_status_overlay();
    }
}

void NetplayPanel::render_main_panel() {
    ImGui::SetNextWindowSize(ImVec2(420, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Netplay", &m_show_panel)) {
        auto& netplay = m_app.get_netplay_manager();

        if (netplay.is_active()) {
            // Connected state
            render_connection_status();
            ImGui::Separator();
            render_session_info();
            ImGui::Separator();
            render_player_list();
            ImGui::Separator();
            render_chat_window();
            ImGui::Separator();
            render_controller_assignment();

            if (ImGui::CollapsingHeader("Advanced Settings")) {
                render_settings();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // Ready / Disconnect buttons
            float button_width = 120;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float total_width = button_width * 2 + spacing;
            float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
            if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            if (m_is_ready) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                if (ImGui::Button("Ready!", ImVec2(button_width, 0))) {
                    m_is_ready = false;
                    netplay.set_ready(false);
                }
                ImGui::PopStyleColor();
            } else {
                if (ImGui::Button("Ready", ImVec2(button_width, 0))) {
                    m_is_ready = true;
                    netplay.set_ready(true);
                }
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Disconnect", ImVec2(button_width, 0))) {
                netplay.disconnect();
                m_is_ready = false;
                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                notifications.info("Disconnected from netplay session");
            }
            ImGui::PopStyleColor();

        } else {
            // Not connected state
            ImGui::TextWrapped("Connect to another player to start a netplay session.");
            ImGui::Spacing();
            ImGui::Spacing();

            // Check if ROM is loaded
            bool rom_loaded = m_app.get_plugin_manager().is_rom_loaded();

            if (!rom_loaded) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "Please load a ROM before starting netplay.");
                ImGui::Spacing();
            }

            float button_width = 150;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float total_width = button_width * 2 + spacing;
            float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
            if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            if (ImGui::Button("Host Game", ImVec2(button_width, 40)) && rom_loaded) {
                m_show_host_dialog = true;
                // Generate a new session code when opening host dialog
                m_session_code = SessionCode::generate();
            }

            ImGui::SameLine();

            if (ImGui::Button("Join Game", ImVec2(button_width, 40)) && rom_loaded) {
                m_show_join_dialog = true;
            }

            // Recent connections
            if (!m_recent_connections.empty()) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                render_recent_connections();
            }

            // Display error message if any
            if (!m_error_message.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", m_error_message.c_str());
            }
        }
    }
    ImGui::End();
}

void NetplayPanel::render_host_dialog() {
    ImGui::SetNextWindowSize(ImVec2(380, 320), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Host Game", &m_show_host_dialog, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Host a new netplay session");
        ImGui::Separator();
        ImGui::Spacing();

        // Player name
        ImGui::InputText("Your Name", m_host_name, sizeof(m_host_name));

        // Port
        ImGui::InputInt("Port", &m_host_port);
        if (m_host_port < 1024) m_host_port = 1024;
        if (m_host_port > 65535) m_host_port = 65535;

        // Session code option
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
                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                notifications.success("Session code copied to clipboard");
            }

            ImGui::TextDisabled("Share this code with your opponent");
        }

        // Allow spectators
        ImGui::Checkbox("Allow Spectators", &m_allow_spectators);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        float button_width = 100;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_width = button_width * 2 + spacing;
        float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
        if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        bool can_host = m_app.get_plugin_manager().is_rom_loaded();

        if (ImGui::Button("Start Host", ImVec2(button_width, 0)) && can_host) {
            auto& netplay = m_app.get_netplay_manager();

            // Configure the session
            NetplayConfig config;
            config.player_name = m_host_name;
            config.default_port = static_cast<uint16_t>(m_host_port);
            config.enable_spectators = m_allow_spectators;
            config.input_delay = m_input_delay;
            config.rollback_window = m_rollback_frames;
            netplay.set_config(config);

            if (netplay.host_session(static_cast<uint16_t>(m_host_port))) {
                m_show_host_dialog = false;
                m_show_panel = true;
                m_error_message.clear();

                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                std::ostringstream msg;
                msg << "Hosting on port " << m_host_port;
                if (m_use_session_code && m_session_code.valid) {
                    msg << " - Code: " << m_session_code.code;
                }
                notifications.success(msg.str(), 4.0f);
                add_system_message("Session started - waiting for players...");

                // Save settings
                save_settings();
            } else {
                m_error_message = get_user_friendly_error("Failed to start hosting. Port may be in use.");
                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                notifications.error(m_error_message);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            m_show_host_dialog = false;
        }

        if (!can_host) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Load a ROM first to host.");
        }
    }
    ImGui::End();
}

void NetplayPanel::render_join_dialog() {
    ImGui::SetNextWindowSize(ImVec2(380, 350), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Join Game", &m_show_join_dialog, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Join an existing netplay session");
        ImGui::Separator();
        ImGui::Spacing();

        // Player name
        ImGui::InputText("Your Name", m_join_name, sizeof(m_join_name));

        // Join method toggle
        ImGui::Checkbox("Join by Session Code", &m_join_by_code);

        if (m_join_by_code) {
            // Session code input
            ImGui::InputText("Session Code", m_join_code, sizeof(m_join_code));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enter the session code provided by the host (e.g., ABC-123)");
            }
        } else {
            // Host IP
            ImGui::InputText("Host IP", m_join_ip, sizeof(m_join_ip));

            // Port
            ImGui::InputInt("Port", &m_join_port);
            if (m_join_port < 1024) m_join_port = 1024;
            if (m_join_port > 65535) m_join_port = 65535;
        }

        // Join as spectator
        ImGui::Checkbox("Join as Spectator", &m_join_as_spectator);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        float button_width = 100;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float total_width = button_width * 2 + spacing;
        float offset = (ImGui::GetContentRegionAvail().x - total_width) * 0.5f;
        if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        bool can_join = m_app.get_plugin_manager().is_rom_loaded();

        // Validate session code if using it
        if (m_join_by_code && strlen(m_join_code) > 0) {
            std::string code_upper = m_join_code;
            // Convert to uppercase for validation
            for (char& c : code_upper) c = std::toupper(static_cast<unsigned char>(c));
            if (!SessionCode::validate(code_upper)) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                    "Invalid code format (use ABC-123)");
            }
        }

        if (ImGui::Button("Join", ImVec2(button_width, 0)) && can_join) {
            auto& netplay = m_app.get_netplay_manager();

            // Configure the session
            NetplayConfig config;
            config.player_name = m_join_name;
            config.input_delay = m_input_delay;
            config.rollback_window = m_rollback_frames;
            netplay.set_config(config);

            bool success = false;
            std::string connect_info;

            if (m_join_by_code) {
                // Join by session code
                std::string code = m_join_code;
                for (char& c : code) c = std::toupper(static_cast<unsigned char>(c));

                if (SessionCode::validate(code)) {
                    success = netplay.join_by_code(code);
                    connect_info = "session code " + code;
                } else {
                    m_error_message = "Invalid session code format";
                }
            } else {
                // Join by IP
                success = netplay.join_session(m_join_ip, static_cast<uint16_t>(m_join_port));
                connect_info = std::string(m_join_ip) + ":" + std::to_string(m_join_port);
            }

            if (success) {
                m_show_join_dialog = false;
                m_show_panel = true;
                m_error_message.clear();

                // Add to recent connections
                if (!m_join_by_code) {
                    add_recent_connection(m_join_name, m_join_ip, m_join_port);
                }

                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                notifications.info("Connecting to " + connect_info + "...", 3.0f);
                add_system_message("Connecting to session...");

                // Save settings
                save_settings();
            } else if (m_error_message.empty()) {
                m_error_message = get_user_friendly_error("Connection refused - check IP and port");
                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                notifications.error(m_error_message);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            m_show_join_dialog = false;
        }

        if (!can_join) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Load the same ROM as the host first.");
        }
    }
    ImGui::End();
}

void NetplayPanel::render_connection_status() {
    auto& netplay = m_app.get_netplay_manager();
    auto state = netplay.get_connection_state();

    ImVec4 status_color;
    std::string status_text = connection_state_to_string(static_cast<int>(state));

    switch (state) {
        case NetplayConnectionState::Connected:
        case NetplayConnectionState::Playing:
            status_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green
            break;
        case NetplayConnectionState::Connecting:
        case NetplayConnectionState::Synchronizing:
            status_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);  // Yellow
            break;
        case NetplayConnectionState::Desynced:
            status_color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);  // Red
            break;
        default:
            status_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);  // Gray
            break;
    }

    ImGui::TextColored(status_color, "Status: %s", status_text.c_str());

    // Connection quality indicator
    int ping = netplay.get_ping();
    ImGui::SameLine();
    render_connection_quality_bar(ping);
}

void NetplayPanel::render_connection_quality_bar(int ping_ms) {
    ImVec4 color = get_connection_quality_color(ping_ms);
    const char* quality_text;

    if (ping_ms < 0) {
        quality_text = "?";
    } else if (ping_ms < 30) {
        quality_text = "Excellent";
    } else if (ping_ms < 60) {
        quality_text = "Good";
    } else if (ping_ms < 100) {
        quality_text = "Fair";
    } else if (ping_ms < 150) {
        quality_text = "Poor";
    } else {
        quality_text = "Bad";
    }

    // Draw colored ping badge
    ImGui::TextColored(color, "[%s %s]", format_ping(ping_ms).c_str(), quality_text);
}

void NetplayPanel::render_session_info() {
    auto& netplay = m_app.get_netplay_manager();
    auto info = netplay.get_session_info();

    ImGui::Text("Game: %s", info.game_name);
    ImGui::Text("Platform: %s", info.platform);
    ImGui::Text("Players: %d / %d", info.player_count, info.max_players);

    if (netplay.is_host()) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "(You are the host)");

        // Show session code if we generated one
        if (m_session_code.valid) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "| Code: %s", m_session_code.code.c_str());
        }
    }
}

void NetplayPanel::render_player_list() {
    auto& netplay = m_app.get_netplay_manager();
    int player_count = netplay.get_player_count();

    ImGui::Text("Players:");

    if (ImGui::BeginTable("PlayerList", 5,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {

        ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableHeadersRow();

        for (int i = 0; i < player_count; i++) {
            auto player = netplay.get_player(i);

            ImGui::TableNextRow();

            // Highlight local player
            if (player.is_local) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(ImVec4(0.2f, 0.3f, 0.5f, 0.5f)));
            }

            // Slot number (P1, P2, etc.)
            ImGui::TableNextColumn();
            ImGui::Text("P%d", player.player_id + 1);

            // Name with typing indicator
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(player.name);
            if (player.is_local) {
                ImGui::SameLine();
                ImGui::TextDisabled("(you)");
            }
            // Show typing indicator for remote players
            // This would need to be tracked in NetplayPlayer struct

            // Ping with quality color
            ImGui::TableNextColumn();
            if (!player.is_local) {
                ImVec4 ping_color = get_connection_quality_color(player.ping_ms);
                ImGui::TextColored(ping_color, "%s %s",
                    get_connection_quality_icon(player.ping_ms),
                    format_ping(player.ping_ms).c_str());
            } else {
                ImGui::TextDisabled("-");
            }

            // Ready status with checkmark
            ImGui::TableNextColumn();
            if (player.is_ready) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "[OK]");
            } else {
                ImGui::TextDisabled("...");
            }

            // Kick button (host only, for non-local players)
            ImGui::TableNextColumn();
            if (netplay.is_host() && !player.is_local) {
                ImGui::PushID(i);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 0.8f));
                if (ImGui::SmallButton("X")) {
                    // TODO: Implement kick functionality in NetplayManager
                    auto& notifications = m_app.get_gui_manager().get_notification_manager();
                    notifications.warning("Kick functionality not yet implemented");
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Kick player");
                }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }
}

void NetplayPanel::render_chat_window() {
    ImGui::Text("Chat:");

    // Chat history
    float chat_height = 120.0f;
    ImGui::BeginChild("ChatHistory", ImVec2(0, chat_height), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& msg : m_chat_messages) {
        // Timestamp
        std::string timestamp = format_chat_timestamp(msg.timestamp);
        ImGui::TextDisabled("[%s]", timestamp.c_str());
        ImGui::SameLine();

        // Sender with color
        ImVec4 sender_color = msg.is_system ?
            ImVec4(0.7f, 0.7f, 0.7f, 1.0f) :
            get_player_chat_color(msg.player_id);
        ImGui::TextColored(sender_color, "%s:", msg.sender.c_str());
        ImGui::SameLine();

        // Message
        if (msg.is_system) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "%s", msg.message.c_str());
        } else {
            ImGui::TextWrapped("%s", msg.message.c_str());
        }
    }

    // Auto-scroll to bottom when new messages arrive
    if (m_chat_scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        m_chat_scroll_to_bottom = false;
    }

    ImGui::EndChild();

    // Chat input
    ImGui::PushItemWidth(-60);

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (ImGui::InputText("##ChatInput", m_chat_input, sizeof(m_chat_input), input_flags)) {
        if (strlen(m_chat_input) > 0) {
            auto& netplay = m_app.get_netplay_manager();
            netplay.send_chat(m_chat_input);

            // Add to local chat (will also come back via callback for remote confirmation)
            add_chat_message(m_app.get_netplay_manager().get_config().player_name,
                           m_chat_input,
                           netplay.get_local_player_id());

            m_chat_input[0] = '\0';
        }
        // Keep focus on input after sending
        ImGui::SetKeyboardFocusHere(-1);
    }

    m_chat_input_focused = ImGui::IsItemFocused();

    ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::Button("Send", ImVec2(50, 0))) {
        if (strlen(m_chat_input) > 0) {
            auto& netplay = m_app.get_netplay_manager();
            netplay.send_chat(m_chat_input);
            add_chat_message(netplay.get_config().player_name, m_chat_input, netplay.get_local_player_id());
            m_chat_input[0] = '\0';
        }
    }
}

void NetplayPanel::render_controller_assignment() {
    auto& netplay = m_app.get_netplay_manager();
    auto& input_manager = netplay.get_input_manager();

    ImGui::Text("Controller Assignment:");
    ImGui::TextDisabled("Assign your local controllers to player slots");

    // Ensure we have enough entries for current player count
    int max_players = input_manager.get_max_players();
    if (static_cast<int>(m_controller_assignments.size()) < max_players) {
        m_controller_assignments.resize(max_players, -1);
    }

    // Controller options: None, Keyboard, Gamepad 0, Gamepad 1, ...
    const char* controller_options[] = {
        "None",
        "Keyboard",
        "Gamepad 1",
        "Gamepad 2",
        "Gamepad 3",
        "Gamepad 4"
    };
    const int controller_values[] = { -1, -2, 0, 1, 2, 3 };
    const int num_options = 6;

    for (int slot = 0; slot < max_players; slot++) {
        // Only show assignment for local slots
        if (!input_manager.is_slot_local(slot)) {
            ImGui::Text("Player %d: (Remote)", slot + 1);
            continue;
        }

        char label[32];
        snprintf(label, sizeof(label), "Player %d", slot + 1);

        // Find current selection index
        int current_selection = 0;  // Default to "None"
        for (int i = 0; i < num_options; i++) {
            if (controller_values[i] == m_controller_assignments[slot]) {
                current_selection = i;
                break;
            }
        }

        ImGui::PushID(slot);
        if (ImGui::Combo(label, &current_selection, controller_options, num_options)) {
            int new_controller = controller_values[current_selection];
            m_controller_assignments[slot] = new_controller;

            // Update the netplay input manager
            if (new_controller == -1) {
                input_manager.unassign_slot(slot);
            } else {
                input_manager.assign_controller_to_slot(new_controller, slot);
            }
        }
        ImGui::PopID();
    }
}

void NetplayPanel::render_settings() {
    auto& netplay = m_app.get_netplay_manager();

    // Input delay slider
    ImGui::SliderInt("Input Delay", &m_input_delay, 0, 10, "%d frames");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Frames of input delay before processing.\n"
            "Higher values reduce rollbacks but increase latency.\n"
            "Recommended: 1-3 for good connections, 3-5 for worse connections.");
    }

    // Rollback frames slider
    ImGui::SliderInt("Max Rollback", &m_rollback_frames, 0, 15, "%d frames");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Maximum frames to roll back for late inputs.\n"
            "Higher values handle worse connections but use more CPU.\n"
            "Recommended: 6-8 for most cases.");
    }

    // Apply button
    if (ImGui::Button("Apply Settings")) {
        NetplayConfig config = netplay.get_config();
        config.input_delay = m_input_delay;
        config.rollback_window = m_rollback_frames;
        netplay.set_config(config);

        auto& notifications = m_app.get_gui_manager().get_notification_manager();
        notifications.success("Netplay settings applied");

        // Save settings
        save_settings();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(Changes apply immediately)");
}

void NetplayPanel::render_status_overlay() {
    auto& netplay = m_app.get_netplay_manager();

    // Position in top-right corner
    ImGuiIO& io = ImGui::GetIO();
    float padding = 10.0f;
    ImVec2 window_pos = ImVec2(io.DisplaySize.x - padding, padding);
    ImVec2 window_pos_pivot = ImVec2(1.0f, 0.0f);

    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("NetplayOverlay", nullptr, flags)) {
        // Connection state indicator with quality color
        auto state = netplay.get_connection_state();
        int ping = netplay.get_ping();
        ImVec4 state_color = get_connection_quality_color(ping);

        switch (state) {
            case NetplayConnectionState::Playing:
                // Use ping-based color
                break;
            case NetplayConnectionState::Desynced:
                state_color = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
                break;
            default:
                state_color = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
                break;
        }

        ImGui::TextColored(state_color, "NETPLAY");

        // Waiting indicator (show when we're ahead of remote player)
        auto stats = netplay.get_stats();
        if (stats.frame_advantage > 3.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Waiting for player...");
        }

        // Player list with pings
        int player_count = netplay.get_player_count();
        for (int i = 0; i < player_count; i++) {
            auto player = netplay.get_player(i);

            if (player.is_local) {
                ImGui::Text("P%d: %s (you)", i + 1, player.name);
            } else {
                ImVec4 ping_color = get_connection_quality_color(player.ping_ms);
                ImGui::TextColored(ping_color, "P%d: %s %s",
                    i + 1,
                    player.name,
                    format_ping(player.ping_ms).c_str());
            }
        }

        // Rollback depth indicator (only show if non-zero)
        int rollback_depth = netplay.get_rollback_depth();
        if (rollback_depth > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Rollback: %d", rollback_depth);
        }

        // Effective input delay
        int effective_delay = netplay.get_effective_input_delay();
        if (effective_delay > 0) {
            ImGui::TextDisabled("Delay: %df", effective_delay);
        }
    }
    ImGui::End();
}

void NetplayPanel::render_recent_connections() {
    ImGui::Text("Recent Connections:");

    for (size_t i = 0; i < m_recent_connections.size(); i++) {
        const auto& conn = m_recent_connections[i];

        ImGui::PushID(static_cast<int>(i));

        std::ostringstream label;
        label << conn.name << " @ " << conn.ip << ":" << conn.port;

        if (ImGui::Button(label.str().c_str(), ImVec2(-1, 0))) {
            // Quick connect
            std::strncpy(m_join_ip, conn.ip.c_str(), sizeof(m_join_ip) - 1);
            m_join_port = conn.port;
            m_join_by_code = false;

            // Start connection
            auto& netplay = m_app.get_netplay_manager();
            NetplayConfig config;
            config.player_name = m_join_name;
            config.input_delay = m_input_delay;
            config.rollback_window = m_rollback_frames;
            netplay.set_config(config);

            if (netplay.join_session(conn.ip, static_cast<uint16_t>(conn.port))) {
                m_show_panel = true;
                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                notifications.info("Connecting to " + conn.ip + "...");
            } else {
                auto& notifications = m_app.get_gui_manager().get_notification_manager();
                notifications.error("Failed to connect to " + conn.ip);
            }
        }

        ImGui::PopID();
    }
}

// ============================================================================
// Chat helpers
// ============================================================================

void NetplayPanel::add_chat_message(const std::string& sender, const std::string& message, int player_id) {
    ChatMessage msg;
    msg.sender = sender;
    msg.message = message;
    msg.timestamp = std::chrono::steady_clock::now();
    msg.player_id = player_id;
    msg.is_system = false;

    m_chat_messages.push_back(msg);

    // Limit history size
    while (m_chat_messages.size() > MAX_CHAT_HISTORY) {
        m_chat_messages.pop_front();
    }

    m_chat_scroll_to_bottom = true;
}

void NetplayPanel::add_system_message(const std::string& message) {
    ChatMessage msg;
    msg.sender = "System";
    msg.message = message;
    msg.timestamp = std::chrono::steady_clock::now();
    msg.player_id = -1;
    msg.is_system = true;

    m_chat_messages.push_back(msg);

    while (m_chat_messages.size() > MAX_CHAT_HISTORY) {
        m_chat_messages.pop_front();
    }

    m_chat_scroll_to_bottom = true;
}

std::string NetplayPanel::format_chat_timestamp(const std::chrono::steady_clock::time_point& time) const {
    // Get time since application start (steady_clock doesn't have wall time)
    // For simplicity, show relative time HH:MM:SS
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch());

    int hours = static_cast<int>((elapsed.count() / 3600) % 24);
    int minutes = static_cast<int>((elapsed.count() / 60) % 60);
    int seconds = static_cast<int>(elapsed.count() % 60);

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
    return buf;
}

ImVec4 NetplayPanel::get_player_chat_color(int player_id) const {
    // Different colors for different players
    static const ImVec4 player_colors[] = {
        ImVec4(0.4f, 0.8f, 1.0f, 1.0f),   // Player 1: Cyan
        ImVec4(1.0f, 0.6f, 0.4f, 1.0f),   // Player 2: Orange
        ImVec4(0.6f, 1.0f, 0.6f, 1.0f),   // Player 3: Green
        ImVec4(1.0f, 0.8f, 0.4f, 1.0f),   // Player 4: Yellow
    };

    if (player_id >= 0 && player_id < 4) {
        return player_colors[player_id];
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // Default white
}

// ============================================================================
// Helper functions
// ============================================================================

std::string NetplayPanel::connection_state_to_string(int state) const {
    switch (static_cast<NetplayConnectionState>(state)) {
        case NetplayConnectionState::Disconnected:
            return "Disconnected";
        case NetplayConnectionState::Connecting:
            return "Connecting...";
        case NetplayConnectionState::Connected:
            return "Connected";
        case NetplayConnectionState::Synchronizing:
            return "Synchronizing...";
        case NetplayConnectionState::Playing:
            return "Playing";
        case NetplayConnectionState::Desynced:
            return "DESYNC!";
        case NetplayConnectionState::Disconnecting:
            return "Disconnecting...";
        default:
            return "Unknown";
    }
}

std::string NetplayPanel::format_ping(int ping_ms) const {
    if (ping_ms < 0) {
        return "?ms";
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%dms", ping_ms);
    return buf;
}

const char* NetplayPanel::get_connection_quality_icon(int ping_ms) const {
    // Return a simple text indicator based on ping
    if (ping_ms < 0) return "[?]";
    if (ping_ms < 30) return "[***]";   // Excellent
    if (ping_ms < 60) return "[** ]";   // Good
    if (ping_ms < 100) return "[*  ]";  // Fair
    if (ping_ms < 150) return "[!  ]";  // Poor
    return "[!!!]";                      // Bad
}

ImVec4 NetplayPanel::get_connection_quality_color(int ping_ms) const {
    if (ping_ms < 0) {
        return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray - unknown
    }
    if (ping_ms < 30) {
        return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);  // Green - excellent
    }
    if (ping_ms < 60) {
        return ImVec4(0.5f, 0.9f, 0.2f, 1.0f);  // Yellow-green - good
    }
    if (ping_ms < 100) {
        return ImVec4(0.9f, 0.9f, 0.2f, 1.0f);  // Yellow - fair
    }
    if (ping_ms < 150) {
        return ImVec4(0.9f, 0.6f, 0.2f, 1.0f);  // Orange - poor
    }
    return ImVec4(0.9f, 0.2f, 0.2f, 1.0f);      // Red - bad
}

std::string NetplayPanel::get_user_friendly_error(const std::string& error) const {
    // Map common error messages to user-friendly versions
    if (error.find("Connection refused") != std::string::npos ||
        error.find("refused") != std::string::npos) {
        return "Connection refused - check IP and port";
    }
    if (error.find("timeout") != std::string::npos ||
        error.find("Timeout") != std::string::npos ||
        error.find("timed out") != std::string::npos) {
        return "Connection timed out - host may be offline";
    }
    if (error.find("closed") != std::string::npos ||
        error.find("reset") != std::string::npos) {
        return "Host closed the session";
    }
    if (error.find("version") != std::string::npos ||
        error.find("Version") != std::string::npos) {
        return "Version mismatch - update your emulator";
    }
    if (error.find("ROM") != std::string::npos ||
        error.find("rom") != std::string::npos ||
        error.find("CRC") != std::string::npos) {
        return "ROM mismatch - ensure both players have the same ROM";
    }
    if (error.find("port") != std::string::npos ||
        error.find("Port") != std::string::npos ||
        error.find("bind") != std::string::npos) {
        return "Port is already in use - try a different port";
    }
    // Return original if no match
    return error;
}

// ============================================================================
// Keyboard shortcuts
// ============================================================================

bool NetplayPanel::handle_keyboard_shortcut(int key, bool ctrl, bool shift, bool alt) {
    // F8: Toggle netplay panel
    if (key == TOGGLE_SHORTCUT_KEY && !ctrl && !shift && !alt) {
        toggle_panel();
        return true;
    }

    // Enter: Send chat message (when panel is visible and chat input is focused)
    // This is handled by ImGui's InputText with EnterReturnsTrue flag

    return false;
}

// ============================================================================
// Settings persistence
// ============================================================================

std::string NetplayPanel::get_settings_path() const {
    auto& paths = m_app.get_paths_config();
    std::filesystem::path config_dir = paths.get_config_directory();
    return (config_dir / "netplay.json").string();
}

void NetplayPanel::load_settings() {
    std::string path = get_settings_path();

    if (!std::filesystem::exists(path)) {
        return;  // No settings file yet
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) return;

        nlohmann::json json;
        file >> json;

        // Load settings
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
            m_rollback_frames = json["rollback_frames"];
        }
        if (json.contains("allow_spectators") && json["allow_spectators"].is_boolean()) {
            m_allow_spectators = json["allow_spectators"];
        }

        // Load recent connections
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

void NetplayPanel::save_settings() {
    std::string path = get_settings_path();

    try {
        // Ensure config directory exists
        std::filesystem::path config_path(path);
        if (config_path.has_parent_path()) {
            std::filesystem::create_directories(config_path.parent_path());
        }

        nlohmann::json json;

        // Save settings
        json["player_name"] = m_host_name;  // Use host name as primary
        json["default_port"] = m_host_port;
        json["input_delay"] = m_input_delay;
        json["rollback_frames"] = m_rollback_frames;
        json["allow_spectators"] = m_allow_spectators;

        // Save recent connections
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

void NetplayPanel::add_recent_connection(const std::string& name, const std::string& ip, int port) {
    // Remove existing entry with same IP and port
    m_recent_connections.erase(
        std::remove_if(m_recent_connections.begin(), m_recent_connections.end(),
            [&](const RecentConnection& rc) {
                return rc.ip == ip && rc.port == port;
            }),
        m_recent_connections.end());

    // Add to front
    RecentConnection conn;
    conn.name = name;
    conn.ip = ip;
    conn.port = port;
    conn.last_used = std::chrono::system_clock::now();

    m_recent_connections.insert(m_recent_connections.begin(), conn);

    // Limit size
    if (m_recent_connections.size() > MAX_RECENT_CONNECTIONS) {
        m_recent_connections.resize(MAX_RECENT_CONNECTIONS);
    }

    // Save immediately
    save_settings();
}

} // namespace emu
