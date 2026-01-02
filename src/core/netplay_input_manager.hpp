#pragma once

#include <array>
#include <vector>
#include <cstdint>

namespace emu {

// Maximum number of players supported
static constexpr int MAX_NETPLAY_PLAYERS = 8;

// Special controller ID values
static constexpr int CONTROLLER_NONE = -1;
static constexpr int CONTROLLER_KEYBOARD = -2;

// NetplayInputManager handles mapping physical controllers to player slots
// for N-player netplay sessions. It tracks:
// - Which physical controller (or keyboard) is assigned to each player slot
// - Which player slots are controlled locally vs by remote players
// - Current input state for each player slot
//
// This allows flexible controller assignment where:
// - Multiple local players can use different controllers
// - Remote players' slots have no local controller assigned
// - The keyboard can be assigned to any slot
//
class NetplayInputManager {
public:
    NetplayInputManager();
    ~NetplayInputManager() = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    // Set maximum number of players for this session (1-8)
    void set_max_players(int max);
    int get_max_players() const { return m_max_players; }

    // =========================================================================
    // Controller Assignment
    // =========================================================================

    // Assign a physical controller to a player slot
    // controller_id: -2 = keyboard, 0+ = gamepad index
    // player_slot: 0 to max_players-1
    void assign_controller_to_slot(int controller_id, int player_slot);

    // Remove controller assignment from a slot
    void unassign_slot(int player_slot);

    // Clear all controller assignments
    void clear_assignments();

    // Get which controller is assigned to a slot
    // Returns: CONTROLLER_NONE (-1) = unassigned, CONTROLLER_KEYBOARD (-2) = keyboard, 0+ = gamepad
    int get_controller_for_slot(int player_slot) const;

    // Get which slot a controller is assigned to
    // Returns: -1 if not assigned to any slot, or 0+ for the slot index
    int get_slot_for_controller(int controller_id) const;

    // Check if a slot has a controller assigned
    bool is_slot_assigned(int player_slot) const;

    // =========================================================================
    // Local/Remote Slot Management
    // =========================================================================

    // Mark a slot as locally controlled or remote
    // Local slots: Input comes from local controllers
    // Remote slots: Input comes from network (no local controller should be assigned)
    void set_slot_local(int player_slot, bool is_local);

    // Check if a slot is controlled locally
    bool is_slot_local(int player_slot) const;

    // Get list of all locally controlled slot indices
    std::vector<int> get_local_slots() const;

    // Get list of all remote slot indices
    std::vector<int> get_remote_slots() const;

    // Get count of local players
    int get_local_player_count() const;

    // =========================================================================
    // Input Handling
    // =========================================================================

    // Update input for a specific controller (called by input manager)
    // This automatically routes the input to the correct player slot
    void update_input(int controller_id, uint32_t buttons);

    // Update keyboard input specifically
    void update_keyboard_input(uint32_t buttons);

    // Set input for a player slot directly (for remote players)
    void set_player_input(int player_slot, uint32_t buttons);

    // Get current input for a player slot
    uint32_t get_player_input(int player_slot) const;

    // Get inputs for all players as a vector
    // The vector has max_players entries, with remote players having
    // whatever input was set via set_player_input()
    std::vector<uint32_t> get_all_player_inputs() const;

    // Get inputs for only local players
    // Returns pairs of (player_slot, input)
    std::vector<std::pair<int, uint32_t>> get_local_player_inputs() const;

    // Clear all input states (e.g., at start of frame)
    void clear_inputs();

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    // Quick setup for common scenarios
    void setup_single_player(int controller_id);  // Slot 0 = local with given controller
    void setup_two_player_local(int p1_controller, int p2_controller);
    void setup_host_vs_remote();  // Slot 0 = local keyboard, slot 1 = remote

private:
    struct SlotMapping {
        int controller_id = CONTROLLER_NONE;  // Which controller is assigned
        bool is_local = false;                // Is this slot locally controlled?
        uint32_t current_input = 0;           // Current frame's input
    };

    std::array<SlotMapping, MAX_NETPLAY_PLAYERS> m_slots;
    int m_max_players = 2;
    uint32_t m_keyboard_input = 0;  // Cached keyboard input for routing
};

} // namespace emu
