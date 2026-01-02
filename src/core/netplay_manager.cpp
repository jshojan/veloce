#include "netplay_manager.hpp"
#include "plugin_manager.hpp"
#include "savestate_manager.hpp"
#include "input_manager.hpp"
#include "application.hpp"

#include <iostream>
#include <sstream>

namespace emu {

NetplayManager::NetplayManager() = default;

NetplayManager::~NetplayManager() {
    shutdown();
}

void NetplayManager::initialize(PluginManager* plugin_manager,
                                SavestateManager* savestate_manager,
                                InputManager* input_manager) {
    m_plugin_manager = plugin_manager;
    m_savestate_manager = savestate_manager;
    m_input_manager = input_manager;
    m_initialized = true;

    std::cout << "NetplayManager initialized" << std::endl;
}

void NetplayManager::shutdown() {
    if (!m_initialized) return;

    // Disconnect if active
    disconnect();

    m_plugin_manager = nullptr;
    m_savestate_manager = nullptr;
    m_input_manager = nullptr;
    m_initialized = false;
}

void NetplayManager::set_config(const NetplayConfig& config) {
    m_config = config;

    // Apply config to active netplay plugin if connected
    auto* netplay = m_plugin_manager ? m_plugin_manager->get_netplay_plugin() : nullptr;
    if (netplay && netplay->is_connected()) {
        netplay->set_input_delay(config.input_delay);
        netplay->set_rollback_window(config.rollback_window);
    }
}

// ============================================================================
// Session Management
// ============================================================================

bool NetplayManager::host_session(uint16_t port) {
    if (!m_initialized || !m_plugin_manager) {
        std::cerr << "NetplayManager: Not initialized" << std::endl;
        return false;
    }

    // Check for active emulator with ROM loaded
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator || !emulator->is_rom_loaded()) {
        std::cerr << "NetplayManager: No ROM loaded" << std::endl;
        return false;
    }

    // Check if emulator supports netplay
    if (!is_emulator_netplay_capable()) {
        std::cerr << "NetplayManager: Emulator does not support netplay" << std::endl;
        return false;
    }

    // Get netplay plugin
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (!netplay) {
        std::cerr << "NetplayManager: No netplay plugin active" << std::endl;
        return false;
    }

    // Cache ROM info
    m_rom_name = emulator->get_info().name;
    m_platform_name = emulator->get_info().name;

    // Initialize netplay plugin with this as host
    if (!netplay->initialize(this)) {
        std::cerr << "NetplayManager: Failed to initialize netplay plugin" << std::endl;
        return false;
    }

    // Apply configuration
    netplay->set_input_delay(m_config.input_delay);
    netplay->set_rollback_window(m_config.rollback_window);

    // Start hosting
    uint16_t actual_port = (port == 0) ? m_config.default_port : port;
    if (!netplay->host_session(actual_port, m_config.player_name.c_str(), m_config.public_session)) {
        std::cerr << "NetplayManager: Failed to host session" << std::endl;
        return false;
    }

    m_local_player_id = 0;  // Host is always player 0

    // Setup input manager for the session
    setup_input_manager_for_session();

    // Notify application to update cached netplay state
    get_application().update_netplay_cache();

    std::cout << "NetplayManager: Hosting session on port " << actual_port << std::endl;
    return true;
}

