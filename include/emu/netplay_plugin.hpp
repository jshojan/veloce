#pragma once

#include "plugin_types.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <functional>

#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_NETPLAY_PLUGIN_API_VERSION 1

namespace emu {

// ===========================================================================
// Constants for rollback netcode
// ===========================================================================

// Maximum rollback frames (should be enough for ~200ms at 60fps)
static constexpr int NETPLAY_MAX_ROLLBACK_FRAMES = 12;

// Maximum input delay frames
static constexpr int NETPLAY_MAX_INPUT_DELAY = 8;

// ===========================================================================
// INetplayCapable - Interface for emulator plugins that support netplay
// ===========================================================================
// Emulator plugins should inherit from this IN ADDITION to IEmulatorPlugin
// to indicate they support netplay features like deterministic emulation,
// fast save states for rollback, and multi-player input handling.

class INetplayCapable {
public:
    virtual ~INetplayCapable() = default;

    // -----------------------------------------------------------------------
    // Determinism
    // -----------------------------------------------------------------------

    // Check if the emulator core is deterministic (required for netplay)
    // A deterministic core guarantees: same initial state + same inputs = same output
    // This means:
    // - No uninitialized memory reads
    // - No floating point in core emulation logic (audio output is ok)
    // - No random number generation without explicit seeding
    // - No timing-based decisions (use cycle counts instead)
    virtual bool is_deterministic() const { return true; }

    // -----------------------------------------------------------------------
    // Multi-player Input
    // -----------------------------------------------------------------------

    // Run a single frame with multi-player input
    // This is the netplay-aware version of run_frame that accepts input
    // for all players simultaneously.
    //
    // player1_buttons: Input for player 1 (host) in platform-native format
    // player2_buttons: Input for player 2 (client) in platform-native format
    //
    // For NES, button format is: A, B, Select, Start, Up, Down, Left, Right
    // (same as controller shift register order)
    virtual void run_frame_netplay(uint32_t player1_buttons, uint32_t player2_buttons) = 0;

    // -----------------------------------------------------------------------
    // N-Player Netplay Support
    // -----------------------------------------------------------------------

    // Run a single frame with N-player input
    // This is the netplay-aware version for variable player counts.
    // The vector contains inputs for each player (index 0 = P1, etc.)
    //
    // player_inputs: Vector of button states for each player
    //
    // Default implementation delegates to 2-player version for backward compatibility.
    virtual void run_frame_netplay_n(const std::vector<uint32_t>& player_inputs) {
        uint32_t p1 = player_inputs.size() > 0 ? player_inputs[0] : 0;
        uint32_t p2 = player_inputs.size() > 1 ? player_inputs[1] : 0;
        run_frame_netplay(p1, p2);
    }

    // Get maximum number of players supported by this core
    // Override in emulator implementations to support more than 2 players.
    // Examples:
    //   - NES: 2 (standard), 4 (with Four Score/Satellite)
    //   - SNES: 2 (standard), 4-8 (with Multitap)
    //   - GBA: 4 (link cable)
    virtual int get_max_players() const { return 2; }

    // -----------------------------------------------------------------------
    // Fast Save State for Rollback
    // -----------------------------------------------------------------------
    // These methods are optimized for the rollback use case where states
    // are saved every frame and loaded frequently. They should:
    // - Avoid memory allocations (use pre-allocated buffers)
    // - Be as fast as possible (inline serialization)
    // - Include all state necessary for deterministic replay

    // Get the maximum state size needed (for pre-allocation)
    // This should return a conservative upper bound that never changes
    // for a given ROM.
    virtual size_t get_max_state_size() const = 0;

    // Fast save state for rollback
    // Writes the complete emulation state to the provided buffer.
    //
    // buffer: Pre-allocated buffer to write state to
    // buffer_size: Size of the buffer (must be >= get_max_state_size())
    //
    // Returns: Actual size of the saved state, or 0 on failure
    //
    // If buffer is nullptr, just returns the required size without saving.
    virtual size_t save_state_fast(uint8_t* buffer, size_t buffer_size) = 0;

    // Fast load state for rollback
    // Restores the complete emulation state from a previously saved buffer.
    //
    // buffer: Buffer containing saved state
    // size: Size of the saved state
    //
    // Returns: true on success, false on failure
    virtual bool load_state_fast(const uint8_t* buffer, size_t size) = 0;

    // -----------------------------------------------------------------------
    // Desync Detection
    // -----------------------------------------------------------------------

