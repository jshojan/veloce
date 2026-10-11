#include "emu/input_plugin.hpp"
#include <unordered_map>
#include <vector>
#include <fstream>
#include <optional>

namespace {

class DefaultInputPlugin : public emu::IInputPlugin {
public:
    DefaultInputPlugin() {
        // Set up default keyboard bindings for NES
        setup_default_bindings();
    }

    ~DefaultInputPlugin() override = default;

    emu::InputPluginInfo get_info() override {
        return {
            "Default Input",
            "1.0.0",
            "Veloce Team",
            "Standard keyboard and gamepad input handler with hot-plugging support. "
            "Features input recording, playback, and turbo/autofire functionality.",
            true,   // supports_recording
            true,   // supports_playback
            true    // supports_turbo
        };
    }

    bool initialize(emu::IInputHost* host) override {
        m_host = host;
        return true;
    }

    void shutdown() override {
        stop_recording();
        stop_playback();
        m_host = nullptr;
    }

    void begin_frame() override {
        if (!m_host) return;

        const auto next_frame = m_host->get_frame_number();
        if (next_frame < m_frame_number) {
            stop_recording();
            stop_playback();
            m_turbo_frame = 0;
            m_turbo_active = false;
        }
        m_frame_number = next_frame;
        if (m_playing) {
            if (m_frame_number > static_cast<uint64_t>(m_playback_inputs.back().frame_number)) m_playing = false;
            else while (m_playback_pos + 1 < m_playback_inputs.size() &&
                        m_playback_inputs[m_playback_pos + 1].frame_number <= static_cast<int64_t>(m_frame_number)) ++m_playback_pos;
        }

        // Toggle turbo state each frame based on rate
        if (m_turbo_rate > 0) {
            m_turbo_frame++;
            if (m_turbo_frame >= m_turbo_rate) {
                m_turbo_frame = 0;
                m_turbo_active = !m_turbo_active;
            }
        }
    }

    uint32_t get_input_state(int controller) override {
        if (!m_host || controller < 0 || controller >= 4) return 0;

        // If we have an override set, use it
        if (m_input_override[controller].has_value()) {
            return record(controller, m_input_override[controller].value());
        }

        // Snapshot format represents controller zero. Read playback without
        // advancing it: all readers of one frame must receive the same mask.
        if (controller == 0 && m_playing && m_playback_pos < m_playback_inputs.size() &&
            m_playback_inputs[m_playback_pos].frame_number <= static_cast<int64_t>(m_frame_number)) {
            return record(controller, m_playback_inputs[m_playback_pos].buttons);
        }

        auto* mapped = dynamic_cast<emu::IMappedInputHost*>(m_host);
        uint32_t buttons = mapped ? mapped->get_mapped_input(controller) : 0;

        // Check each binding
        for (int i = 0; i < static_cast<int>(emu::VirtualButton::COUNT); i++) {
            auto button = static_cast<emu::VirtualButton>(i);
            auto it = m_bindings[controller].find(button);
            if (it != m_bindings[controller].end() && (!mapped || m_explicit_bindings[controller].count(button))) {
                const auto& binding = it->second;
                bool pressed = false;

                switch (binding.type) {
                    case emu::InputSourceType::Keyboard:
                        pressed = m_host->is_key_pressed(binding.code);
                        break;
                    case emu::InputSourceType::GamepadButton:
                        if (binding.device_id == -1) {
                            for (int i = 0; i < m_host->get_connected_controller_count(); ++i) {
                                if (m_host->is_gamepad_button_pressed(i, binding.code)) { pressed = true; break; }
                            }
                        } else pressed = m_host->is_gamepad_button_pressed(binding.device_id, binding.code);
                        break;
                    case emu::InputSourceType::GamepadAxis: {
                        auto active = [&](int device) {
                            const float axis = m_host->get_gamepad_axis(device, binding.code);
                            return binding.axis_positive ? axis > binding.axis_threshold : axis < -binding.axis_threshold;
                        };
                        if (binding.device_id == -1) {
                            for (int i = 0; i < m_host->get_connected_controller_count(); ++i) {
                                if (active(i)) { pressed = true; break; }
                            }
                        } else pressed = active(binding.device_id);
                        break;
                    }
                }

                buttons &= ~emu::button_to_mask(button);

                if (pressed) {
                    buttons |= emu::button_to_mask(button);
                }
            }
        }

        for (const auto& [button, enabled] : m_turbo_enabled[controller]) {
            if (enabled && !m_turbo_active) buttons &= ~emu::button_to_mask(button);
        }

        return record(controller, buttons);
    }

    emu::InputBinding get_binding(int controller, emu::VirtualButton button) const override {
        if (controller < 0 || controller >= 4) return {};
        if (!m_explicit_bindings[controller].count(button)) {
            if (auto* mapped = dynamic_cast<emu::IMappedInputHost*>(m_host)) {
                emu::InputBinding binding{};
                if (mapped->get_mapped_binding(controller, button, binding)) return binding;
            }
        }
        auto it = m_bindings[controller].find(button);
        if (it != m_bindings[controller].end()) {
            return it->second;
        }
        return {};
    }

    void set_binding(int controller, emu::VirtualButton button, const emu::InputBinding& binding) override {
        if (controller >= 0 && controller < 4) {
            m_bindings[controller][button] = binding;
            m_explicit_bindings[controller][button] = true;
        }
    }