bool NetplayManager::join_session(const std::string& host, uint16_t port) {
    if (!m_initialized || !m_plugin_manager) {
        std::cerr << "NetplayManager: Not initialized" << std::endl;
        return false;
    }

    // Check for active emulator with ROM loaded
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator || !emulator->is_rom_loaded()) {
        std::cerr << "NetplayManager: No ROM loaded" << std::endl;
        return false;
    }

    // Check if emulator supports netplay
    if (!is_emulator_netplay_capable()) {
        std::cerr << "NetplayManager: Emulator does not support netplay" << std::endl;
        return false;
    }

    // Get netplay plugin
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (!netplay) {
        std::cerr << "NetplayManager: No netplay plugin active" << std::endl;
        return false;
    }

    // Cache ROM info
    m_rom_name = emulator->get_info().name;
    m_platform_name = emulator->get_info().name;

    // Initialize netplay plugin with this as host
    if (!netplay->initialize(this)) {
        std::cerr << "NetplayManager: Failed to initialize netplay plugin" << std::endl;
        return false;
    }

    // Apply configuration
    netplay->set_input_delay(m_config.input_delay);
    netplay->set_rollback_window(m_config.rollback_window);

    // Join session
    uint16_t actual_port = (port == 0) ? m_config.default_port : port;
    if (!netplay->join_session(host.c_str(), actual_port, m_config.player_name.c_str())) {
        std::cerr << "NetplayManager: Failed to join session at " << host << ":" << actual_port << std::endl;
        return false;
    }

    // Setup input manager for the session
    setup_input_manager_for_session();

    // Notify application to update cached netplay state
    get_application().update_netplay_cache();

    std::cout << "NetplayManager: Joining session at " << host << ":" << actual_port << std::endl;
    return true;
}

bool NetplayManager::join_by_code(const std::string& session_code) {
    if (!m_initialized || !m_plugin_manager) {
        return false;
    }

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (!netplay) {
        return false;
    }

    return netplay->join_session_by_code(session_code.c_str(), m_config.player_name.c_str());
}

void NetplayManager::disconnect() {
    if (!m_plugin_manager) return;

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (netplay && netplay->is_connected()) {
        netplay->disconnect();
        netplay->shutdown();
    }

    m_local_player_id = 0;
    m_local_input = 0;
    m_remote_input = 0;
    m_is_rolling_back = false;
    m_rollback_depth = 0;

    // Notify application to update cached netplay state
    get_application().update_netplay_cache();
}

bool NetplayManager::is_active() const {
    if (!m_plugin_manager) return false;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay && netplay->is_connected();
}

bool NetplayManager::is_host() const {
    if (!m_plugin_manager) return false;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay && netplay->is_host();
}

bool NetplayManager::is_connected() const {
    if (!m_plugin_manager) return false;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay && netplay->is_connected();
}

bool NetplayManager::is_playing() const {
    if (!m_plugin_manager) return false;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay && netplay->is_playing();
}

int NetplayManager::get_local_player_id() const {
    if (!m_plugin_manager) return 0;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_local_player_id() : 0;
}

NetplaySessionInfo NetplayManager::get_session_info() const {
    if (!m_plugin_manager) return {};
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_session_info() : NetplaySessionInfo{};
}

NetplayConnectionState NetplayManager::get_connection_state() const {
    if (!m_plugin_manager) return NetplayConnectionState::Disconnected;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_connection_state() : NetplayConnectionState::Disconnected;
}

void NetplayManager::set_ready(bool ready) {
    if (!m_plugin_manager) return;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (netplay) {
        netplay->set_ready(ready);
    }
}

int NetplayManager::get_player_count() const {
    if (!m_plugin_manager) return 0;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_player_count() : 0;
}

NetplayPlayer NetplayManager::get_player(int player_id) const {
    if (!m_plugin_manager) return {};
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_player(player_id) : NetplayPlayer{};
}

// ============================================================================
// Frame Processing
// ============================================================================

bool NetplayManager::begin_frame() {
    if (!is_active()) return true;  // Not in netplay, proceed normally

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (!netplay) return true;

    // Let netplay plugin handle frame start
    // This may trigger rollback if late inputs arrived
    if (!netplay->begin_frame()) {
        // Waiting for synchronization
        return false;
    }

    // Check if we need to rollback
    m_is_rolling_back = netplay->is_rolling_back();
    m_rollback_depth = netplay->get_current_rollback_depth();

    return true;
}

std::vector<uint32_t> NetplayManager::get_synchronized_inputs() {
    std::vector<uint32_t> inputs(m_active_player_count, 0);
    get_synchronized_inputs_fast(inputs);
    return inputs;
}

