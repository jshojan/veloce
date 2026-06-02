#include "input_manager.hpp"

#include <SDL.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>

namespace emu {

InputManager::InputManager() = default;

InputManager::~InputManager() {
    shutdown();
}

bool InputManager::initialize() {
    // Get keyboard state
    m_keyboard_state = SDL_GetKeyboardState(&m_keyboard_state_count);

    // Load default bindings
    load_default_bindings();

    // Open any connected controllers
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; i++) {
        if (SDL_IsGameController(i)) {
            open_controller(i);
        }
    }

    // Try to load saved config for current platform
    load_platform_config(m_current_platform);

    std::cout << "Input manager initialized with " << m_controllers.size()
              << " controller(s)" << std::endl;
    return true;
}

void InputManager::shutdown() {
    for (auto& controller : m_controllers) {
        if (controller.controller) {
            SDL_GameControllerClose(controller.controller);
        }
    }
    m_controllers.clear();
}

void InputManager::load_default_bindings() {
    // Default keyboard bindings for NES-style controller
    m_bindings[VirtualButton::Up] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_UP, 0, false};
    m_bindings[VirtualButton::Down] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_DOWN, 0, false};
    m_bindings[VirtualButton::Left] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_LEFT, 0, false};
    m_bindings[VirtualButton::Right] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_RIGHT, 0, false};
    m_bindings[VirtualButton::A] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_Z, 0, false};
    m_bindings[VirtualButton::B] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_X, 0, false};
    m_bindings[VirtualButton::Start] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_RETURN, 0, false};
    m_bindings[VirtualButton::Select] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_RSHIFT, 0, false};

    // Additional buttons for SNES-style
    m_bindings[VirtualButton::X] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_A, 0, false};
    m_bindings[VirtualButton::Y] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_S, 0, false};
    m_bindings[VirtualButton::L] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_Q, 0, false};
    m_bindings[VirtualButton::R] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_W, 0, false};
}

void InputManager::process_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED:
            open_controller(event.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            close_controller(event.cdevice.which);
            break;
    }
}

void InputManager::update() {
    m_prev_button_state = m_button_state;
    update_button_state();
}

void InputManager::update_button_state() {
    m_button_state = 0;

    // Don't update game input state during input capture mode
    // This prevents the game from receiving inputs while mapping controls
    if (m_input_capture_mode) {
        return;
    }

    // Check keyboard bindings
    for (const auto& [button, binding] : m_bindings) {
        bool pressed = false;

        if (binding.type == InputSourceType::Keyboard) {
            if (binding.code >= 0 && binding.code < m_keyboard_state_count) {
                pressed = m_keyboard_state[binding.code] != 0;
            }
        }
        else if (binding.type == InputSourceType::GamepadButton) {
            for (const auto& controller : m_controllers) {
                if (binding.device_id == -1 || binding.device_id == controller.instance_id) {
                    if (SDL_GameControllerGetButton(controller.controller,
                            static_cast<SDL_GameControllerButton>(binding.code))) {
                        pressed = true;
                        break;
                    }
                }
            }
        }
        else if (binding.type == InputSourceType::GamepadAxis) {
            for (const auto& controller : m_controllers) {
                if (binding.device_id == -1 || binding.device_id == controller.instance_id) {
                    int16_t value = SDL_GameControllerGetAxis(controller.controller,
                            static_cast<SDL_GameControllerAxis>(binding.code));
                    float normalized = value / 32767.0f;

                    if (binding.axis_positive && normalized > binding.axis_threshold) {
                        pressed = true;
                    } else if (!binding.axis_positive && normalized < -binding.axis_threshold) {
                        pressed = true;
                    }

                    if (pressed) break;
                }
            }
        }

        if (pressed) {
            m_button_state |= button_to_mask(button);
        }
    }

    // Hardcoded gamepad mapping for out-of-the-box convenience. This is only a
    // FALLBACK: if the user has configured any gamepad binding, we skip it so the
    // custom bindings are authoritative. Otherwise the fixed mapping below would
    // be OR'd on top of a remapped binding and fire two virtual buttons from one
    // physical button (e.g. a swapped A/B firing both jump and punch at once).
    bool has_gamepad_binding = false;
    for (const auto& [button, binding] : m_bindings) {
        (void)button;
        if (binding.type == InputSourceType::GamepadButton ||
            binding.type == InputSourceType::GamepadAxis) {
            has_gamepad_binding = true;
            break;
        }
    }

    for (const auto& controller : m_controllers) {
        if (has_gamepad_binding) break;
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
            m_button_state |= button_to_mask(VirtualButton::Up);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            m_button_state |= button_to_mask(VirtualButton::Down);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            m_button_state |= button_to_mask(VirtualButton::Left);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            m_button_state |= button_to_mask(VirtualButton::Right);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_A))
            m_button_state |= button_to_mask(VirtualButton::A);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_B))
            m_button_state |= button_to_mask(VirtualButton::B);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_X))
            m_button_state |= button_to_mask(VirtualButton::X);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_Y))
            m_button_state |= button_to_mask(VirtualButton::Y);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_START))
            m_button_state |= button_to_mask(VirtualButton::Start);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_BACK))
            m_button_state |= button_to_mask(VirtualButton::Select);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
            m_button_state |= button_to_mask(VirtualButton::L);
        if (SDL_GameControllerGetButton(controller.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
            m_button_state |= button_to_mask(VirtualButton::R);
    }
}

