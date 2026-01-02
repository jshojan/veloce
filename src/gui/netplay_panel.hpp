#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <chrono>
#include <deque>

// Forward declare ImVec4 to avoid including imgui.h in header
struct ImVec4;

namespace emu {

class Application;

// Chat message structure
struct ChatMessage {
    std::string sender;          // Player name or "System"
    std::string message;         // Message content
    std::chrono::steady_clock::time_point timestamp;
    int player_id;               // -1 for system messages
    bool is_system;              // System message (join/leave/etc)
};

// Session code generator/validator
struct SessionCode {
    std::string code;            // e.g., "ABC-123"
    bool valid = false;

    // Generate a random session code
    static SessionCode generate();

    // Validate a session code format
    static bool validate(const std::string& code);

    // Convert code to numeric seed for port calculation
    uint16_t to_port_offset() const;
};

// Recent connection for quick reconnect
struct RecentConnection {
    std::string name;
    std::string ip;
    int port;
    std::chrono::system_clock::time_point last_used;
};

// Netplay panel for hosting/joining games and managing netplay sessions
class NetplayPanel {
public:
    explicit NetplayPanel(Application& app);
    ~NetplayPanel();

    // Render the panel - called each frame when visible
    void render();

    // Dialog controls (can be called from menu)
    void show_host_dialog() { m_show_host_dialog = true; }
    void show_join_dialog() { m_show_join_dialog = true; }

    // Panel visibility
    void show_panel(bool show) { m_show_panel = show; }
    bool is_panel_visible() const { return m_show_panel; }
    void toggle_panel() { m_show_panel = !m_show_panel; }

    // Status overlay is shown during gameplay
    void set_show_status_overlay(bool show) { m_show_status_overlay = show; }
    bool is_status_overlay_visible() const { return m_show_status_overlay; }

    // Handle keyboard shortcuts
    // Returns true if the event was handled
    bool handle_keyboard_shortcut(int key, bool ctrl, bool shift, bool alt);

    // Add a chat message (called from NetplayManager callbacks)
    void add_chat_message(const std::string& sender, const std::string& message, int player_id = -1);
    void add_system_message(const std::string& message);

    // Settings persistence
    void load_settings();
    void save_settings();

private:
    // Main panel sections
    void render_main_panel();
    void render_host_dialog();
    void render_join_dialog();
    void render_player_list();
    void render_controller_assignment();
    void render_settings();
    void render_status_overlay();  // Small overlay during gameplay
    void render_chat_window();     // Chat window

    // Helper functions
    void render_connection_status();
    void render_session_info();
    void render_connection_quality_bar(int ping_ms);
    std::string connection_state_to_string(int state) const;
    std::string format_ping(int ping_ms) const;
    const char* get_connection_quality_icon(int ping_ms) const;
    ImVec4 get_connection_quality_color(int ping_ms) const;
    std::string format_chat_timestamp(const std::chrono::steady_clock::time_point& time) const;
    ImVec4 get_player_chat_color(int player_id) const;

    // Error message helpers
    std::string get_user_friendly_error(const std::string& error) const;

    // Settings path helper
    std::string get_settings_path() const;

    // Recent connections management
    void add_recent_connection(const std::string& name, const std::string& ip, int port);
    void render_recent_connections();

    Application& m_app;

    // Panel visibility
    bool m_show_panel = false;
    bool m_show_status_overlay = true;  // Show during gameplay by default

    // Dialog state
    bool m_show_host_dialog = false;
    bool m_show_join_dialog = false;

    // Host dialog fields
    char m_host_name[64] = "Player 1";
    int m_host_port = 7845;
    bool m_allow_spectators = true;
    bool m_use_session_code = true;      // Generate session code for easy sharing
    SessionCode m_session_code;          // Generated session code

    // Join dialog fields
    char m_join_name[64] = "Player 2";
    char m_join_ip[64] = "127.0.0.1";
    char m_join_code[16] = "";           // Session code input
    int m_join_port = 7845;
    bool m_join_as_spectator = false;
    bool m_join_by_code = false;         // Join using code instead of IP

    // Settings
    int m_input_delay = 2;
    int m_rollback_frames = 7;

    // Controller assignment UI state
    // Maps player slot index to local controller selection
    // -2 = keyboard, -1 = none, 0+ = gamepad index
    std::vector<int> m_controller_assignments;

    // Chat state
    char m_chat_input[256] = "";
    std::deque<ChatMessage> m_chat_messages;
    static constexpr size_t MAX_CHAT_HISTORY = 100;
    bool m_chat_scroll_to_bottom = false;
    bool m_chat_input_focused = false;
    bool m_is_typing = false;            // For "is typing" indicator

    // Ready state
    bool m_is_ready = false;

    // Error message display
    std::string m_error_message;
    float m_error_display_time = 0.0f;

    // Recent connections
    std::vector<RecentConnection> m_recent_connections;
    static constexpr size_t MAX_RECENT_CONNECTIONS = 5;

    // Keyboard shortcut state
    static constexpr int TOGGLE_SHORTCUT_KEY = 293;  // F8 in SDL/ImGui

    // Notification callback registration flag
    bool m_callbacks_registered = false;
};

} // namespace emu