    // Get a hash of the current emulation state for desync detection
    // This should be a fast hash of the critical emulation state.
    // Two machines with the same state should produce the same hash.
    //
    // Returns: 64-bit hash of current state
    virtual uint64_t get_state_hash() const = 0;

    // -----------------------------------------------------------------------
    // Audio Handling for Rollback
    // -----------------------------------------------------------------------

    // Discard audio samples generated during rollback re-simulation
    // During rollback, we re-run frames but don't want to play their audio.
    // Call this after each re-simulated frame during rollback.
    virtual void discard_audio() {
        // Default implementation - override if needed
        // Most implementations can just clear their audio buffer
    }
};

// ===========================================================================
// Utility classes for netplay plugin implementations
// ===========================================================================

// Maximum number of players supported in netplay
static constexpr int NETPLAY_MAX_PLAYERS = 8;

// Input history for rollback netcode
// Stores recent inputs for all players to enable re-simulation
class InputHistory {
public:
    struct FrameInput {
        uint64_t frame = 0;
        std::array<uint32_t, NETPLAY_MAX_PLAYERS> player_inputs = {};  // Up to 8 players
        std::array<bool, NETPLAY_MAX_PLAYERS> player_confirmed = {};   // Confirmation status per player
        int player_count = 2;  // How many players are active in this session

        // Backward compatible accessors for 2-player code
        uint32_t player1() const { return player_inputs[0]; }
        uint32_t player2() const { return player_inputs[1]; }
        bool player1_confirmed() const { return player_confirmed[0]; }
        bool player2_confirmed() const { return player_confirmed[1]; }

        // Legacy field accessors (for compatibility)
        uint32_t player1_buttons() const { return player_inputs[0]; }
        uint32_t player2_buttons() const { return player_inputs[1]; }
    };

    explicit InputHistory(size_t max_frames = NETPLAY_MAX_ROLLBACK_FRAMES * 2, int player_count = 2)
        : m_player_count(player_count) {
        m_history.resize(max_frames);
    }

    void set_player_count(int count) {
        m_player_count = (count > 0 && count <= NETPLAY_MAX_PLAYERS) ? count : 2;
    }

    int get_player_count() const { return m_player_count; }

    void clear() {
        m_write_index = 0;
        m_count = 0;
        m_oldest_frame = 0;
    }

    // Legacy 2-player add_input for backward compatibility
    void add_input(uint64_t frame, uint32_t p1, uint32_t p2, bool p1_confirmed, bool p2_confirmed) {
        FrameInput input;
        input.frame = frame;
        input.player_inputs[0] = p1;
        input.player_inputs[1] = p2;
        input.player_confirmed[0] = p1_confirmed;
        input.player_confirmed[1] = p2_confirmed;
        input.player_count = 2;

        add_frame_input(input);
    }

    // N-player add_input
    void add_input_n(uint64_t frame, const std::vector<uint32_t>& inputs,
                     const std::vector<bool>& confirmed) {
        FrameInput input;
        input.frame = frame;
        input.player_count = m_player_count;

        for (int i = 0; i < m_player_count && i < static_cast<int>(inputs.size()); i++) {
            input.player_inputs[i] = inputs[i];
            input.player_confirmed[i] = (i < static_cast<int>(confirmed.size())) ? confirmed[i] : false;
        }

        add_frame_input(input);
    }

    bool get_input(uint64_t frame, FrameInput& out) const {
        if (m_count == 0) return false;

        for (size_t i = 0; i < m_count; i++) {
            size_t idx = (m_write_index + m_history.size() - 1 - i) % m_history.size();
            if (m_history[idx].frame == frame) {
                out = m_history[idx];
                return true;
            }
        }
        return false;
    }

    void confirm_input(uint64_t frame, int player, uint32_t buttons) {
        if (player < 0 || player >= NETPLAY_MAX_PLAYERS) return;

        for (size_t i = 0; i < m_count; i++) {
            size_t idx = (m_write_index + m_history.size() - 1 - i) % m_history.size();
            if (m_history[idx].frame == frame) {
                m_history[idx].player_inputs[player] = buttons;
                m_history[idx].player_confirmed[player] = true;
                return;
            }
        }
    }

    // Check if all players have confirmed input for a frame
    bool is_frame_fully_confirmed(uint64_t frame) const {
        FrameInput input;
        if (!get_input(frame, input)) return false;

        for (int i = 0; i < input.player_count; i++) {
            if (!input.player_confirmed[i]) return false;
        }
        return true;
    }