bool InputManager::is_button_pressed(VirtualButton button) const {
    return (m_button_state & button_to_mask(button)) != 0;
}

bool InputManager::is_button_just_pressed(VirtualButton button) const {
    uint32_t mask = button_to_mask(button);
    return (m_button_state & mask) && !(m_prev_button_state & mask);
}

bool InputManager::is_key_pressed(int scancode) const {
    if (scancode >= 0 && scancode < m_keyboard_state_count) {
        return m_keyboard_state[scancode] != 0;
    }
    return false;
}

void InputManager::open_controller(int device_index) {
    SDL_GameController* controller = SDL_GameControllerOpen(device_index);
    if (controller) {
        int instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        const char* name_ptr = SDL_GameControllerName(controller);
        std::string name = name_ptr ? name_ptr : "Unknown Controller";

        // Get the joystick GUID for more reliable duplicate detection
        SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
        SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
        char guid_str[33];
        SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

        // Check if this controller is already in our list (avoid duplicates)
        // On Windows, both XInput and DirectInput may detect the same controller
        // Check by instance_id first, then by GUID + name combination
        for (const auto& existing : m_controllers) {
            if (existing.instance_id == instance_id) {
                // Same instance ID - definitely a duplicate
                SDL_GameControllerClose(controller);
                return;
            }

            // Check if same GUID and name (likely same physical device via different API)
            if (existing.guid == guid_str && existing.name == name) {
                SDL_GameControllerClose(controller);
                std::cout << "Ignoring duplicate controller (same GUID): " << name << std::endl;
                return;
            }
        }

        ControllerInfo info;
        info.controller = controller;
        info.instance_id = instance_id;
        info.name = name;
        info.guid = guid_str;
        m_controllers.push_back(info);

        std::cout << "Controller connected: " << info.name << " (GUID: " << guid_str << ")" << std::endl;
    }
}

void InputManager::close_controller(int instance_id) {
    for (auto it = m_controllers.begin(); it != m_controllers.end(); ++it) {
        if (it->instance_id == instance_id) {
            std::cout << "Controller disconnected: " << it->name << std::endl;
            SDL_GameControllerClose(it->controller);
            m_controllers.erase(it);
            break;
        }
    }
}

int InputManager::get_connected_controller_count() const {
    return static_cast<int>(m_controllers.size());
}