void NetplayManager::get_synchronized_inputs_fast(std::vector<uint32_t>& out_inputs) {
    // Ensure output buffer is correctly sized
    if (static_cast<int>(out_inputs.size()) != m_active_player_count) {
        out_inputs.resize(m_active_player_count, 0);
    }

    // Zero the buffer first
    for (int i = 0; i < m_active_player_count; i++) {
        out_inputs[i] = 0;
    }

    // Note: is_active() check is done by caller using cached value
    // This method is only called when netplay IS active

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (!netplay) {
        return;
    }

    auto* emulator = m_plugin_manager->get_emulator_plugin();
    uint64_t frame = emulator ? emulator->get_frame_count() : 0;

    // Get synchronized input for each player from netplay plugin
    for (int player = 0; player < m_active_player_count; player++) {
        uint32_t buttons = 0;
        netplay->get_input(player, buttons, frame);
        out_inputs[player] = buttons;
    }
}

bool NetplayManager::get_synchronized_input(int player, uint32_t& buttons) {
    if (!is_active()) {
        // Not in netplay - use local input for player 0 or from input manager
        if (player >= 0 && player < m_active_player_count) {
            buttons = m_netplay_input_manager.get_player_input(player);
        } else if (player == 0) {
            buttons = m_local_input;
        } else {
            buttons = 0;
        }
        return true;
    }

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (!netplay) {
        buttons = 0;
        return false;
    }

    auto* emulator = m_plugin_manager->get_emulator_plugin();
    uint64_t frame = emulator ? emulator->get_frame_count() : 0;

    // Get synchronized input from netplay plugin
    return netplay->get_input(player, buttons, frame);
}

void NetplayManager::end_frame() {
    if (!is_active()) return;

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (netplay) {
        netplay->end_frame();
    }

    m_is_rolling_back = false;
    m_rollback_depth = 0;
}

int NetplayManager::get_active_player_count() const {
    return m_active_player_count;
}

// ============================================================================
// Input Handling
// ============================================================================

void NetplayManager::set_local_input(uint32_t buttons) {
    m_local_input = buttons;

    if (!is_active()) return;

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (netplay && emulator) {
        uint64_t frame = emulator->get_frame_count();
        int local_id = netplay->get_local_player_id();
        netplay->send_input(local_id, buttons, frame);
    }
}

int NetplayManager::get_effective_input_delay() const {
    if (!is_active()) return 0;

    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_input_delay() : 0;
}

// ============================================================================
// Rollback Information
// ============================================================================

bool NetplayManager::is_rolling_back() const {
    return m_is_rolling_back;
}

int NetplayManager::get_rollback_depth() const {
    return m_rollback_depth;
}

NetplayStats NetplayManager::get_stats() const {
    if (!m_plugin_manager) return {};
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_stats() : NetplayStats{};
}

int NetplayManager::get_ping() const {
    if (!m_plugin_manager) return 0;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    return netplay ? netplay->get_ping() : 0;
}

// ============================================================================
// Event Callbacks
// ============================================================================

void NetplayManager::on_connected(NetplayEventCallback callback) {
    m_on_connected = std::move(callback);
}

void NetplayManager::on_disconnected(NetplayEventCallback callback) {
    m_on_disconnected = std::move(callback);
}

void NetplayManager::on_player_joined(NetplayEventCallback callback) {
    m_on_player_joined = std::move(callback);
}

void NetplayManager::on_player_left(NetplayEventCallback callback) {
    m_on_player_left = std::move(callback);
}

void NetplayManager::on_desync(NetplayEventCallback callback) {
    m_on_desync = std::move(callback);
}

void NetplayManager::on_chat(NetplayEventCallback callback) {
    m_on_chat = std::move(callback);
}

void NetplayManager::send_chat(const std::string& message) {
    if (!m_plugin_manager) return;
    auto* netplay = m_plugin_manager->get_netplay_plugin();
    if (netplay) {
        netplay->send_chat_message(message.c_str());
    }
}

// ============================================================================
// INetplayHost Implementation
// ============================================================================

