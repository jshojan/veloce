#include "input_manager.hpp"
#include "emu/n64_input.hpp"

#include <SDL.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace emu {

InputManager::InputManager() = default;

InputManager::~InputManager() {
    shutdown();
}

bool InputManager::initialize() {
    // Get keyboard state
    m_keyboard_state = SDL_GetKeyboardState(&m_keyboard_state_count);

    // Load default bindings
    load_default_bindings_for_platform(m_current_platform);
    reset_secondary_profiles();

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
    m_active_controller = -1;
    m_port_instances.fill(-1);
    m_port_identities = {};
    m_port_buttons.fill(0);
    m_button_state = m_prev_button_state = 0;
    m_keyboard_state = nullptr;
    m_keyboard_state_count = 0;
}

void InputManager::load_default_bindings() {
    m_bindings.clear();
    m_binding_identities.clear();
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
    m_port_buttons.fill(0);

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
                if (!is_secondary_controller(controller.instance_id) &&
                    (binding.device_id == -1 || binding.device_id == controller.instance_id)) {
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
                if (!is_secondary_controller(controller.instance_id) &&
                    (binding.device_id == -1 || binding.device_id == controller.instance_id)) {
                    int16_t value = SDL_GameControllerGetAxis(controller.controller,
                            static_cast<SDL_GameControllerAxis>(binding.code));
                    float normalized = std::clamp(value / 32767.0f, -1.0f, 1.0f);

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

    if (!has_gamepad_binding) {
        for (int i=0;i<get_connected_controller_count();++i) {
            if (!is_secondary_controller(m_controllers[i].instance_id) &&
                (m_active_controller<0 || i==m_active_controller))
                m_button_state |= default_gamepad_buttons(i);
        }
    }
    m_port_buttons[0] = m_button_state;
    for (int port=1;port<4;++port)
        m_port_buttons[port] = secondary_buttons(port);
}

uint32_t InputManager::get_button_state(int port) const {
    return port>=0 && port<4 && !m_input_capture_mode ? m_port_buttons[port] : 0;
}

bool InputManager::is_secondary_controller(int instance) const {
    return std::find(m_port_instances.begin()+1,m_port_instances.end(),instance)!=m_port_instances.end();
}

uint32_t InputManager::default_gamepad_buttons(int index) const {
    if (index<0 || index>=get_connected_controller_count()) return 0;
    uint32_t buttons=0;
    const std::pair<SDL_GameControllerButton,VirtualButton> mapping[] = {
        {SDL_CONTROLLER_BUTTON_DPAD_UP,VirtualButton::Up},
        {SDL_CONTROLLER_BUTTON_DPAD_DOWN,VirtualButton::Down},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT,VirtualButton::Left},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT,VirtualButton::Right},
        {SDL_CONTROLLER_BUTTON_A,VirtualButton::A},{SDL_CONTROLLER_BUTTON_B,VirtualButton::B},
        {SDL_CONTROLLER_BUTTON_X,VirtualButton::X},{SDL_CONTROLLER_BUTTON_Y,VirtualButton::Y},
        {SDL_CONTROLLER_BUTTON_START,VirtualButton::Start},{SDL_CONTROLLER_BUTTON_BACK,VirtualButton::Select},
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,VirtualButton::L},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,VirtualButton::R}};
    for (const auto& [physical,logical]:mapping)
        if (is_gamepad_button_pressed(index,physical)) buttons |= button_to_mask(logical);
    if (m_current_platform=="N64") {
        if (get_gamepad_axis(index,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>.5f) buttons |= button_to_mask(VirtualButton::Z);
        const auto x=get_gamepad_axis(index,SDL_CONTROLLER_AXIS_RIGHTX),y=get_gamepad_axis(index,SDL_CONTROLLER_AXIS_RIGHTY);
        if (x<-.5f) buttons |= button_to_mask(VirtualButton::CLeft);
        if (x>.5f) buttons |= button_to_mask(VirtualButton::CRight);
        if (y<-.5f) buttons |= button_to_mask(VirtualButton::CUp);
        if (y>.5f) buttons |= button_to_mask(VirtualButton::CDown);
    }
    return buttons;
}