std::string InputManager::get_controller_name(int index) const {
    if (index >= 0 && index < static_cast<int>(m_controllers.size())) {
        return m_controllers[index].name;
    }
    return "";
}

void InputManager::set_binding(VirtualButton button, const InputBinding& binding) {
    m_bindings[button] = binding;
}

const InputBinding* InputManager::get_binding(VirtualButton button) const {
    auto it = m_bindings.find(button);
    if (it != m_bindings.end()) {
        return &it->second;
    }
    return nullptr;
}

bool InputManager::save_config(const std::string& path) {
    return save_platform_config(m_current_platform);
}

bool InputManager::load_config(const std::string& path) {
    return load_platform_config(m_current_platform);
}

void InputManager::clear_binding(VirtualButton button) {
    m_bindings.erase(button);
}

void InputManager::set_active_controller(int controller_index) {
    m_active_controller = controller_index;
}

std::vector<std::pair<int, std::string>> InputManager::get_available_controllers() const {
    std::vector<std::pair<int, std::string>> result;

    // Keyboard is always available
    result.push_back({-1, "Keyboard"});

    // Add all connected gamepads
    for (size_t i = 0; i < m_controllers.size(); i++) {
        result.push_back({static_cast<int>(i), m_controllers[i].name});
    }

    return result;
}

int InputManager::get_controller_instance_id(int index) const {
    if (index >= 0 && index < static_cast<int>(m_controllers.size())) {
        return m_controllers[index].instance_id;
    }
    return -1;
}

bool InputManager::is_gamepad_button_pressed(int controller_index, int button) const {
    if (controller_index >= 0 && controller_index < static_cast<int>(m_controllers.size())) {
        return SDL_GameControllerGetButton(m_controllers[controller_index].controller,
            static_cast<SDL_GameControllerButton>(button)) != 0;
    }
    return false;
}

float InputManager::get_gamepad_axis(int controller_index, int axis) const {
    if (controller_index >= 0 && controller_index < static_cast<int>(m_controllers.size())) {
        int16_t value = SDL_GameControllerGetAxis(m_controllers[controller_index].controller,
            static_cast<SDL_GameControllerAxis>(axis));
        return value / 32767.0f;
    }
    return 0.0f;
}

void InputManager::set_current_platform(const std::string& platform) {
    if (m_current_platform != platform) {
        // Save current config before switching
        save_platform_config(m_current_platform);

        m_current_platform = platform;

        // Try to load config for new platform
        if (!load_platform_config(platform)) {
            // If no saved config, load defaults
            load_default_bindings_for_platform(platform);
        }
    }
}

void InputManager::set_controller_layout(const ControllerLayoutInfo* layout) {
    m_controller_layout = layout;

    // If we have a layout, update the current platform to match
    if (layout && layout->platform_name) {
        set_current_platform(layout->platform_name);
    }
}

void InputManager::load_default_bindings_for_platform(const std::string& platform) {
    m_bindings.clear();

    // Common D-pad bindings for all platforms
    m_bindings[VirtualButton::Up] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_UP, 0, false};
    m_bindings[VirtualButton::Down] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_DOWN, 0, false};
    m_bindings[VirtualButton::Left] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_LEFT, 0, false};
    m_bindings[VirtualButton::Right] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_RIGHT, 0, false};
    m_bindings[VirtualButton::Start] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_RETURN, 0, false};
    m_bindings[VirtualButton::Select] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_RSHIFT, 0, false};

    if (platform == "NES") {
        m_bindings[VirtualButton::A] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_Z, 0, false};
        m_bindings[VirtualButton::B] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_X, 0, false};
    } else if (platform == "SNES" || platform == "GB" || platform == "GBA") {
        m_bindings[VirtualButton::A] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_Z, 0, false};
        m_bindings[VirtualButton::B] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_X, 0, false};
        m_bindings[VirtualButton::X] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_A, 0, false};
        m_bindings[VirtualButton::Y] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_S, 0, false};
        m_bindings[VirtualButton::L] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_Q, 0, false};
        m_bindings[VirtualButton::R] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_W, 0, false};
    } else {
        // Default: NES-style
        m_bindings[VirtualButton::A] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_Z, 0, false};
        m_bindings[VirtualButton::B] = {InputSourceType::Keyboard, -1, SDL_SCANCODE_X, 0, false};
    }
}

