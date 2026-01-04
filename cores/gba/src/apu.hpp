#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <functional>

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

    // Get audio samples (legacy buffered mode)
    size_t get_samples(float* buffer, size_t max_samples);

    // Streaming audio callback - called frequently with small batches for low latency
    // Parameters: samples (interleaved stereo), sample_count (stereo pairs), sample_rate
    using AudioStreamCallback = std::function<void(const float*, size_t, int)>;
    void set_audio_callback(AudioStreamCallback callback) { m_audio_callback = callback; }

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

    // GBA Direct Sound FIFO
    struct DSFIFO {
        static constexpr int CAPACITY = 8;  // 8 x 32-bit words = 32 bytes
        std::array<uint32_t, CAPACITY> data{};
        int rd_ptr = 0;
        int wr_ptr = 0;
        int count = 0;

        void reset() {
            rd_ptr = wr_ptr = count = 0;
            for (auto& d : data) d = 0;
        }

        void write_word(uint32_t value) {
            if (count < CAPACITY) {
                data[wr_ptr] = value;
                wr_ptr = (wr_ptr + 1) % CAPACITY;
                count++;
            }
        }

        uint32_t read_word() {
            if (count > 0) {
                uint32_t value = data[rd_ptr];
                rd_ptr = (rd_ptr + 1) % CAPACITY;
                count--;
                return value;
            }
            return 0;
        }

        bool empty() const { return count == 0; }
        int size() const { return count; }
    };

    // Pipeline for byte-by-byte FIFO consumption with interpolation
    struct DSPipe {
        uint32_t word = 0;
        int bytes_left = 0;
        int8_t sample = 0;        // Current sample
        int8_t prev_sample = 0;   // Previous sample for interpolation
        float interp_pos = 0.0f;  // Interpolation position (0.0 to 1.0)
        float interp_step = 0.0f; // Step per output sample (based on timer rate)
    };

    std::array<DSFIFO, 2> m_dsound_fifo;
    std::array<DSPipe, 2> m_dsound_pipe;

    // SOUNDCNT_H register state
    uint16_t m_soundcnt_h = 0;
    int m_dmg_volume = 2;           // 0=25%, 1=50%, 2=100%
    bool m_dsound_a_vol = false;    // false=50%, true=100%
    bool m_dsound_b_vol = false;
    bool m_dsound_a_left = false;
    bool m_dsound_a_right = false;
    int m_dsound_a_timer = 0;       // 0 or 1
    bool m_dsound_b_left = false;
    bool m_dsound_b_right = false;
    int m_dsound_b_timer = 0;

    // Callback to request DMA refill
    std::function<void(int)> m_request_fifo_dma;

    // Consume one byte from FIFO
    void consume_fifo_sample(int idx);

public:
    // Direct Sound methods
    void write_fifo_a(uint32_t value);
    void write_fifo_b(uint32_t value);
    void on_timer_overflow(int timer_id);
    void write_soundcnt_h(uint16_t value);
    uint16_t read_soundcnt_h() const;
    int get_fifo_count(int idx) const { return m_dsound_fifo[idx].size(); }
    void set_fifo_dma_callback(std::function<void(int)> cb) { m_request_fifo_dma = cb; }

private:

    // Master control
    bool m_enabled = false;
    uint8_t m_nr50 = 0;  // Master volume/VIN panning
    uint8_t m_nr51 = 0;  // Sound panning
    uint8_t m_nr52 = 0;  // Sound on/off

    // Audio buffer (legacy buffered mode)
    static constexpr size_t AUDIO_BUFFER_SIZE = 8192;
    std::array<float, AUDIO_BUFFER_SIZE * 2> m_audio_buffer;
    size_t m_audio_write_pos = 0;

    // Streaming audio callback and buffer
    AudioStreamCallback m_audio_callback;
    static constexpr size_t STREAM_BUFFER_SIZE = 64;  // Small buffer for low latency
    float m_stream_buffer[STREAM_BUFFER_SIZE * 2];    // Stereo
    size_t m_stream_pos = 0;

    // Timing
    int m_cycles = 0;
    int m_sample_counter = 0;

    // Duty patterns
    static const uint8_t s_duty_table[4][8];
};

} // namespace gba