    uint64_t get_oldest_frame() const { return m_oldest_frame; }
    size_t get_count() const { return m_count; }

private:
    void add_frame_input(const FrameInput& input) {
        m_history[m_write_index] = input;
        m_write_index = (m_write_index + 1) % m_history.size();
        if (m_count < m_history.size()) {
            m_count++;
        } else {
            m_oldest_frame++;
        }
    }

    std::vector<FrameInput> m_history;
    size_t m_write_index = 0;
    size_t m_count = 0;
    uint64_t m_oldest_frame = 0;
    int m_player_count = 2;
};

// Save state ring buffer for rollback
// Pre-allocates states to avoid allocations during gameplay
class RollbackStateBuffer {
public:
    explicit RollbackStateBuffer(size_t max_state_size, size_t num_states = NETPLAY_MAX_ROLLBACK_FRAMES)
        : m_max_state_size(max_state_size), m_num_states(num_states) {
        m_states.resize(num_states);
        for (auto& state : m_states) {
            state.data.resize(max_state_size);
            state.valid = false;
            state.frame = 0;
            state.size = 0;
        }
    }

    void clear() {
        for (auto& state : m_states) {
            state.valid = false;
        }
        m_write_index = 0;
    }

    // Get a buffer to write a state for the given frame
    uint8_t* get_write_buffer(uint64_t frame) {
        m_states[m_write_index].frame = frame;
        m_states[m_write_index].valid = true;
        return m_states[m_write_index].data.data();
    }

    // Commit the write after save_state_fast returns
    void commit_write(size_t actual_size) {
        m_states[m_write_index].size = actual_size;
        m_write_index = (m_write_index + 1) % m_num_states;
    }

    // Find a state for the given frame
    const uint8_t* find_state(uint64_t frame, size_t& out_size) const {
        for (const auto& state : m_states) {
            if (state.valid && state.frame == frame) {
                out_size = state.size;
                return state.data.data();
            }
        }
        return nullptr;
    }

    // Find the newest valid state at or before the given frame
    const uint8_t* find_nearest_state(uint64_t frame, uint64_t& out_frame, size_t& out_size) const {
        const uint8_t* best = nullptr;
        uint64_t best_frame = 0;

        for (const auto& state : m_states) {
            if (state.valid && state.frame <= frame && state.frame > best_frame) {
                best = state.data.data();
                best_frame = state.frame;
                out_size = state.size;
            }
        }

        out_frame = best_frame;
        return best;
    }

    size_t get_max_state_size() const { return m_max_state_size; }

private:
    struct SavedState {
        std::vector<uint8_t> data;
        uint64_t frame;
        size_t size;
        bool valid;
    };

    std::vector<SavedState> m_states;
    size_t m_max_state_size;
    size_t m_num_states;
    size_t m_write_index = 0;
};

// Netplay plugin information
struct NetplayPluginInfo {
    const char* name;           // "GGPO Netplay", "Delay-Based Netplay", etc.
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description
    uint32_t capabilities;      // NetplayCapabilities flags
    int max_players;            // Maximum supported players (typically 2-4)
    int max_spectators;         // Maximum spectators (0 = not supported)
};

// Connection state for netplay session
enum class NetplayConnectionState {
    Disconnected,       // Not connected
    Connecting,         // Attempting to connect
    Connected,          // Connected and ready
    Synchronizing,      // Initial state synchronization
    Playing,            // Active gameplay
    Desynced,           // Desync detected
    Disconnecting       // Graceful disconnect in progress
};

// Player role in the session
enum class NetplayRole {
    None,               // Not in a session
    Host,               // Session host (player 1, authoritative)
    Client,             // Connected client
    Spectator           // Watch-only mode
};

// Player information
struct NetplayPlayer {
    int player_id;              // Player slot (0-3)
    char name[64];              // Display name
    NetplayRole role;           // Host, Client, or Spectator
    int ping_ms;                // Current latency in milliseconds
    bool is_local;              // True if this is the local player
    bool is_ready;              // Ready to start
};

// Session information
struct NetplaySessionInfo {
    char session_id[64];        // Unique session identifier (for matchmaking)
    char host_name[64];         // Host player name
    char game_name[256];        // ROM name being played
    uint32_t game_crc32;        // ROM CRC32 for verification
    char platform[32];          // "NES", "SNES", etc.
    int player_count;           // Current connected players
    int max_players;            // Maximum allowed players
    int spectator_count;        // Current spectators
    bool is_public;             // Listed in public lobbies
    int input_delay;            // Configured input delay frames
    int rollback_frames;        // Maximum rollback window
};

// Input frame data sent over network
struct NetplayInputFrame {
    uint64_t frame;             // Frame number
    int player_id;              // Which player this input is from
    uint32_t buttons;           // Button state bitmask
    uint32_t checksum;          // Optional state checksum for desync detection
};

// Rollback event information
struct RollbackEvent {
    uint64_t confirmed_frame;   // Last confirmed frame
    uint64_t rollback_frame;    // Frame we rolled back to
    int frames_resimulated;     // Number of frames re-simulated
};

// Desync information
struct DesyncInfo {
    uint64_t frame;             // Frame where desync occurred
    uint32_t local_checksum;    // Local state checksum
    uint32_t remote_checksum;   // Remote state checksum
    int player_id;              // Which player reported different state
};

// Network statistics
struct NetplayStats {
    int local_ping_ms;          // Ping to host/peer
    int remote_ping_ms;         // Remote player's ping (for host)
    int send_queue_size;        // Pending outbound messages
    int recv_queue_size;        // Pending inbound messages
    uint64_t bytes_sent;        // Total bytes sent
    uint64_t bytes_received;    // Total bytes received
    int rollback_count;         // Total rollbacks this session
    int max_rollback_frames;    // Maximum rollback depth seen
    float frame_advantage;      // Local frame advantage (-/+ frames)
};

// Notification types for UI
enum class NetplayNotificationType {
    Info,
    Success,
    Warning,
    Error
};

// Host interface provided to netplay plugins
class INetplayHost {
public:
    virtual ~INetplayHost() = default;