std::string InputManager::get_config_path(const std::string& platform) const {
    std::string lowercase_platform = platform;
    std::transform(lowercase_platform.begin(), lowercase_platform.end(),
                   lowercase_platform.begin(), ::tolower);
    return "config/input_" + lowercase_platform + ".json";
}

bool InputManager::save_platform_config(const std::string& platform) {
    try {
        // Create config directory if it doesn't exist
        std::filesystem::create_directories("config");

        nlohmann::json j;
        j["platform"] = platform;
        j["active_controller"] = m_active_controller;

        nlohmann::json bindings_json;
        for (const auto& [button, binding] : m_bindings) {
            std::string button_name = get_button_name(button);
            nlohmann::json binding_json;

            switch (binding.type) {
                case InputSourceType::Keyboard:
                    binding_json["type"] = "Keyboard";
                    break;
                case InputSourceType::GamepadButton:
                    binding_json["type"] = "GamepadButton";
                    break;
                case InputSourceType::GamepadAxis:
                    binding_json["type"] = "GamepadAxis";
                    break;
            }

            binding_json["device_id"] = binding.device_id;
            binding_json["code"] = binding.code;
            binding_json["axis_threshold"] = binding.axis_threshold;
            binding_json["axis_positive"] = binding.axis_positive;

            bindings_json[button_name] = binding_json;
        }
        j["bindings"] = bindings_json;

        std::ofstream file(get_config_path(platform));
        if (!file) {
            std::cerr << "Failed to open config file for writing: " << get_config_path(platform) << std::endl;
            return false;
        }

        file << j.dump(2);
        std::cout << "Saved input config for " << platform << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving input config: " << e.what() << std::endl;
        return false;
    }
}

bool InputManager::load_platform_config(const std::string& platform) {
    try {
        std::ifstream file(get_config_path(platform));
        if (!file) {
            return false;  // No config file exists, use defaults
        }

        nlohmann::json j;
        file >> j;

        if (j.contains("active_controller")) {
            m_active_controller = j["active_controller"].get<int>();
        }

        if (j.contains("bindings")) {
            m_bindings.clear();

            for (auto& [button_name, binding_json] : j["bindings"].items()) {
                // Convert button name to enum
                VirtualButton button = VirtualButton::COUNT;
                for (int i = 0; i < static_cast<int>(VirtualButton::COUNT); i++) {
                    if (get_button_name(static_cast<VirtualButton>(i)) == button_name) {
                        button = static_cast<VirtualButton>(i);
                        break;
                    }
                }

                if (button == VirtualButton::COUNT) continue;

                InputBinding binding;
                std::string type_str = binding_json["type"].get<std::string>();
                if (type_str == "Keyboard") {
                    binding.type = InputSourceType::Keyboard;
                } else if (type_str == "GamepadButton") {
                    binding.type = InputSourceType::GamepadButton;
                } else if (type_str == "GamepadAxis") {
                    binding.type = InputSourceType::GamepadAxis;
                }

                binding.device_id = binding_json["device_id"].get<int>();
                binding.code = binding_json["code"].get<int>();
                binding.axis_threshold = binding_json.value("axis_threshold", 0.5f);
                binding.axis_positive = binding_json.value("axis_positive", true);

                m_bindings[button] = binding;
            }
        }

        std::cout << "Loaded input config for " << platform << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading input config: " << e.what() << std::endl;
        return false;
    }
}

