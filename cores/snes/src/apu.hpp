#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <memory>

namespace snes {

class SPC700;
class DSP;

// SNES APU - Wrapper for SPC700 + DSP audio subsystem
// Handles synchronization between main CPU and audio processor
class APU {
public:
    APU();
    ~APU();

    void reset();

    // Step APU for given number of master clock cycles
    void step(int master_cycles);

    // Communication ports (main CPU side)
    uint8_t read_port(int port);
    void write_port(int port, uint8_t value);

    // Get audio samples (stereo, interleaved)
    size_t get_samples(float* buffer, size_t max_samples);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    std::unique_ptr<SPC700> m_spc;
    std::unique_ptr<DSP> m_dsp;

    // Timing
    int m_cycle_counter = 0;
    static constexpr int MASTER_CYCLES_PER_SPC = 21;  // ~1.024 MHz from 21.477 MHz

    // Audio output buffer
    static constexpr size_t AUDIO_BUFFER_SIZE = 8192;
    std::array<float, AUDIO_BUFFER_SIZE * 2> m_audio_buffer;
    size_t m_audio_write_pos = 0;

    // Sample rate conversion
    int m_sample_counter = 0;
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int DSP_RATE = 32000;

    // Resampling state
    int16_t m_last_left = 0;
    int16_t m_last_right = 0;
};

} // namespace snes