void NetplayManager::pause_emulator() {
    // This would be called by the netplay plugin when needed
    // The actual pause is handled by Application
    std::cout << "NetplayManager: Pause requested" << std::endl;
}

void NetplayManager::resume_emulator() {
    std::cout << "NetplayManager: Resume requested" << std::endl;
}

bool NetplayManager::is_emulator_paused() const {
    // In practice, this would query Application
    return false;
}

void NetplayManager::reset_emulator() {
    if (!m_plugin_manager) return;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (emulator) {
        emulator->reset();
    }
}

uint64_t NetplayManager::get_frame_count() const {
    if (!m_plugin_manager) return 0;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    return emulator ? emulator->get_frame_count() : 0;
}

double NetplayManager::get_fps() const {
    if (!m_plugin_manager) return 60.0;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    return emulator ? emulator->get_info().native_fps : 60.0;
}

const char* NetplayManager::get_rom_name() const {
    return m_rom_name.c_str();
}

uint32_t NetplayManager::get_rom_crc32() const {
    if (!m_plugin_manager) return 0;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    return emulator ? emulator->get_rom_crc32() : 0;
}

const char* NetplayManager::get_platform_name() const {
    return m_platform_name.c_str();
}

bool NetplayManager::save_state_to_buffer(std::vector<uint8_t>& buffer) {
    if (!m_plugin_manager) return false;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator) return false;

    // Try fast path first if emulator supports it
    auto* netplay_capable = get_netplay_capable_emulator();
    if (netplay_capable) {
        size_t max_size = netplay_capable->get_max_state_size();
        buffer.resize(max_size);
        size_t actual_size = netplay_capable->save_state_fast(buffer.data(), buffer.size());
        if (actual_size > 0) {
            buffer.resize(actual_size);
            return true;
        }
    }

    // Fall back to standard save state
    return emulator->save_state(buffer);
}

bool NetplayManager::load_state_from_buffer(const std::vector<uint8_t>& buffer) {
    if (!m_plugin_manager) return false;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator) return false;

    // Try fast path first if emulator supports it
    auto* netplay_capable = get_netplay_capable_emulator();
    if (netplay_capable) {
        if (netplay_capable->load_state_fast(buffer.data(), buffer.size())) {
            return true;
        }
    }

    // Fall back to standard load state
    return emulator->load_state(buffer);
}

void NetplayManager::set_controller_input(int controller, uint32_t buttons) {
    if (controller == 0) {
        m_local_input = buttons;
    } else {
        m_remote_input = buttons;
    }
}

uint32_t NetplayManager::get_local_input(int controller) const {
    if (controller == m_local_player_id) {
        return m_local_input;
    }
    return 0;  // Remote input not available through this interface
}

void NetplayManager::on_netplay_connected(int player_id) {
    m_local_player_id = player_id;
    std::cout << "NetplayManager: Connected as player " << player_id << std::endl;

    if (m_on_connected) {
        std::ostringstream msg;
        msg << "Connected as player " << (player_id + 1);
        m_on_connected(msg.str());
    }
}

void NetplayManager::on_netplay_disconnected(const char* reason) {
    std::cout << "NetplayManager: Disconnected - " << (reason ? reason : "unknown") << std::endl;

    if (m_on_disconnected) {
        m_on_disconnected(reason ? reason : "Disconnected");
    }
}

void NetplayManager::on_netplay_player_joined(const NetplayPlayer& player) {
    std::cout << "NetplayManager: Player " << player.name << " joined as player " << player.player_id << std::endl;

    if (m_on_player_joined) {
        std::ostringstream msg;
        msg << player.name << " joined";
        m_on_player_joined(msg.str());
    }
}

void NetplayManager::on_netplay_player_left(int player_id, const char* reason) {
    std::cout << "NetplayManager: Player " << player_id << " left - " << (reason ? reason : "unknown") << std::endl;

    if (m_on_player_left) {
        std::ostringstream msg;
        msg << "Player " << (player_id + 1) << " left";
        if (reason) {
            msg << " (" << reason << ")";
        }
        m_on_player_left(msg.str());
    }
}

