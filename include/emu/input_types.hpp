#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace emu {

// Virtual button identifiers
enum class VirtualButton : uint32_t {
    A = 0,
    B,
    X,
    Y,
    L,
    R,
    Start,
    Select,
    Up,
    Down,
    Left,
    Right,
    // Special buttons for UI
    Menu,
    QuickSave,
    QuickLoad,
    FrameAdvance,
    Pause,
    Reset,
    Rewind,
    FastForward,
    // Append new controls to preserve existing input masks and bindings.
    Z, CUp, CDown, CLeft, CRight,
    COUNT
};

// Physical input source types
enum class InputSourceType {
    Keyboard,
    GamepadButton,
    GamepadAxis,
};

// Represents a physical input binding
struct InputBinding {
    InputSourceType type;
    int device_id;      // Gamepad index or -1 for keyboard
    int code;           // SDL scancode or gamepad button/axis
    float axis_threshold; // For axis bindings, the threshold to trigger
    bool axis_positive;   // For axis bindings, which direction

    bool operator==(const InputBinding& other) const {
        return type == other.type &&
               device_id == other.device_id &&
               code == other.code;
    }
};

// Controller configuration for a specific platform
struct ControllerConfig {
    std::string platform_name;  // "NES", "SNES", etc.
    std::unordered_map<VirtualButton, InputBinding> bindings;
};

// Input state snapshot
struct InputSnapshot {
    uint32_t buttons;           // Bitmask of pressed buttons
    int64_t frame_number;       // Frame when this input was recorded
};

// Convert VirtualButton to bitmask value
inline uint32_t button_to_mask(VirtualButton button) {
    return 1u << static_cast<uint32_t>(button);
}

// Check if button is pressed in bitmask
inline bool is_button_pressed(uint32_t buttons, VirtualButton button) {
    return (buttons & button_to_mask(button)) != 0;
}

} // namespace emu
