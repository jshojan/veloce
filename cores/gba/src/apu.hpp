#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>

namespace gba {

// Audio Processing Unit - supports both GB and GBA
class APU {
public:
    APU();
    ~APU();

    void reset();
    void step(int cycles);

    void set_system_type(SystemType type) { m_system_type = type; }

    // GB audio register access
    uint8_t read_register(uint16_t address);
    void write_register(uint16_t address, uint8_t value);

    // Get audio samples
    size_t get_samples(float* buffer, size_t max_samples);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void clock_frame_sequencer();
    void clock_length_counters();
    void clock_envelopes();
    void clock_sweep();

    void mix_output(float& left, float& right);

    SystemType m_system_type = SystemType::GameBoy;

    // Frame sequencer
    int m_frame_counter = 0;
    int m_frame_counter_step = 0;

    // Channel 1: Pulse with sweep
    struct Pulse1 {
        bool enabled = false;
        uint8_t duty = 0;
        bool length_enable = false;
        uint8_t length_counter = 0;
        uint8_t envelope_initial = 0;
        bool envelope_dir = false;
        uint8_t envelope_period = 0;
        uint8_t envelope_counter = 0;
        uint8_t volume = 0;
        uint16_t frequency = 0;
        uint16_t timer = 0;
        uint8_t sequence_pos = 0;

        // Sweep
        bool sweep_enabled = false;
        uint8_t sweep_period = 0;
        uint8_t sweep_shift = 0;
        bool sweep_negate = false;
        uint16_t sweep_shadow = 0;
        uint8_t sweep_counter = 0;
    };
    Pulse1 m_pulse1;

    // Channel 2: Pulse (no sweep)
    struct Pulse2 {
        bool enabled = false;
        uint8_t duty = 0;
        bool length_enable = false;
        uint8_t length_counter = 0;
        uint8_t envelope_initial = 0;
        bool envelope_dir = false;
        uint8_t envelope_period = 0;
        uint8_t envelope_counter = 0;
        uint8_t volume = 0;
        uint16_t frequency = 0;
        uint16_t timer = 0;
        uint8_t sequence_pos = 0;
    };
    Pulse2 m_pulse2;

    // Channel 3: Wave
    struct Wave {
        bool enabled = false;
        bool dac_enabled = false;
        uint16_t length_counter = 0;  // 256 max, needs 16-bit
        bool length_enable = false;
        uint8_t volume_code = 0;
        uint16_t frequency = 0;
        uint16_t timer = 0;
        uint8_t position = 0;
        uint8_t sample_buffer = 0;
        std::array<uint8_t, 16> wave_ram;
    };
    Wave m_wave;

    // Channel 4: Noise
    struct Noise {
        bool enabled = false;
        bool length_enable = false;
        uint8_t length_counter = 0;
        uint8_t envelope_initial = 0;
        bool envelope_dir = false;
        uint8_t envelope_period = 0;
        uint8_t envelope_counter = 0;
        uint8_t volume = 0;
        uint8_t divisor_code = 0;
        bool width_mode = false;
        uint8_t clock_shift = 0;
        uint16_t timer = 0;
        uint16_t lfsr = 0x7FFF;
    };
    Noise m_noise;

    // GBA Direct Sound channels (simplified)
    struct DirectSound {
        std::array<int8_t, 32> fifo;
        int fifo_pos = 0;
        int fifo_size = 0;
        int8_t current_sample = 0;
    };
    DirectSound m_fifo_a;
    DirectSound m_fifo_b;

    // Master control
    bool m_enabled = false;
    uint8_t m_nr50 = 0;  // Master volume/VIN panning
    uint8_t m_nr51 = 0;  // Sound panning
    uint8_t m_nr52 = 0;  // Sound on/off

    // Audio buffer
    static constexpr size_t AUDIO_BUFFER_SIZE = 8192;
    std::array<float, AUDIO_BUFFER_SIZE * 2> m_audio_buffer;
    size_t m_audio_write_pos = 0;

    // Timing
    int m_cycles = 0;
    int m_sample_counter = 0;

    // Duty patterns
    static const uint8_t s_duty_table[4][8];
};

} // namespace gba