    // Emulation control
    virtual void pause_emulator() = 0;
    virtual void resume_emulator() = 0;
    virtual bool is_emulator_paused() const = 0;
    virtual void reset_emulator() = 0;

    // Frame information
    virtual uint64_t get_frame_count() const = 0;
    virtual double get_fps() const = 0;

    // ROM information
    virtual bool is_rom_loaded() const = 0;
    virtual const char* get_rom_name() const = 0;
    virtual uint32_t get_rom_crc32() const = 0;
    virtual const char* get_platform_name() const = 0;

    // Save state operations (for rollback)
    virtual bool save_state_to_buffer(std::vector<uint8_t>& buffer) = 0;
    virtual bool load_state_from_buffer(const std::vector<uint8_t>& buffer) = 0;

    // Input injection
    virtual void set_controller_input(int controller, uint32_t buttons) = 0;
    virtual uint32_t get_local_input(int controller) const = 0;

    // Configuration
    virtual const char* get_config_directory() const = 0;

    // UI notifications
    virtual void show_notification(NetplayNotificationType type, const char* message, float duration = 3.0f) = 0;

    // Notifications (callbacks to host)
    virtual void on_netplay_connected(int player_id) = 0;
    virtual void on_netplay_disconnected(const char* reason) = 0;
    virtual void on_netplay_player_joined(const NetplayPlayer& player) = 0;
    virtual void on_netplay_player_left(int player_id, const char* reason) = 0;
    virtual void on_netplay_desync(const DesyncInfo& info) = 0;
    virtual void on_netplay_chat_message(int player_id, const char* message) = 0;
};

// Callbacks for async operations
using ConnectCallback = std::function<void(bool success, const char* error)>;
using HostCallback = std::function<void(bool success, const char* session_code, const char* error)>;

// Main netplay plugin interface
//
// This interface supports two netplay models:
//
// 1. DELAY-BASED: Simple input delay where both players wait N frames
//    before inputs are processed. Low CPU usage but higher input latency.
//    - Good for stable connections, turn-based or slower games
//    - set_input_delay() controls the delay
//
// 2. ROLLBACK (GGPO-style): Speculative execution with state rollback
//    when remote inputs arrive late. Lower perceived latency at the cost
//    of CPU usage for re-simulation.
//    - Good for fighting games, action games, competitive play
//    - set_rollback_window() controls max rollback frames
//    - Requires fast save_state/load_state from emulator plugin
//
// The plugin implementation chooses which model to use based on its
// capabilities and configuration. Hybrid approaches are also possible
// (small delay + limited rollback).
//
class INetplayPlugin {
public:
    virtual ~INetplayPlugin() = default;

    // Plugin information
    virtual NetplayPluginInfo get_info() = 0;

    // Initialize with host interface
    virtual bool initialize(INetplayHost* host) = 0;
    virtual void shutdown() = 0;