void NetplayManager::on_netplay_desync(const DesyncInfo& info) {
    std::cerr << "NetplayManager: DESYNC at frame " << info.frame
              << " (local: " << std::hex << info.local_checksum
              << ", remote: " << info.remote_checksum << ")" << std::dec << std::endl;

    if (m_on_desync) {
        std::ostringstream msg;
        msg << "Desync detected at frame " << info.frame;
        m_on_desync(msg.str());
    }
}

void NetplayManager::on_netplay_chat_message(int player_id, const char* message) {
    if (m_on_chat) {
        std::ostringstream msg;
        msg << "Player " << (player_id + 1) << ": " << message;
        m_on_chat(msg.str());
    }
}

// ============================================================================
// Private Helpers
// ============================================================================

bool NetplayManager::is_emulator_netplay_capable() const {
    return get_netplay_capable_emulator() != nullptr;
}

INetplayCapable* NetplayManager::get_netplay_capable_emulator() const {
    if (!m_plugin_manager) return nullptr;
    auto* emulator = m_plugin_manager->get_emulator_plugin();
    if (!emulator) return nullptr;

    // Try to cast to INetplayCapable
    // Emulator plugins that support netplay should inherit from both
    // IEmulatorPlugin and INetplayCapable
    return dynamic_cast<INetplayCapable*>(emulator);
}

void NetplayManager::perform_rollback(uint64_t target_frame) {
    // This is called by the netplay plugin when it needs to rollback
    // The actual implementation depends on how the rollback state buffer
    // is managed (by the plugin or by this manager)

    auto* netplay_capable = get_netplay_capable_emulator();
    if (!netplay_capable) return;

    m_is_rolling_back = true;

    // The netplay plugin handles the actual rollback using its state buffer
    // and calls load_state_from_buffer() as needed

    std::cout << "NetplayManager: Rolling back to frame " << target_frame << std::endl;
}

void NetplayManager::setup_input_manager_for_session() {
    // Query the emulator for max player count
    auto* netplay_capable = get_netplay_capable_emulator();
    int max_players = netplay_capable ? netplay_capable->get_max_players() : 2;

    // Get netplay plugin for session info
    auto* netplay = m_plugin_manager ? m_plugin_manager->get_netplay_plugin() : nullptr;

    // Determine active player count from session info or default to 2
    if (netplay && netplay->is_connected()) {
        auto session_info = netplay->get_session_info();
        m_active_player_count = session_info.player_count > 0 ? session_info.player_count : 2;
    } else {
        m_active_player_count = 2;  // Default to 2 players
    }

    // Clamp to emulator's maximum
    if (m_active_player_count > max_players) {
        m_active_player_count = max_players;
    }

    // Configure the input manager
    m_netplay_input_manager.set_max_players(m_active_player_count);
    m_netplay_input_manager.clear_assignments();

    // Setup default slot configuration based on role
    if (netplay && netplay->is_host()) {
        // Host: local player is slot 0, assign keyboard by default
        m_netplay_input_manager.assign_controller_to_slot(CONTROLLER_KEYBOARD, 0);
        m_netplay_input_manager.set_slot_local(0, true);

        // Other slots are remote by default
        for (int i = 1; i < m_active_player_count; i++) {
            m_netplay_input_manager.set_slot_local(i, false);
        }
    } else if (netplay) {
        // Client: get our local player ID from the netplay plugin
        int local_id = netplay->get_local_player_id();
        if (local_id >= 0 && local_id < m_active_player_count) {
            m_netplay_input_manager.assign_controller_to_slot(CONTROLLER_KEYBOARD, local_id);
            m_netplay_input_manager.set_slot_local(local_id, true);
        }

        // Other slots are remote
        for (int i = 0; i < m_active_player_count; i++) {
            if (i != local_id) {
                m_netplay_input_manager.set_slot_local(i, false);
            }
        }
    }

    std::cout << "NetplayManager: Input manager configured for " << m_active_player_count
              << " players (max: " << max_players << ")" << std::endl;
}

} // namespace emu