std::unordered_map<VirtualButton,InputBinding> InputManager::default_port_bindings() const {
    std::unordered_map<VirtualButton,InputBinding> bindings;
    const std::pair<VirtualButton,SDL_GameControllerButton> mapping[] = {
        {VirtualButton::Up,SDL_CONTROLLER_BUTTON_DPAD_UP},{VirtualButton::Down,SDL_CONTROLLER_BUTTON_DPAD_DOWN},
        {VirtualButton::Left,SDL_CONTROLLER_BUTTON_DPAD_LEFT},{VirtualButton::Right,SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
        {VirtualButton::A,SDL_CONTROLLER_BUTTON_A},{VirtualButton::B,SDL_CONTROLLER_BUTTON_B},
        {VirtualButton::X,SDL_CONTROLLER_BUTTON_X},{VirtualButton::Y,SDL_CONTROLLER_BUTTON_Y},
        {VirtualButton::Start,SDL_CONTROLLER_BUTTON_START},{VirtualButton::Select,SDL_CONTROLLER_BUTTON_BACK},
        {VirtualButton::L,SDL_CONTROLLER_BUTTON_LEFTSHOULDER},{VirtualButton::R,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}};
    for(const auto& [button,code]:mapping) bindings[button]={InputSourceType::GamepadButton,-1,code,0,false};
    if(m_current_platform=="N64") {
        bindings.erase(VirtualButton::X);bindings.erase(VirtualButton::Y);bindings.erase(VirtualButton::Select);
        bindings[VirtualButton::Z]={InputSourceType::GamepadAxis,-1,SDL_CONTROLLER_AXIS_TRIGGERLEFT,.5f,true};
        bindings[VirtualButton::CUp]={InputSourceType::GamepadAxis,-1,SDL_CONTROLLER_AXIS_RIGHTY,.5f,false};
        bindings[VirtualButton::CDown]={InputSourceType::GamepadAxis,-1,SDL_CONTROLLER_AXIS_RIGHTY,.5f,true};
        bindings[VirtualButton::CLeft]={InputSourceType::GamepadAxis,-1,SDL_CONTROLLER_AXIS_RIGHTX,.5f,false};
        bindings[VirtualButton::CRight]={InputSourceType::GamepadAxis,-1,SDL_CONTROLLER_AXIS_RIGHTX,.5f,true};
    }
    return bindings;
}

void InputManager::reset_secondary_profiles() {
    m_stick_profiles={};
    for(int port=1;port<4;++port) m_player_bindings[port]=default_port_bindings();
}

void InputManager::reset_port_bindings(int port) {
    if(port>=0 && port<4)m_stick_profiles[port]=StickProfile{};
    if(port==0) load_default_bindings_for_platform(m_current_platform);
    else if(port>0 && port<4) m_player_bindings[port]=default_port_bindings();
}

uint32_t InputManager::secondary_buttons(int port) const {
    const int device=get_controller_for_port(port);
    uint32_t buttons=0;
    for(const auto& [button,binding]:m_player_bindings[port]) {
        bool pressed=false;
        switch(binding.type) {
            case InputSourceType::Keyboard:pressed=is_key_pressed(binding.code);break;
            case InputSourceType::GamepadButton:pressed=is_gamepad_button_pressed(device,binding.code);break;
            case InputSourceType::GamepadAxis:{
                const float axis=get_gamepad_axis(device,binding.code);
                pressed=device>=0 && (binding.axis_positive ? axis>binding.axis_threshold : axis< -binding.axis_threshold);
                break;
            }
        }
        if(pressed) buttons |= button_to_mask(button);
    }
    return buttons;
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
        // SDL instance IDs identify connections. GUID/name identify a model,
        // so two identical physical pads must remain independently usable.
        for (const auto& existing : m_controllers) {
            if (existing.instance_id == instance_id) {
                // Same instance ID - definitely a duplicate
                SDL_GameControllerClose(controller);
                return;
            }
        }

        ControllerInfo info;
        info.controller = controller;
        info.instance_id = instance_id;
        info.name = name;
        info.guid = guid_str;
#if SDL_VERSION_ATLEAST(2, 0, 14)
        const char* serial = SDL_GameControllerGetSerial(controller);
        info.serial = serial ? serial : "";
#endif
        m_controllers.push_back(info);
        restore_controller_bindings(info.guid);
        restore_port_assignments(info.guid);

        std::cout << "Controller connected: " << info.name << " (GUID: " << guid_str << ")" << std::endl;
    }
}

