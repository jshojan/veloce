#pragma once
#include "emu/audio_plugin.hpp"
#include <functional>
#include <utility>

namespace emu {
// Emulation-thread bridge. Plugins process source-rate interleaved stereo floats;
// the device callback receives their output with the same source rate to resample.
class AudioPipeline final : public IAudioHost {
public:
    using Output = std::function<void(const float*, size_t, int)>;
    AudioPipeline(Output output, std::function<void()> pause, std::function<void()> resume)
        : m_output(std::move(output)), m_pause(std::move(pause)), m_resume(std::move(resume)) {}
    AudioFormat get_format() const override { return {m_rate, 2, 32}; }
    void output_samples(const float* samples, size_t count) override {
        if (samples && count && count % 2 == 0) m_output(samples, count, m_rate);
    }
    void pause_output() override { m_pause(); }
    void resume_output() override { m_resume(); }
    void begin_frame() { m_streamed = false; }
    void stream(IAudioPlugin* plugin, const float* samples, size_t count, int rate, bool suppressed) {
        m_streamed = true;
        if (!suppressed) process(plugin, samples, count, rate);
    }
    void batch(IAudioPlugin* plugin, const float* samples, size_t count, int rate, bool suppressed) {
        if (!m_streamed && !suppressed) process(plugin, samples, count, rate);
    }
private:
    void process(IAudioPlugin* plugin, const float* samples, size_t count, int rate) {
        if (!samples || !count || count % 2 || rate <= 0) return;
        m_rate = rate;
        if (plugin) plugin->process(samples, count);
        else output_samples(samples, count);
    }
    Output m_output;
    std::function<void()> m_pause, m_resume;
    int m_rate = 44100;
    bool m_streamed = false;
};
}
