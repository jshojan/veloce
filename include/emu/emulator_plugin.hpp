#pragma once

#include "controller_layout.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>

// DLL export macro for Windows
#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

namespace emu {

// Information about the emulator plugin
struct EmulatorInfo {
    const char* name;               // "NES", "SNES", etc.
    const char* version;            // "1.0.0"
    const char* author;             // Plugin author
    const char* description;        // Brief description of the core
    const char** file_extensions;   // {".nes", nullptr}
    double native_fps;              // 60.0988 for NES (NTSC)
    uint64_t cycles_per_second;     // 1789773 for NES CPU
    int screen_width;               // 256 for NES
    int screen_height;              // 240 for NES
};

// Framebuffer for video output
struct FrameBuffer {
    uint32_t* pixels;   // RGBA8888 format
    int width;
    int height;
};

// Audio buffer for sound output
struct AudioBuffer {
    float* samples;     // Interleaved stereo samples (-1.0 to 1.0)
    int sample_count;   // Number of sample pairs
    int sample_rate;    // Typically 44100 or 48000
};

// Input state for controllers
// Button bitmask uses VirtualButton ordering from input_types.hpp
struct InputState {
    uint32_t buttons;   // Bitmask of pressed buttons
};

// Main emulator plugin interface
class IEmulatorPlugin {
public:
    virtual ~IEmulatorPlugin() = default;

    // Plugin information
    virtual EmulatorInfo get_info() = 0;

    // Controller layout for this platform
    // Returns the visual layout for input configuration UI
    // Default implementation returns nullptr (use default layout based on platform name)
    virtual const ControllerLayoutInfo* get_controller_layout() { return nullptr; }

    // ROM loading and management
    virtual bool load_rom(const uint8_t* data, size_t size) = 0;
    virtual void unload_rom() = 0;
    virtual bool is_rom_loaded() const = 0;
    virtual uint32_t get_rom_crc32() const = 0;

    // Emulation control
    virtual void reset() = 0;
    virtual void run_frame(const InputState& input) = 0;
    virtual uint64_t get_cycle_count() const = 0;
    virtual uint64_t get_frame_count() const = 0;

    // Video output
    virtual FrameBuffer get_framebuffer() = 0;

    // Audio output
    virtual AudioBuffer get_audio() = 0;
    virtual void clear_audio_buffer() = 0;

    // ============================================================
    // Streaming Audio (low-latency)
    // ============================================================

    // Callback type for streaming audio - called during emulation with small batches
    // Parameters: samples (interleaved stereo floats), sample_count (number of stereo pairs), sample_rate
    using AudioStreamCallback = std::function<void(const float* samples, size_t sample_count, int sample_rate)>;

    // Set the audio streaming callback for low-latency audio
    // When set, the core should push audio samples frequently during run_frame()
    // instead of batching them until get_audio() is called.
    // Set to nullptr to disable streaming and use traditional get_audio() mode.
    virtual void set_audio_callback(AudioStreamCallback callback) { m_audio_callback = callback; }

    // Check if streaming audio is enabled
    bool has_audio_callback() const { return m_audio_callback != nullptr; }

    // Memory access (for speedrun plugins and RAM watch)
    virtual uint8_t read_memory(uint16_t address) = 0;
    virtual void write_memory(uint16_t address, uint8_t value) = 0;

    // Save states
    virtual bool save_state(std::vector<uint8_t>& data) = 0;
    virtual bool load_state(const std::vector<uint8_t>& data) = 0;

    // Battery-backed save file support (SRAM, EEPROM, etc.)
    // These allow games with battery saves to persist data across sessions.
    // The application handles file I/O; the plugin provides/accepts raw data.

    // Returns true if the currently loaded ROM has battery-backed save data
    virtual bool has_battery_save() const { return false; }

    // Get the current battery save data (PRG RAM, EEPROM, etc.)
    // Returns empty vector if no save data available
    virtual std::vector<uint8_t> get_battery_save_data() const { return {}; }

    // Load battery save data (called before reset, after ROM load)
    // Returns true if data was successfully loaded
    virtual bool set_battery_save_data(const std::vector<uint8_t>& data) { (void)data; return false; }

    // ============================================================
    // Speed / Timing Configuration
    // ============================================================

    // Returns true if the core wants to run in fast/uncapped mode
    // When true, the application should skip frame timing and run as fast as possible
    // This is useful for fast-forward or "overclock" modes
    virtual bool is_fast_mode_enabled() const { return false; }

    // ============================================================
    // Configuration GUI (optional)
    // ============================================================

    // Returns true if this core has configuration options
    // Override this to return true if you implement render_config_gui()
    virtual bool has_config_gui() const { return false; }

    // Set the ImGui context from the main application
    // This MUST be called before render_config_gui() to ensure the plugin
    // uses the same ImGui context as the application
    virtual void set_imgui_context(void* context) { (void)context; }

    // Render the core's configuration GUI panel in a standalone window
    // Called when the user opens the core settings window
    // The visible flag can be used to close the window
    // Default implementation does nothing
    virtual void render_config_gui(bool& visible) { (void)visible; }

    // Render the core's configuration GUI content only (no window wrapper)
    // Used when embedding the config in another window (e.g., Cores panel)
    // Default implementation calls render_config_gui with a dummy visible flag
    virtual void render_config_gui_content() {
        bool dummy = true;
        render_config_gui(dummy);
    }

    // Get the name for the configuration window (e.g., "Game Boy Settings")
    // Default returns the core name + " Settings"
    virtual const char* get_config_window_name() const { return nullptr; }

    // ============================================================
    // Configuration Persistence (optional)
    // ============================================================

    // Save the core's configuration to a file
    // The path is provided by the application (e.g., config/cores/gb.json)
    // Returns true on success
    virtual bool save_config(const char* path) { (void)path; return true; }

    // Load the core's configuration from a file
    // The path is provided by the application
    // Returns true on success (or if file doesn't exist - uses defaults)
    virtual bool load_config(const char* path) { (void)path; return true; }

protected:
    // Audio streaming callback (set by application for low-latency audio)
    AudioStreamCallback m_audio_callback;
};

} // namespace emu

// C interface for dynamic loading
extern "C" {
    // Create a new plugin instance
    EMU_PLUGIN_EXPORT emu::IEmulatorPlugin* create_emulator_plugin();

    // Destroy plugin instance
    EMU_PLUGIN_EXPORT void destroy_emulator_plugin(emu::IEmulatorPlugin* plugin);

    // Get plugin API version (for compatibility checking)
    EMU_PLUGIN_EXPORT uint32_t get_plugin_api_version();
}

// Current API version
#define EMU_PLUGIN_API_VERSION 1
