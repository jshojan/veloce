#include "emu/audio_plugin.hpp"
#include <cstring>
#include <algorithm>

namespace {

class DefaultAudioPlugin : public emu::IAudioPlugin {
public:
    DefaultAudioPlugin() = default;
    ~DefaultAudioPlugin() override = default;

    emu::AudioPluginInfo get_info() override {
        return {
            "Default Audio",
            "1.0.0",
            "Veloce Team",
            "Standard audio passthrough with volume control. Provides basic "
            "muting and volume adjustment for emulator audio output.",
            false,  // No recording
            false   // No effects
        };
    }

    bool initialize(emu::IAudioHost* host) override {
        m_host = host;
        return true;
    }

    void shutdown() override {
        m_host = nullptr;
    }

    void process(const float* input, size_t sample_count) override {
        if (!m_host || !input || sample_count == 0) return;

        // Apply volume and mute
        float effective_volume = m_muted ? 0.0f : m_volume;

        if (effective_volume == 1.0f) {
            // Direct passthrough
            m_host->output_samples(input, sample_count);
        } else {
            // Apply volume
            // Use stack buffer for small amounts, heap for large
            constexpr size_t STACK_BUFFER_SIZE = 4096;
            float stack_buffer[STACK_BUFFER_SIZE];
            float* buffer = sample_count <= STACK_BUFFER_SIZE ?
                            stack_buffer : new float[sample_count];

            for (size_t i = 0; i < sample_count; i++) {
                buffer[i] = input[i] * effective_volume;
            }

            m_host->output_samples(buffer, sample_count);

            if (sample_count > STACK_BUFFER_SIZE) {
                delete[] buffer;
            }
        }
    }

    void set_volume(float volume) override {
        m_volume = std::clamp(volume, 0.0f, 1.0f);
    }

    float get_volume() const override {
        return m_volume;
    }

    void set_muted(bool muted) override {
        m_muted = muted;
    }

    bool is_muted() const override {
        return m_muted;
    }

private:
    emu::IAudioHost* m_host = nullptr;
    float m_volume = 1.0f;
    bool m_muted = false;
};

} // anonymous namespace

// C interface implementation
extern "C" {

EMU_PLUGIN_EXPORT emu::IAudioPlugin* create_audio_plugin() {
    return new DefaultAudioPlugin();
}

EMU_PLUGIN_EXPORT void destroy_audio_plugin(emu::IAudioPlugin* plugin) {
    delete plugin;
}

EMU_PLUGIN_EXPORT uint32_t get_audio_plugin_api_version() {
    return EMU_AUDIO_PLUGIN_API_VERSION;
}

} // extern "C"