std::string InputManager::get_button_name(VirtualButton button) {
    switch (button) {
        case VirtualButton::A: return "A";
        case VirtualButton::B: return "B";
        case VirtualButton::X: return "X";
        case VirtualButton::Y: return "Y";
        case VirtualButton::L: return "L";
        case VirtualButton::R: return "R";
        case VirtualButton::Start: return "Start";
        case VirtualButton::Select: return "Select";
        case VirtualButton::Up: return "Up";
        case VirtualButton::Down: return "Down";
        case VirtualButton::Left: return "Left";
        case VirtualButton::Right: return "Right";
        case VirtualButton::Menu: return "Menu";
        case VirtualButton::QuickSave: return "QuickSave";
        case VirtualButton::QuickLoad: return "QuickLoad";
        case VirtualButton::FrameAdvance: return "FrameAdvance";
        case VirtualButton::Pause: return "Pause";
        case VirtualButton::Reset: return "Reset";
        case VirtualButton::Rewind: return "Rewind";
        case VirtualButton::FastForward: return "FastForward";
        default: return "Unknown";
    }
}

std::string InputManager::get_binding_display_name(const InputBinding& binding) {
    if (binding.type == InputSourceType::Keyboard) {
        const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(binding.code));
        if (name && name[0] != '\0') {
            return std::string("Key: ") + name;
        }
        return "Key: " + std::to_string(binding.code);
    } else if (binding.type == InputSourceType::GamepadButton) {
        const char* name = SDL_GameControllerGetStringForButton(
            static_cast<SDL_GameControllerButton>(binding.code));
        if (name) {
            return std::string("Pad: ") + name;
        }
        return "Pad Button: " + std::to_string(binding.code);
    } else if (binding.type == InputSourceType::GamepadAxis) {
        const char* name = SDL_GameControllerGetStringForAxis(
            static_cast<SDL_GameControllerAxis>(binding.code));
        std::string dir = binding.axis_positive ? "+" : "-";
        if (name) {
            return std::string("Pad: ") + name + dir;
        }
        return "Pad Axis: " + std::to_string(binding.code) + dir;
    }
    return "Unbound";
}

std::vector<VirtualButton> InputManager::get_platform_buttons(const std::string& platform) {
    std::vector<VirtualButton> buttons;

    // D-pad is always present
    buttons.push_back(VirtualButton::Up);
    buttons.push_back(VirtualButton::Down);
    buttons.push_back(VirtualButton::Left);
    buttons.push_back(VirtualButton::Right);

    if (platform == "NES") {
        buttons.push_back(VirtualButton::A);
        buttons.push_back(VirtualButton::B);
        buttons.push_back(VirtualButton::Start);
        buttons.push_back(VirtualButton::Select);
    } else if (platform == "SNES") {
        buttons.push_back(VirtualButton::A);
        buttons.push_back(VirtualButton::B);
        buttons.push_back(VirtualButton::X);
        buttons.push_back(VirtualButton::Y);
        buttons.push_back(VirtualButton::L);
        buttons.push_back(VirtualButton::R);
        buttons.push_back(VirtualButton::Start);
        buttons.push_back(VirtualButton::Select);
    } else if (platform == "GB" || platform == "GBC") {
        buttons.push_back(VirtualButton::A);
        buttons.push_back(VirtualButton::B);
        buttons.push_back(VirtualButton::Start);
        buttons.push_back(VirtualButton::Select);
    } else if (platform == "GBA") {
        buttons.push_back(VirtualButton::A);
        buttons.push_back(VirtualButton::B);
        buttons.push_back(VirtualButton::L);
        buttons.push_back(VirtualButton::R);
        buttons.push_back(VirtualButton::Start);
        buttons.push_back(VirtualButton::Select);
    } else {
        // Default: NES-style
        buttons.push_back(VirtualButton::A);
        buttons.push_back(VirtualButton::B);
        buttons.push_back(VirtualButton::Start);
        buttons.push_back(VirtualButton::Select);
    }

    return buttons;
}

} // namespace emu