    // =========================================================================
    // Session Management
    // =========================================================================

    // Host a new session
    // port: UDP port to listen on (0 = any available)
    // player_name: Display name for the host
    // is_public: List in public lobbies (if supported)
    // Returns: true if hosting started successfully
    virtual bool host_session(uint16_t port, const char* player_name, bool is_public = false) = 0;

    // Async version with callback
    virtual void host_session_async(uint16_t port, const char* player_name,
                                    bool is_public, HostCallback callback) {
        bool result = host_session(port, player_name, is_public);
        if (callback) callback(result, result ? get_session_code() : nullptr,
                               result ? nullptr : "Failed to host session");
    }

    // Join an existing session
    // host: IP address or hostname
    // port: UDP port
    // player_name: Display name for the joining player
    // Returns: true if connection started successfully
    virtual bool join_session(const char* host, uint16_t port, const char* player_name) = 0;

    // Join via session code (for relay/matchmaking servers)
    virtual bool join_session_by_code(const char* session_code, const char* player_name) {
        (void)session_code; (void)player_name;
        return false; // Not supported by default
    }

    // Async version with callback
    virtual void join_session_async(const char* host, uint16_t port,
                                    const char* player_name, ConnectCallback callback) {
        bool result = join_session(host, port, player_name);
        if (callback) callback(result, result ? nullptr : "Failed to connect");
    }

    // Disconnect from current session
    virtual void disconnect() = 0;

    // Get current connection state
    virtual NetplayConnectionState get_connection_state() const = 0;

    // Connection status helpers
    virtual bool is_connected() const {
        auto state = get_connection_state();
        return state == NetplayConnectionState::Connected ||
               state == NetplayConnectionState::Synchronizing ||
               state == NetplayConnectionState::Playing;
    }

    virtual bool is_playing() const {
        return get_connection_state() == NetplayConnectionState::Playing;
    }

    // Get current role
    virtual NetplayRole get_role() const = 0;

    virtual bool is_host() const {
        return get_role() == NetplayRole::Host;
    }

    // Get session information
    virtual NetplaySessionInfo get_session_info() const = 0;

    // Get session code for sharing (if supported)
    virtual const char* get_session_code() const { return nullptr; }

    // =========================================================================
    // Player Management
    // =========================================================================

    // Get local player ID (0-3)
    virtual int get_local_player_id() const = 0;

    // Get number of connected players
    virtual int get_player_count() const = 0;

    // Get player information
    virtual NetplayPlayer get_player(int player_id) const = 0;

    // Set ready state (game starts when all players ready)
    virtual void set_ready(bool ready) = 0;

    // Kick a player (host only)
    virtual bool kick_player(int player_id, const char* reason = nullptr) {
        (void)player_id; (void)reason;
        return false;
    }

    // =========================================================================
    // Input Synchronization
    // =========================================================================

    // Called at the START of each frame to get synchronized inputs
    // This is the main integration point with the emulation loop.
    //
    // For rollback netplay, this may trigger state loads and re-simulation
    // if remote inputs arrive for past frames.
    //
    // Returns: true if inputs are available and frame should proceed
    //          false if waiting for synchronization (pause emulation)
    virtual bool begin_frame() = 0;

    // Send local input for the current frame
    // player: Local player ID
    // buttons: Button state bitmask
    // frame: Current frame number
    virtual void send_input(int player, uint32_t buttons, uint64_t frame) = 0;

    // Get synchronized input for a player at a specific frame
    // For the current frame, this returns confirmed or predicted input.
    // For past frames during rollback, this returns confirmed input.
    //
    // player: Player ID (0-3)
    // buttons: Output - button state
    // frame: Frame number to get input for
    // Returns: true if input is confirmed, false if predicted
    virtual bool get_input(int player, uint32_t& buttons, uint64_t frame) = 0;

    // Called at the END of each frame
    // Handles state saving for rollback, network updates, etc.
    virtual void end_frame() = 0;

    // Get number of active players in the session
    virtual int get_active_player_count() const {
        auto info = get_session_info();
        return info.player_count > 0 ? info.player_count : 2;
    }

    // Get synchronized inputs for all players at once (efficient batch version)
    // out_inputs: Output buffer, should be pre-sized to get_active_player_count()
    // frame: Current frame number
    virtual void get_synchronized_inputs_fast(std::vector<uint32_t>& out_inputs, uint64_t frame) {
        int count = get_active_player_count();
        if (static_cast<int>(out_inputs.size()) != count) {
            out_inputs.resize(count, 0);
        }
        for (int i = 0; i < count; i++) {
            get_input(i, out_inputs[i], frame);
        }
    }