    bool is_turbo_enabled(int controller, emu::VirtualButton button) const override {
        if (controller < 0 || controller >= 4) return false;
        auto it = m_turbo_enabled[controller].find(button);
        return it != m_turbo_enabled[controller].end() && it->second;
    }

    void set_turbo_enabled(int controller, emu::VirtualButton button, bool enabled) override {
        if (controller >= 0 && controller < 4) {
            m_turbo_enabled[controller][button] = enabled;
        }
    }

    int get_turbo_rate() const override {
        return m_turbo_rate;
    }

    void set_turbo_rate(int frames_per_press) override {
        m_turbo_rate = frames_per_press > 0 ? frames_per_press : 2;
    }

    bool start_recording() override {
        m_recorded_inputs.clear();
        m_recording = true;
        return true;
    }

    void stop_recording() override {
        m_recording = false;
    }

    bool is_recording() const override {
        return m_recording;
    }

    std::vector<emu::InputSnapshot> get_recording() const override {
        return m_recorded_inputs;
    }

    bool start_playback(const std::vector<emu::InputSnapshot>& inputs) override {
        if (inputs.empty()) return false;
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (inputs[i].frame_number < 0 || (i && inputs[i-1].frame_number >= inputs[i].frame_number)) return false;
        }
        m_playback_inputs = inputs;
        m_playback_pos = 0;
        m_playing = true;
        return true;
    }

    void stop_playback() override {
        m_playing = false;
        m_playback_inputs.clear();
        m_playback_pos = 0;
    }

    bool is_playing() const override {
        return m_playing;
    }

    uint64_t get_playback_frame() const override {
        if (m_playback_pos < m_playback_inputs.size()) {
            return m_playback_inputs[m_playback_pos].frame_number;
        }
        return 0;
    }

    uint64_t get_playback_length() const override {
        if (m_playback_inputs.empty()) return 0;
        return m_playback_inputs.back().frame_number;
    }

    void set_input_override(int controller, uint32_t buttons) override {
        if (controller >= 0 && controller < 4) {
            m_input_override[controller] = buttons;
        }
    }

    void clear_input_override(int controller) override {
        if (controller >= 0 && controller < 4) {
            m_input_override[controller].reset();
        }
    }

    void set_controller_layout(const emu::ControllerLayoutInfo* layout) override {
        m_controller_layout = layout;
    }

    const emu::ControllerLayoutInfo* get_controller_layout() const override {
        return m_controller_layout;
    }

private:
    uint32_t record(int controller, uint32_t buttons) {
        if (m_recording && controller == 0 &&
            (m_recorded_inputs.empty() || m_recorded_inputs.back().frame_number != static_cast<int64_t>(m_frame_number))) {
            m_recorded_inputs.push_back({buttons, static_cast<int64_t>(m_frame_number)});
        }
        return buttons;
    }
    void setup_default_bindings() {
        // Default NES keyboard bindings for controller 0
        // SDL scancodes
        constexpr int SDL_SCANCODE_UP = 82;
        constexpr int SDL_SCANCODE_DOWN = 81;
        constexpr int SDL_SCANCODE_LEFT = 80;
        constexpr int SDL_SCANCODE_RIGHT = 79;
        constexpr int SDL_SCANCODE_Z = 29;
        constexpr int SDL_SCANCODE_X = 27;
        constexpr int SDL_SCANCODE_RETURN = 40;
        constexpr int SDL_SCANCODE_RSHIFT = 229;

        m_bindings[0][emu::VirtualButton::Up] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_UP, 0, false};
        m_bindings[0][emu::VirtualButton::Down] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_DOWN, 0, false};
        m_bindings[0][emu::VirtualButton::Left] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_LEFT, 0, false};
        m_bindings[0][emu::VirtualButton::Right] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_RIGHT, 0, false};
        m_bindings[0][emu::VirtualButton::A] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_Z, 0, false};
        m_bindings[0][emu::VirtualButton::B] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_X, 0, false};
        m_bindings[0][emu::VirtualButton::Start] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_RETURN, 0, false};
        m_bindings[0][emu::VirtualButton::Select] = {emu::InputSourceType::Keyboard, -1, SDL_SCANCODE_RSHIFT, 0, false};
    }

    emu::IInputHost* m_host = nullptr;
    uint64_t m_frame_number = 0;

    // Controller layout from emulator plugin
    const emu::ControllerLayoutInfo* m_controller_layout = nullptr;

    // Bindings per controller
    std::unordered_map<emu::VirtualButton, emu::InputBinding> m_bindings[4];

    std::unordered_map<emu::VirtualButton, bool> m_explicit_bindings[4];

    // Turbo settings per controller
    std::unordered_map<emu::VirtualButton, bool> m_turbo_enabled[4];
    int m_turbo_rate = 2;  // Frames per press
    int m_turbo_frame = 0;
    bool m_turbo_active = false;

    // Recording
    bool m_recording = false;
    std::vector<emu::InputSnapshot> m_recorded_inputs;

    // Playback
    bool m_playing = false;
    std::vector<emu::InputSnapshot> m_playback_inputs;
    size_t m_playback_pos = 0;

    // Override
    std::optional<uint32_t> m_input_override[4];
};

} // anonymous namespace

// C interface implementation
extern "C" {

EMU_PLUGIN_EXPORT emu::IInputPlugin* create_input_plugin() {
    return new DefaultInputPlugin();
}

EMU_PLUGIN_EXPORT void destroy_input_plugin(emu::IInputPlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_input_plugin_api_version() {
    return EMU_INPUT_PLUGIN_API_VERSION;
}

} // extern "C"
