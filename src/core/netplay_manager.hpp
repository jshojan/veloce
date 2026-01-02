#pragma once

#include "emu/netplay_plugin.hpp"
#include "emu/emulator_plugin.hpp"
#include "netplay_input_manager.hpp"
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace emu {

// Forward declarations
class PluginManager;
class SavestateManager;
class InputManager;

// Netplay session configuration
struct NetplayConfig {
    std::string player_name = "Player";
    uint16_t default_port = 7845;
    int input_delay = 2;          // Default input delay frames
    int rollback_window = 7;      // Maximum rollback frames
    bool enable_spectators = true;
    bool public_session = false;
};

// Callbacks for netplay events (for GUI notifications)
using NetplayEventCallback = std::function<void(const std::string& message)>;

// NetplayManager coordinates between the application, emulator plugin, and netplay plugin.
// It acts as the INetplayHost interface provider and handles:
// - Session management (host/join/disconnect)
// - Frame synchronization and rollback
// - Input routing through netplay
// - State synchronization for desync recovery
//
// The manager integrates with the main game loop in Application to ensure
// proper frame timing and input handling during netplay sessions.
//
class NetplayManager : public INetplayHost {
public:
    NetplayManager();
    ~NetplayManager();

    // Initialize with required subsystems
    void initialize(PluginManager* plugin_manager,
                   SavestateManager* savestate_manager,
                   InputManager* input_manager);
    void shutdown();

    // Configuration
    void set_config(const NetplayConfig& config);
    const NetplayConfig& get_config() const { return m_config; }

    // =========================================================================
    // Session Management
    // =========================================================================

    // Host a new netplay session
    // Returns: true if hosting started successfully
    bool host_session(uint16_t port = 0);

    // Join an existing session
    // Returns: true if connection attempt started
    bool join_session(const std::string& host, uint16_t port);

    // Join via session code (if matchmaking supported)
    bool join_by_code(const std::string& session_code);

    // Disconnect from current session
    void disconnect();

    // Session state queries
    bool is_active() const;
    bool is_host() const;
    bool is_connected() const;
    bool is_playing() const;
    int get_local_player_id() const;
    NetplaySessionInfo get_session_info() const;
    NetplayConnectionState get_connection_state() const;

    // Player management
    void set_ready(bool ready);
    int get_player_count() const;
    NetplayPlayer get_player(int player_id) const;

    // =========================================================================
    // Frame Processing
    // =========================================================================

    // Called at the START of each frame in the main game loop
    // This handles:
    // - Receiving remote inputs
    // - Detecting if rollback is needed
    // - Performing rollback and re-simulation if necessary
    //
    // Returns: true if frame should proceed, false if waiting for sync
    //
    // The normal game loop becomes:
    //   if (netplay_manager.begin_frame()) {
    //       auto inputs = netplay_manager.get_synchronized_inputs();
    //       emulator->run_frame_netplay_n(inputs);
    //       netplay_manager.end_frame();
    //   }
    //
    bool begin_frame();

    // Get synchronized inputs for all players
    // Returns a vector of inputs for each player slot (size = active player count)
    // Local player inputs come from assigned controllers
    // Remote player inputs come from network or prediction
    std::vector<uint32_t> get_synchronized_inputs();

    // Fast version: writes to pre-allocated buffer, avoiding allocations
    // out_inputs must be pre-sized to at least get_active_player_count() elements
    void get_synchronized_inputs_fast(std::vector<uint32_t>& out_inputs);

    // Legacy 2-player version for backward compatibility
    // Send local input and get synchronized input for a player
    // For local player: registers input and returns it
    // For remote player: returns received or predicted input
    //
    // player: Player ID (0 or 1)
    // buttons: Output - button state for this player
    // Returns: true if input is confirmed, false if predicted
    bool get_synchronized_input(int player, uint32_t& buttons);

    // Called at the END of each frame
    // Handles state saving for rollback, network updates, etc.
    void end_frame();

    // =========================================================================
    // N-Player Input Management
    // =========================================================================

    // Get the input manager for controller-to-slot assignment
    NetplayInputManager& get_input_manager() { return m_netplay_input_manager; }
    const NetplayInputManager& get_input_manager() const { return m_netplay_input_manager; }

    // Get active player count for current session
    int get_active_player_count() const;

    // =========================================================================
    // Input Handling
    // =========================================================================

    // Set local input for the current frame
    // Called by InputManager when polling local controller
    void set_local_input(uint32_t buttons);

    // Get the local player's input delay (for GUI display)
    int get_effective_input_delay() const;

    // =========================================================================
    // Rollback Information (for debugging/GUI)
    // =========================================================================

    // Check if currently in rollback
    bool is_rolling_back() const;

    // Get current rollback depth (0 if not rolling back)
    int get_rollback_depth() const;

    // Get network statistics
    NetplayStats get_stats() const;

    // Get ping to remote player(s)
    int get_ping() const;

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    // Register callback for connection events
    void on_connected(NetplayEventCallback callback);
    void on_disconnected(NetplayEventCallback callback);
    void on_player_joined(NetplayEventCallback callback);
    void on_player_left(NetplayEventCallback callback);
    void on_desync(NetplayEventCallback callback);
    void on_chat(NetplayEventCallback callback);

    // =========================================================================
    // Chat
    // =========================================================================

    void send_chat(const std::string& message);

    // =========================================================================
    // INetplayHost interface implementation
    // =========================================================================

    void pause_emulator() override;
    void resume_emulator() override;
    bool is_emulator_paused() const override;
    void reset_emulator() override;

    uint64_t get_frame_count() const override;
    double get_fps() const override;

    const char* get_rom_name() const override;
    uint32_t get_rom_crc32() const override;
    const char* get_platform_name() const override;

    bool save_state_to_buffer(std::vector<uint8_t>& buffer) override;
    bool load_state_from_buffer(const std::vector<uint8_t>& buffer) override;

    void set_controller_input(int controller, uint32_t buttons) override;
    uint32_t get_local_input(int controller) const override;

    void on_netplay_connected(int player_id) override;
    void on_netplay_disconnected(const char* reason) override;
    void on_netplay_player_joined(const NetplayPlayer& player) override;
    void on_netplay_player_left(int player_id, const char* reason) override;
    void on_netplay_desync(const DesyncInfo& info) override;
    void on_netplay_chat_message(int player_id, const char* message) override;

private:
    // Check if the active emulator supports netplay
    bool is_emulator_netplay_capable() const;
    INetplayCapable* get_netplay_capable_emulator() const;

    // Perform rollback and re-simulation
    void perform_rollback(uint64_t target_frame);

    // Initialize input manager based on emulator capabilities
    void setup_input_manager_for_session();

    // Subsystem references
    PluginManager* m_plugin_manager = nullptr;
    SavestateManager* m_savestate_manager = nullptr;
    InputManager* m_input_manager = nullptr;

    // N-player input management
    NetplayInputManager m_netplay_input_manager;

    // Configuration
    NetplayConfig m_config;

    // State
    bool m_initialized = false;
    uint32_t m_local_input = 0;      // Legacy: single local input
    uint32_t m_remote_input = 0;     // Legacy: single remote input
    int m_local_player_id = 0;
    bool m_is_rolling_back = false;
    int m_rollback_depth = 0;
    int m_active_player_count = 2;   // Current session player count

    // Cached ROM info (for INetplayHost)
    std::string m_rom_name;
    std::string m_platform_name;

    // Event callbacks
    NetplayEventCallback m_on_connected;
    NetplayEventCallback m_on_disconnected;
    NetplayEventCallback m_on_player_joined;
    NetplayEventCallback m_on_player_left;
    NetplayEventCallback m_on_desync;
    NetplayEventCallback m_on_chat;
};

} // namespace emu