    // Set local input for a player slot (for local controller input routing)
    virtual void set_local_input(int player, uint32_t buttons) {
        (void)player; (void)buttons;
    }

    // =========================================================================
    // State Synchronization
    // =========================================================================

    // Request a full state sync (for recovery from desync)
    virtual void request_state_sync() = 0;

    // Send state to remote players (host broadcasts, or for sync recovery)
    virtual void send_state(const std::vector<uint8_t>& state, uint64_t frame) = 0;

    // Check if state sync is in progress
    virtual bool is_syncing() const {
        return get_connection_state() == NetplayConnectionState::Synchronizing;
    }

    // =========================================================================
    // Rollback Configuration
    // =========================================================================

    // Set input delay in frames (delay-based or hybrid mode)
    // Higher delay = more time for inputs to arrive, fewer rollbacks
    // Typical values: 0-4 frames
    virtual void set_input_delay(int frames) = 0;
    virtual int get_input_delay() const = 0;

    // Set maximum rollback window
    // How many frames back we can roll back to re-simulate
    // Larger window = handle worse connections, but more CPU usage
    // Typical values: 2-8 frames
    virtual void set_rollback_window(int frames) = 0;
    virtual int get_rollback_window() const = 0;

    // Get current rollback frame depth (0 if no rollback in progress)
    virtual int get_current_rollback_depth() const = 0;

    // Check if currently in a rollback/resimulation
    virtual bool is_rolling_back() const = 0;

    // =========================================================================
    // Network Statistics
    // =========================================================================

    // Get current network statistics
    virtual NetplayStats get_stats() const = 0;

    // Get ping to a specific player (or average if player_id == -1)
    virtual int get_ping(int player_id = -1) const = 0;

    // =========================================================================
    // Chat (Optional)
    // =========================================================================

    // Send chat message to all players
    virtual void send_chat_message(const char* message) {
        (void)message; // Optional feature
    }

    // =========================================================================
    // Spectator Support (Optional)
    // =========================================================================

    // Join as spectator
    virtual bool join_as_spectator(const char* host, uint16_t port, const char* name) {
        (void)host; (void)port; (void)name;
        return false;
    }

    // Get spectator count
    virtual int get_spectator_count() const { return 0; }

    // =========================================================================
    // Event Callbacks (Alternative to polling)
    // =========================================================================

    // Register callback for rollback events (for debugging/statistics)
    using RollbackCallback = std::function<void(const RollbackEvent&)>;
    virtual void on_rollback(RollbackCallback callback) { (void)callback; }

    // =========================================================================
    // Debug/Development
    // =========================================================================

    // Force a rollback for testing (debug builds only)
    virtual void debug_force_rollback(int frames) { (void)frames; }

    // Simulate packet loss (debug builds only)
    virtual void debug_set_packet_loss(float percent) { (void)percent; }

    // Simulate latency (debug builds only)
    virtual void debug_set_artificial_latency(int ms) { (void)ms; }

    // Get frame advantage (positive = ahead of remote)
    virtual float get_frame_advantage() const { return 0.0f; }

    // =========================================================================
    // GUI Integration
    // =========================================================================

    // Set ImGui context (must be called before render methods)
    virtual void set_imgui_context(void* context) { (void)context; }

    // Render the Netplay menu in the main menu bar
    // Called by the GUI manager when rendering the main menu.
    // The plugin is responsible for creating its own menu structure.
    // Returns true if a menu was rendered.
    virtual bool render_menu() { return false; }

    // Render any netplay-related windows/panels
    // Called each frame to render dialogs, overlays, status panels, etc.
    virtual void render_gui() {}

    // Show the host game dialog
    virtual void show_host_dialog() {}

    // Show the join game dialog
    virtual void show_join_dialog() {}

    // Panel visibility control (for Window menu toggle)
    virtual void show_panel(bool show) { (void)show; }
    virtual bool is_panel_visible() const { return false; }
};

} // namespace emu

// C interface for plugin loading
extern "C" {
    EMU_PLUGIN_EXPORT emu::INetplayPlugin* create_netplay_plugin();
    EMU_PLUGIN_EXPORT void destroy_netplay_plugin(emu::INetplayPlugin* plugin);
    EMU_PLUGIN_EXPORT uint32_t get_netplay_plugin_api_version();
}
