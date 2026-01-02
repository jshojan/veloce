#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

// DLL export macro for Windows
#ifdef _WIN32
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define EMU_PLUGIN_EXPORT
#endif

#define EMU_AUDIO_PLUGIN_API_VERSION 1

namespace emu {

// Audio plugin information
struct AudioPluginInfo {
    const char* name;           // "Default Audio", "DSP Effects", etc.
    const char* version;        // "1.0.0"
    const char* author;         // Plugin author
    const char* description;    // Brief description
    bool supports_recording;    // Can record audio
    bool supports_effects;      // Has DSP effects
};

// Audio format specification
struct AudioFormat {
    int sample_rate;        // 44100, 48000, etc.
    int channels;           // 1 = mono, 2 = stereo
    int bits_per_sample;    // 16, 24, 32
};

// Host interface provided to audio plugins
class IAudioHost {
public:
    virtual ~IAudioHost() = default;

    // Get the requested audio format
    virtual AudioFormat get_format() const = 0;

    // Push processed audio to output device
    virtual void output_samples(const float* samples, size_t count) = 0;

    // Pause/resume audio output
    virtual void pause_output() = 0;
    virtual void resume_output() = 0;
};

// Audio plugin interface
class IAudioPlugin {
public:
    virtual ~IAudioPlugin() = default;

    // Get plugin info
    virtual AudioPluginInfo get_info() = 0;

    // Initialize with host interface
    virtual bool initialize(IAudioHost* host) = 0;
    virtual void shutdown() = 0;

    // Process audio samples from emulator
    // Input: raw samples from emulator
    // Plugin should call host->output_samples() with processed audio
    virtual void process(const float* input, size_t sample_count) = 0;

    // Volume control (0.0 - 1.0)
    virtual void set_volume(float volume) = 0;
    virtual float get_volume() const = 0;

    // Mute control
    virtual void set_muted(bool muted) = 0;
    virtual bool is_muted() const = 0;

    // Recording (optional, return false if not supported)
    virtual bool start_recording(const char* filename) { return false; }
    virtual void stop_recording() {}
    virtual bool is_recording() const { return false; }

    // DSP effects (optional)
    virtual int get_effect_count() const { return 0; }
    virtual const char* get_effect_name(int index) const { return nullptr; }
    virtual bool is_effect_enabled(int index) const { return false; }
    virtual void set_effect_enabled(int index, bool enabled) {}
    virtual float get_effect_parameter(int index, int param) const { return 0.0f; }
    virtual void set_effect_parameter(int index, int param, float value) {}
};

} // namespace emu

// C interface for plugin loading
extern "C" {
    EMU_PLUGIN_EXPORT emu::IAudioPlugin* create_audio_plugin();
    EMU_PLUGIN_EXPORT void destroy_audio_plugin(emu::IAudioPlugin* plugin);
    EMU_PLUGIN_EXPORT uint32_t get_audio_plugin_api_version();
}
