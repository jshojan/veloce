#include "netplay_input_manager.hpp"
#include <algorithm>

namespace emu {

NetplayInputManager::NetplayInputManager() {
    // Initialize all slots as unassigned and not local
    for (auto& slot : m_slots) {
        slot.controller_id = CONTROLLER_NONE;
        slot.is_local = false;
        slot.current_input = 0;
    }
}

// =============================================================================
// Configuration
// =============================================================================

void NetplayInputManager::set_max_players(int max) {
    if (max >= 1 && max <= MAX_NETPLAY_PLAYERS) {
        m_max_players = max;
    }
}

// =============================================================================
// Controller Assignment
// =============================================================================

void NetplayInputManager::assign_controller_to_slot(int controller_id, int player_slot) {
    if (player_slot < 0 || player_slot >= m_max_players) {
        return;
    }

    // First, unassign this controller from any other slot it might be in
    for (int i = 0; i < m_max_players; i++) {
        if (m_slots[i].controller_id == controller_id && i != player_slot) {
            m_slots[i].controller_id = CONTROLLER_NONE;
        }
    }

    // Assign to the new slot
    m_slots[player_slot].controller_id = controller_id;

    // If assigning a controller, the slot should be local
    if (controller_id != CONTROLLER_NONE) {
        m_slots[player_slot].is_local = true;
    }
}

void NetplayInputManager::unassign_slot(int player_slot) {
    if (player_slot >= 0 && player_slot < m_max_players) {
        m_slots[player_slot].controller_id = CONTROLLER_NONE;
        // Note: We don't change is_local here - that's managed separately
    }
}

void NetplayInputManager::clear_assignments() {
    for (int i = 0; i < m_max_players; i++) {
        m_slots[i].controller_id = CONTROLLER_NONE;
        m_slots[i].current_input = 0;
    }
    m_keyboard_input = 0;
}

int NetplayInputManager::get_controller_for_slot(int player_slot) const {
    if (player_slot >= 0 && player_slot < m_max_players) {
        return m_slots[player_slot].controller_id;
    }
    return CONTROLLER_NONE;
}

int NetplayInputManager::get_slot_for_controller(int controller_id) const {
    for (int i = 0; i < m_max_players; i++) {
        if (m_slots[i].controller_id == controller_id) {
            return i;
        }
    }
    return -1;  // Not assigned to any slot
}

bool NetplayInputManager::is_slot_assigned(int player_slot) const {
    if (player_slot >= 0 && player_slot < m_max_players) {
        return m_slots[player_slot].controller_id != CONTROLLER_NONE;
    }
    return false;
}

// =============================================================================
// Local/Remote Slot Management
// =============================================================================

void NetplayInputManager::set_slot_local(int player_slot, bool is_local) {
    if (player_slot >= 0 && player_slot < m_max_players) {
        m_slots[player_slot].is_local = is_local;
    }
}

bool NetplayInputManager::is_slot_local(int player_slot) const {
    if (player_slot >= 0 && player_slot < m_max_players) {
        return m_slots[player_slot].is_local;
    }
    return false;
}

std::vector<int> NetplayInputManager::get_local_slots() const {
    std::vector<int> local_slots;
    for (int i = 0; i < m_max_players; i++) {
        if (m_slots[i].is_local) {
            local_slots.push_back(i);
        }
    }
    return local_slots;
}

std::vector<int> NetplayInputManager::get_remote_slots() const {
    std::vector<int> remote_slots;
    for (int i = 0; i < m_max_players; i++) {
        if (!m_slots[i].is_local) {
            remote_slots.push_back(i);
        }
    }
    return remote_slots;
}

int NetplayInputManager::get_local_player_count() const {
    int count = 0;
    for (int i = 0; i < m_max_players; i++) {
        if (m_slots[i].is_local) {
            count++;
        }
    }
    return count;
}

// =============================================================================
// Input Handling
// =============================================================================

void NetplayInputManager::update_input(int controller_id, uint32_t buttons) {
    // Find which slot this controller is assigned to and update it
    int slot = get_slot_for_controller(controller_id);
    if (slot >= 0 && slot < m_max_players && m_slots[slot].is_local) {
        m_slots[slot].current_input = buttons;
    }
}

void NetplayInputManager::update_keyboard_input(uint32_t buttons) {
    m_keyboard_input = buttons;

    // Route to any slot that has keyboard assigned
    for (int i = 0; i < m_max_players; i++) {
        if (m_slots[i].controller_id == CONTROLLER_KEYBOARD && m_slots[i].is_local) {
            m_slots[i].current_input = buttons;
        }
    }
}

void NetplayInputManager::set_player_input(int player_slot, uint32_t buttons) {
    if (player_slot >= 0 && player_slot < m_max_players) {
        m_slots[player_slot].current_input = buttons;
    }
}

uint32_t NetplayInputManager::get_player_input(int player_slot) const {
    if (player_slot >= 0 && player_slot < m_max_players) {
        return m_slots[player_slot].current_input;
    }
    return 0;
}

std::vector<uint32_t> NetplayInputManager::get_all_player_inputs() const {
    std::vector<uint32_t> inputs;
    inputs.reserve(m_max_players);
    for (int i = 0; i < m_max_players; i++) {
        inputs.push_back(m_slots[i].current_input);
    }
    return inputs;
}

std::vector<std::pair<int, uint32_t>> NetplayInputManager::get_local_player_inputs() const {
    std::vector<std::pair<int, uint32_t>> inputs;
    for (int i = 0; i < m_max_players; i++) {
        if (m_slots[i].is_local) {
            inputs.emplace_back(i, m_slots[i].current_input);
        }
    }
    return inputs;
}

void NetplayInputManager::clear_inputs() {
    for (int i = 0; i < m_max_players; i++) {
        m_slots[i].current_input = 0;
    }
    m_keyboard_input = 0;
}

// =============================================================================
// Convenience Methods
// =============================================================================

void NetplayInputManager::setup_single_player(int controller_id) {
    clear_assignments();
    m_max_players = 1;
    assign_controller_to_slot(controller_id, 0);
    set_slot_local(0, true);
}

void NetplayInputManager::setup_two_player_local(int p1_controller, int p2_controller) {
    clear_assignments();
    m_max_players = 2;

    assign_controller_to_slot(p1_controller, 0);
    set_slot_local(0, true);

    assign_controller_to_slot(p2_controller, 1);
    set_slot_local(1, true);
}

void NetplayInputManager::setup_host_vs_remote() {
    clear_assignments();
    m_max_players = 2;

    // Host is player 0 with keyboard by default
    assign_controller_to_slot(CONTROLLER_KEYBOARD, 0);
    set_slot_local(0, true);

    // Remote player is player 1 (no local controller)
    set_slot_local(1, false);
}

} // namespace emu