void InputManager::close_controller(int instance_id) {
    for (auto it = m_controllers.begin(); it != m_controllers.end(); ++it) {
        if (it->instance_id == instance_id) {
            std::cout << "Controller disconnected: " << it->name << std::endl;
            const int removed = static_cast<int>(it - m_controllers.begin());
            SDL_GameControllerClose(it->controller);
            m_controllers.erase(it);
            if (m_active_controller == removed) m_active_controller = -1;
            else if (m_active_controller > removed) --m_active_controller;
            // Release held input immediately rather than waiting for a frame.
            m_button_state = 0;
            m_port_buttons.fill(0);
            for (auto& instance:m_port_instances) if (instance==instance_id) instance=-1;
            break;
        }
    }
}

void InputManager::restore_controller_bindings(const std::string& connected_guid) {
    for (const auto& [button, identity] : m_binding_identities) {
        // A different model joining must not reassign a disconnected binding
        // to a pad of the old model that was already connected.
        if (!connected_guid.empty() && identity.guid != connected_guid) continue;
        auto binding = m_bindings.find(button);
        if (binding == m_bindings.end()) continue;
        // Preserve an existing connection even when another identical pad joins.
        if (std::any_of(m_controllers.begin(), m_controllers.end(), [&](const auto& pad) {
                return pad.instance_id == binding->second.device_id;
            })) continue;
        const ControllerInfo* match = nullptr;
        bool ambiguous = false;
        for (const auto& pad : m_controllers) {
            if (pad.guid != identity.guid ||
                (!identity.serial.empty() && pad.serial != identity.serial)) continue;
            if (match) { ambiguous = true; break; }
            match = &pad;
        }
        binding->second.device_id = match && !ambiguous ? match->instance_id : -2;
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

void InputManager::set_binding(VirtualButton button, const InputBinding& binding, int port) {
    if(port<0 || port>=4 || button>=VirtualButton::COUNT) return;
    if(port>0) {
        if(binding.type!=InputSourceType::Keyboard && binding.type!=InputSourceType::GamepadButton && binding.type!=InputSourceType::GamepadAxis) return;
        const int limit=binding.type==InputSourceType::Keyboard ? SDL_NUM_SCANCODES :
            binding.type==InputSourceType::GamepadButton ? int(SDL_CONTROLLER_BUTTON_MAX) : int(SDL_CONTROLLER_AXIS_MAX);
        if(binding.code<0 || binding.code>=limit || !std::isfinite(binding.axis_threshold) || binding.axis_threshold<0 || binding.axis_threshold>1) return;
        auto relative=binding;
        relative.device_id=-1;
        m_player_bindings[port][button]=relative;
        return;
    }
    m_bindings[button] = binding;
    m_binding_identities.erase(button);
    if (binding.type != InputSourceType::Keyboard && binding.device_id >= 0) {
        for (const auto& controller : m_controllers) {
            if (controller.instance_id == binding.device_id) {
                m_binding_identities[button] = {controller.guid, controller.serial};
                break;
            }
        }
    }
}

const InputBinding* InputManager::get_binding(VirtualButton button, int port) const {
    if(port<0 || port>=4) return nullptr;
    if(port>0) {
        const auto found=m_player_bindings[port].find(button);
        return found==m_player_bindings[port].end() ? nullptr : &found->second;
    }
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

void InputManager::clear_binding(VirtualButton button, int port) {
    if(port<0 || port>=4) return;
    if(port>0) {m_player_bindings[port].erase(button);return;}
    m_bindings.erase(button);
    m_binding_identities.erase(button);
}

void InputManager::set_active_controller(int controller_index) {
    set_controller_for_port(0,controller_index);
}

int InputManager::get_controller_for_port(int port) const {
    if (port<0 || port>=4) return -1;
    if (port==0) return m_active_controller;
    for (int i=0;i<get_connected_controller_count();++i)
        if (m_controllers[i].instance_id==m_port_instances[port]) return i;
    return -1;
}

void InputManager::set_controller_for_port(int port,int index) {
    if (port<0 || port>=4) return;
    const bool connected=index>=0 && index<get_connected_controller_count();
    const int instance=connected ? m_controllers[index].instance_id : -1;
    if (connected) {
        for (int other=0;other<4;++other) if (other!=port && m_port_instances[other]==instance) {
            m_port_instances[other]=-1;
            m_port_identities[other]={};
            if (other==0) m_active_controller=-1;
        }
    }
    m_port_instances[port]=instance;
    m_port_identities[port]=connected ? ControllerIdentity{m_controllers[index].guid,m_controllers[index].serial} : ControllerIdentity{};
    if (port==0) m_active_controller=connected ? index : -1;
    m_button_state=0;
    m_port_buttons.fill(0);
}

void InputManager::restore_port_assignments(const std::string& connected_guid) {
    for (int port=0;port<4;++port) {
        const auto& identity=m_port_identities[port];
        if (identity.guid.empty() || (!connected_guid.empty() && identity.guid!=connected_guid)) continue;
        if (m_port_instances[port]>=0 && std::any_of(m_controllers.begin(),m_controllers.end(),[&](const auto& pad){return pad.instance_id==m_port_instances[port];})) continue;
        // Shared models without serials cannot identify which saved port owns a pad.
        if (identity.serial.empty() && std::count_if(m_port_identities.begin(),m_port_identities.end(),[&](const auto& other){return other.guid==identity.guid && other.serial.empty();})>1) continue;
        int match=-1;
        for (int i=0;i<get_connected_controller_count();++i) {
            const auto& pad=m_controllers[i];
            if (pad.guid!=identity.guid || (!identity.serial.empty() && pad.serial!=identity.serial)) continue;
            if (match!=-1) { match=-2;break; }
            match=i;
        }
        if (match<0) continue;
        const int instance=m_controllers[match].instance_id;
        if (std::find(m_port_instances.begin(),m_port_instances.end(),instance)!=m_port_instances.end()) continue;
        m_port_instances[port]=instance;
        if (port==0) m_active_controller=match;
    }
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
        return std::clamp(value / 32767.0f, -1.0f, 1.0f);
    }
    return 0.0f;
}

bool InputManager::set_stick_profile(int port,const StickProfile& profile) {
    if(port<0 || port>=4 || !std::isfinite(profile.deadzone) || profile.deadzone<0 || profile.deadzone>=1 || profile.range<1 || profile.range>127)return false;
    for(int axis:profile.axes)if(axis< -1 || axis>=SDL_CONTROLLER_AXIS_MAX)return false;
    m_stick_profiles[port]=profile;return true;
}

const InputManager::StickProfile* InputManager::get_stick_profile(int port) const {
    return port>=0 && port<4 ? &m_stick_profiles[port] : nullptr;
}

uint32_t InputManager::encode_controller_input(int controller, uint32_t buttons) const {
    if (m_current_platform != "N64") return buttons;
    if (m_input_capture_mode || controller<0 || controller>=4) return 0;
    const int device = get_controller_for_port(controller);
    int x=0,y=0;
    if (device>=0) {
        const auto& profile=m_stick_profiles[controller];
        auto sample=[&](unsigned index) {
            if(profile.axes[index]<0)return 0;
            const float value=get_gamepad_axis(device,profile.axes[index]);
            return n64_input::quantize(profile.inverted[index]?-value:value,profile.deadzone,profile.range);
        };
        x=sample(0);y=sample(1);
    } else if (controller==0) {
        // T/F/G/H do not overlap the default N64 digital bindings.
        x=80*(int(is_key_pressed(SDL_SCANCODE_H))-int(is_key_pressed(SDL_SCANCODE_F)));
        y=80*(int(is_key_pressed(SDL_SCANCODE_T))-int(is_key_pressed(SDL_SCANCODE_G)));
    }
    return n64_input::pack(buttons,x,y);
}

void InputManager::set_current_platform(const std::string& platform) {
    if (m_current_platform != platform) {
        // Save current config before switching
        save_platform_config(m_current_platform);

        m_current_platform = platform;
        reset_secondary_profiles();

        // Try to load config for new platform
        if (!load_platform_config(platform)) {
            // If no saved config, load defaults
            m_port_instances.fill(-1);
            m_port_identities={};
            m_active_controller=-1;
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
    m_binding_identities.clear();

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
    } else if (platform == "GameCube" || platform == "Wii") {
        m_bindings.erase(VirtualButton::Select);
        const std::pair<VirtualButton, SDL_Scancode> keys[] = {
            {VirtualButton::A, SDL_SCANCODE_Z}, {VirtualButton::B, SDL_SCANCODE_X},
            {VirtualButton::X, SDL_SCANCODE_A}, {VirtualButton::Y, SDL_SCANCODE_S},
            {VirtualButton::Z, SDL_SCANCODE_C}, {VirtualButton::L, SDL_SCANCODE_Q},
            {VirtualButton::R, SDL_SCANCODE_W}};
        for (auto key : keys) m_bindings[key.first] = {InputSourceType::Keyboard, -1, key.second, 0, false};
    } else if (platform == "N64") {
        m_bindings.erase(VirtualButton::Select);
        const std::pair<VirtualButton, SDL_Scancode> keys[] = {
            {VirtualButton::A, SDL_SCANCODE_Z}, {VirtualButton::B, SDL_SCANCODE_X},
            {VirtualButton::Z, SDL_SCANCODE_C}, {VirtualButton::L, SDL_SCANCODE_Q},
            {VirtualButton::R, SDL_SCANCODE_W}, {VirtualButton::CUp, SDL_SCANCODE_I},
            {VirtualButton::CDown, SDL_SCANCODE_K}, {VirtualButton::CLeft, SDL_SCANCODE_J},
            {VirtualButton::CRight, SDL_SCANCODE_L}};
        for (auto key : keys) m_bindings[key.first] = {InputSourceType::Keyboard, -1, key.second, 0, false};
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
        j["controller_ports"]=nlohmann::json::array();
        for (const auto& identity:m_port_identities)
            j["controller_ports"].push_back({{"guid",identity.guid},{"serial",identity.serial}});

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

            auto identity = m_binding_identities.find(button);
            if (identity != m_binding_identities.end()) {
                binding_json["device_guid"] = identity->second.guid;
                binding_json["device_serial"] = identity->second.serial;
            }
            binding_json["device_id"] = binding.device_id;
            binding_json["code"] = binding.code;
            binding_json["axis_threshold"] = binding.axis_threshold;
            binding_json["axis_positive"] = binding.axis_positive;

            bindings_json[button_name] = binding_json;
        }
        if(platform=="N64") {
            j["n64_sticks"]=nlohmann::json::array();
            for(const auto& profile:m_stick_profiles)
                j["n64_sticks"].push_back({{"axes",profile.axes},{"inverted",profile.inverted},{"deadzone",profile.deadzone},{"range",profile.range}});
        }
        j["bindings"] = bindings_json;
        j["player_bindings"]=nlohmann::json::array();
        for(int port=1;port<4;++port) {
            auto profile=nlohmann::json::object();
            for(const auto& [button,binding]:m_player_bindings[port]) {
                const char* type=binding.type==InputSourceType::Keyboard ? "Keyboard" :
                    binding.type==InputSourceType::GamepadButton ? "GamepadButton" : "GamepadAxis";
                profile[get_button_name(button)]={{"type",type},{"code",binding.code},
                    {"axis_threshold",binding.axis_threshold},{"axis_positive",binding.axis_positive}};
            }
            j["player_bindings"].push_back(profile);
        }


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

        // Parse secondary profiles before changing live configuration. Missing
        // fields migrate old files to standard assigned-gamepad defaults.
        std::array<StickProfile,4> sticks{};
        if(platform=="N64" && j.contains("n64_sticks")) {
            const auto& saved=j.at("n64_sticks");
            if(!saved.is_array() || saved.size()!=4)return false;
            for(unsigned port=0;port<4;++port) {
                const auto& entry=saved.at(port);
                if(!entry.is_object() || !entry.at("axes").is_array() || entry.at("axes").size()!=2 || !entry.at("inverted").is_array() || entry.at("inverted").size()!=2)return false;
                for(unsigned index=0;index<2;++index) {
                    if(!entry.at("axes").at(index).is_number_integer() || !entry.at("inverted").at(index).is_boolean())return false;
                    const double axis=entry.at("axes").at(index).get<double>();
                    if(axis< -1 || axis>=SDL_CONTROLLER_AXIS_MAX)return false;
                    sticks[port].axes[index]=entry.at("axes").at(index).get<int>();
                    sticks[port].inverted[index]=entry.at("inverted").at(index).get<bool>();
                    if(sticks[port].axes[index]< -1 || sticks[port].axes[index]>=SDL_CONTROLLER_AXIS_MAX)return false;
                }
                if(!entry.at("deadzone").is_number() || !entry.at("range").is_number_integer())return false;
                const double range=entry.at("range").get<double>();
                if(range<1 || range>127)return false;
                sticks[port].deadzone=entry.at("deadzone").get<float>();sticks[port].range=entry.at("range").get<int>();
                if(!std::isfinite(sticks[port].deadzone) || sticks[port].deadzone<0 || sticks[port].deadzone>=1 || sticks[port].range<1 || sticks[port].range>127)return false;
            }
        }
        auto profiles=m_player_bindings;
        for(int port=1;port<4;++port) profiles[port]=default_port_bindings();
        if(j.contains("player_bindings")) {
            const auto& saved=j.at("player_bindings");
            if(!saved.is_array() || saved.size()!=3) return false;
            for(int port=1;port<4;++port) {
                if(!saved[port-1].is_object()) return false;
                profiles[port].clear();
                for(const auto& [name,value]:saved[port-1].items()) {
                    VirtualButton button=VirtualButton::COUNT;
                    for(unsigned i=0;i<unsigned(VirtualButton::COUNT);++i)
                        if(get_button_name(VirtualButton(i))==name) {button=VirtualButton(i);break;}
                    if(button==VirtualButton::COUNT) return false;
                    InputBinding binding{};binding.device_id=-1;
                    const auto type=value.at("type").get<std::string>();
                    if(type=="Keyboard") binding.type=InputSourceType::Keyboard;
                    else if(type=="GamepadButton") binding.type=InputSourceType::GamepadButton;
                    else if(type=="GamepadAxis") binding.type=InputSourceType::GamepadAxis;
                    else return false;
                    binding.code=value.at("code").get<int>();
                    binding.axis_threshold=value.value("axis_threshold",.5f);
                    binding.axis_positive=value.value("axis_positive",true);
                    const int limit=binding.type==InputSourceType::Keyboard ? SDL_NUM_SCANCODES :
                        binding.type==InputSourceType::GamepadButton ? int(SDL_CONTROLLER_BUTTON_MAX) : int(SDL_CONTROLLER_AXIS_MAX);
                    if(binding.code<0 || binding.code>=limit || !std::isfinite(binding.axis_threshold) ||
                        binding.axis_threshold<0 || binding.axis_threshold>1) return false;
                    profiles[port][button]=binding;
                }
            }
        }
        m_port_instances.fill(-1);
        m_port_identities={};
        m_active_controller=-1;
        m_port_buttons.fill(0);
        if (j.contains("controller_ports")) {
            const auto& ports=j.at("controller_ports");
            if (!ports.is_array() || ports.size()!=4) return false;
            for (int port=0;port<4;++port)
                m_port_identities[port]={ports[port].value("guid",std::string{}),ports[port].value("serial",std::string{})};
        } else if (j.contains("active_controller")) {
            m_active_controller = j["active_controller"].get<int>();
        }

        if (j.contains("bindings")) {
            m_bindings.clear();
            m_binding_identities.clear();

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

                if (binding.type != InputSourceType::Keyboard && binding_json.contains("device_guid")) {
                    m_binding_identities[button] = {binding_json["device_guid"].get<std::string>(),
                        binding_json.value("device_serial", std::string{})};
                    // Never resolve a saved connection ID against a different
                    // pad after process restart. -2 is disconnected, not "any".
                    binding.device_id = -2;
                }
                m_bindings[button] = binding;
            }
        }
        m_player_bindings=std::move(profiles);
        m_stick_profiles=sticks;
        restore_controller_bindings();
        if (j.contains("controller_ports")) restore_port_assignments();
        else set_active_controller(m_active_controller);

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
        case VirtualButton::Z: return "Z";
        case VirtualButton::CUp: return "CUp";
        case VirtualButton::CDown: return "CDown";
        case VirtualButton::CLeft: return "CLeft";
        case VirtualButton::CRight: return "CRight";
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
    } else if (platform == "GameCube" || platform == "Wii") {
        for (auto b : {VirtualButton::A, VirtualButton::B, VirtualButton::X, VirtualButton::Y,
                       VirtualButton::Z, VirtualButton::Start, VirtualButton::L, VirtualButton::R}) buttons.push_back(b);
    } else if (platform == "N64") {
        for (auto b : {VirtualButton::A, VirtualButton::B, VirtualButton::Z, VirtualButton::Start,
                       VirtualButton::L, VirtualButton::R, VirtualButton::CUp, VirtualButton::CDown,
                       VirtualButton::CLeft, VirtualButton::CRight}) buttons.push_back(b);
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
