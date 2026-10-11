#pragma once

#include "emu/input_types.hpp"
#include "emu/controller_layout.hpp"
#include <array>
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <utility>

union SDL_Event;
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

namespace emu {

class InputManager {
public:
    InputManager();
    ~InputManager();

    // Disable copy
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Initialize input system
    bool initialize();

    // Shutdown
    void shutdown();

    // Process SDL events for input
    void process_event(const SDL_Event& event);

    // Update input state (call once per frame)
    void update();

    // Get current input state for emulation
    uint32_t get_button_state(int port = 0) const;

    // Check if a specific button is pressed
    bool is_button_pressed(VirtualButton button) const;

    // Check if a button was just pressed this frame
    bool is_button_just_pressed(VirtualButton button) const;

    // Keyboard state
    bool is_key_pressed(int scancode) const;

    // Gamepad state (for input capture)
    bool is_gamepad_button_pressed(int controller_index, int button) const;
    float get_gamepad_axis(int controller_index, int axis) const;
    uint32_t encode_controller_input(int controller, uint32_t buttons) const;

    // Controller management
    int get_connected_controller_count() const;
    std::string get_controller_name(int index) const;
    int get_controller_instance_id(int index) const;

    // Get list of available controllers (id, name pairs)
    // Returns keyboard as (-1, "Keyboard") plus all connected gamepads
    std::vector<std::pair<int, std::string>> get_available_controllers() const;

    // Active controller selection (-1 for keyboard, 0+ for gamepad index)
    void set_active_controller(int controller_index);
    int get_active_controller() const { return m_active_controller; }
    // Secondary ports own a connection; -1 disconnects the port.
    void set_controller_for_port(int port, int controller_index);
    int get_controller_for_port(int port) const;

    // Input binding configuration
    void set_binding(VirtualButton button, const InputBinding& binding);
    const InputBinding* get_binding(VirtualButton button) const;
    void clear_binding(VirtualButton button);
    void load_default_bindings();
    void load_default_bindings_for_platform(const std::string& platform);

    // Current platform (for per-platform configs)
    void set_current_platform(const std::string& platform);
    const std::string& get_current_platform() const { return m_current_platform; }

    // Controller layout (set by emulator plugin when ROM loads)
    void set_controller_layout(const ControllerLayoutInfo* layout);
    const ControllerLayoutInfo* get_controller_layout() const { return m_controller_layout; }

    // Platform-specific configuration
    bool save_platform_config(const std::string& platform);
    bool load_platform_config(const std::string& platform);

    // Legacy save/load (uses current platform)
    bool save_config(const std::string& path);
    bool load_config(const std::string& path);

    // Get button name for display
    static std::string get_button_name(VirtualButton button);
    static std::string get_binding_display_name(const InputBinding& binding);

    // Get buttons available for a platform
    static std::vector<VirtualButton> get_platform_buttons(const std::string& platform);

    // Input capture mode - when true, game input is blocked
    // Used during controller mapping to prevent game from receiving inputs
    void set_input_capture_mode(bool capturing) { m_input_capture_mode = capturing; }
    bool is_input_capture_mode() const { return m_input_capture_mode; }

private:
    void open_controller(int device_index);
    void close_controller(int instance_id);
    void update_button_state();
    void restore_controller_bindings(const std::string& connected_guid = {});
    void restore_port_assignments(const std::string& connected_guid = {});
    bool is_secondary_controller(int instance_id) const;
    uint32_t default_gamepad_buttons(int index) const;
    std::string get_config_path(const std::string& platform) const;

    // Current button state bitmask
    uint32_t m_button_state = 0;
    uint32_t m_prev_button_state = 0;

    // Keyboard state
    const uint8_t* m_keyboard_state = nullptr;
    int m_keyboard_state_count = 0;

    // Connected controllers
    struct ControllerInfo {
        SDL_GameController* controller;
        int instance_id;
        std::string name;
        std::string guid;  // Device model; different physical pads may share it
        std::string serial;
    };
    std::vector<ControllerInfo> m_controllers;

    // Active controller (-1 for keyboard, or index into m_controllers)
    int m_active_controller = -1;

    // Current platform
    std::string m_current_platform = "NES";

    // Current controller layout (from emulator plugin)
    const ControllerLayoutInfo* m_controller_layout = nullptr;

    // Input bindings (current active bindings)
    std::unordered_map<VirtualButton, InputBinding> m_bindings;

    struct ControllerIdentity {
        std::string guid;
        std::string serial;
    };
    std::unordered_map<VirtualButton, ControllerIdentity> m_binding_identities;
    std::array<int, 4> m_port_instances{{-1,-1,-1,-1}};
    std::array<ControllerIdentity, 4> m_port_identities{};
    std::array<uint32_t, 4> m_port_buttons{};

    // Per-platform bindings cache
    std::map<std::string, std::unordered_map<VirtualButton, InputBinding>> m_platform_bindings;

    // Input capture mode - blocks game input during controller mapping
    bool m_input_capture_mode = false;
};

} // namespace emu
