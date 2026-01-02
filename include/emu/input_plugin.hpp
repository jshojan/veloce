#pragma once

#include "input_types.hpp"
#include "controller_layout.hpp"
#include <cstdint>
#include <vector>

// DLL export macro for Windows
#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_INPUT_PLUGIN_API_VERSION 1

namespace emu {

// Input plugin information
struct InputPluginInfo {
    const char* name;           // "Default Input", "TAS Input", etc.
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description
    bool supports_recording;    // Can record input
    bool supports_playback;     // Can playback recorded input
    bool supports_turbo;        // Has turbo/autofire support
};

// Controller type detection
struct ControllerInfo {
    int id;                     // Controller index
    const char* name;           // Controller name (from driver)
    bool is_connected;          // Currently connected
    int button_count;           // Number of buttons
    int axis_count;             // Number of axes
    int hat_count;              // Number of D-pads
};

// Host interface provided to input plugins
class IInputHost {
public:
    virtual ~IInputHost() = default;

    // Get current frame number
    virtual uint64_t get_frame_number() const = 0;

    // Get emulated system name
    virtual const char* get_platform_name() const = 0;

    // Get number of controllers for current system
    virtual int get_controller_count() const = 0;

    // Raw input polling
    virtual bool is_key_pressed(int scancode) const = 0;
    virtual bool is_gamepad_button_pressed(int controller, int button) const = 0;
    virtual float get_gamepad_axis(int controller, int axis) const = 0;

    // Connected controller info
    virtual int get_connected_controller_count() const = 0;
    virtual ControllerInfo get_controller_info(int index) const = 0;
};

// Input plugin interface
class IInputPlugin {
public:
    virtual ~IInputPlugin() = default;

    // Get plugin info
    virtual InputPluginInfo get_info() = 0;

    // Initialize with host interface
    virtual bool initialize(IInputHost* host) = 0;
    virtual void shutdown() = 0;

    // Set the controller layout from the emulator plugin
    // Called when a ROM is loaded to configure input for this platform
    virtual void set_controller_layout(const ControllerLayoutInfo* layout) {}

    // Get the current controller layout (may return nullptr if not set)
    virtual const ControllerLayoutInfo* get_controller_layout() const { return nullptr; }

    // Called at the start of each frame
    virtual void begin_frame() = 0;

    // Get input state for a specific controller
    // Returns the button bitmask for the current frame
    virtual uint32_t get_input_state(int controller) = 0;

    // Get/set input bindings
    virtual InputBinding get_binding(int controller, VirtualButton button) const = 0;
    virtual void set_binding(int controller, VirtualButton button, const InputBinding& binding) = 0;

    // Turbo/autofire (optional)
    virtual bool is_turbo_enabled(int controller, VirtualButton button) const { return false; }
    virtual void set_turbo_enabled(int controller, VirtualButton button, bool enabled) {}
    virtual int get_turbo_rate() const { return 0; }
    virtual void set_turbo_rate(int frames_per_press) {}

    // Input recording (optional)
    virtual bool start_recording() { return false; }
    virtual void stop_recording() {}
    virtual bool is_recording() const { return false; }
    virtual std::vector<InputSnapshot> get_recording() const { return {}; }

    // Input playback (optional)
    virtual bool start_playback(const std::vector<InputSnapshot>& inputs) { return false; }
    virtual void stop_playback() {}
    virtual bool is_playing() const { return false; }
    virtual uint64_t get_playback_frame() const { return 0; }
    virtual uint64_t get_playback_length() const { return 0; }

    // Override input during playback (for TAS editing)
    virtual void set_input_override(int controller, uint32_t buttons) {}
    virtual void clear_input_override(int controller) {}

    // Save/load bindings
    virtual bool save_config(const char* filename) { return false; }
    virtual bool load_config(const char* filename) { return false; }
};

} // namespace emu

// C interface for plugin loading
extern "C" {
    EMU_PLUGIN_EXPORT emu::IInputPlugin* create_input_plugin();
    EMU_PLUGIN_EXPORT void destroy_input_plugin(emu::IInputPlugin* plugin);
    EMU_PLUGIN_EXPORT uint32_t get_input_plugin_api_version();
}
