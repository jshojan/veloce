#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <functional>

namespace nes {

class Bus;

// NES APU (Audio Processing Unit) - 2A03
class APU {
public:
    explicit APU(Bus& bus);
    ~APU();

    void reset();
    void step(int cpu_cycles);

    // Set the current CPU cycle counter (for accurate jitter timing)
    // This should be called before cpu_write for accurate $4017 timing
    void set_cpu_cycle(uint64_t cycle);

    // Region configuration
    enum class Region { NTSC, PAL, Dendy };
    void set_region(Region region);
    Region get_region() const { return m_region; }

    uint8_t cpu_read(uint16_t address);
    void cpu_write(uint16_t address, uint8_t value);

    // Get audio samples (stereo, interleaved)
    size_t get_samples(float* buffer, size_t max_samples);

    // Streaming audio callback - called frequently with small batches for low latency
    // Parameters: samples (interleaved stereo), sample_count (stereo pairs), sample_rate
    using AudioStreamCallback = std::function<void(const float*, size_t, int)>;
    void set_audio_callback(AudioStreamCallback callback) { m_audio_callback = callback; }

    // DMC DMA support - returns cycles the CPU should stall
    int get_dmc_dma_cycles();

    // Check if DMC or frame counter IRQ is pending
    bool irq_pending() const { return m_frame_irq || m_dmc.irq_pending; }

    // Set expansion audio output (for mapper audio chips)
    void set_expansion_audio(float output) { m_expansion_audio = output; }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void clock_frame_counter();
    void clock_length_counters();
    void clock_envelopes();
    void clock_sweeps();
    void clock_dmc();
    void dmc_fetch_sample();

    float mix_output();

    Bus& m_bus;

    // Frame counter
    int m_frame_counter_mode = 0;
    int m_frame_counter_step = 0;
    int m_frame_counter_cycles = 0;
    bool m_irq_inhibit = false;
    bool m_frame_irq = false;

    // Frame counter reset delay (handles $4017 write timing jitter)
    // When $4017 is written:
    // - On odd CPU cycle: reset happens 3 cycles later
    // - On even CPU cycle: reset happens 4 cycles later
    int m_frame_counter_reset_delay = 0;
    bool m_frame_counter_reset_pending = false;
    int m_pending_frame_counter_mode = 0;

    // Pulse channels
    struct Pulse {
        bool enabled = false;
        uint8_t duty = 0;
        bool length_halt = false;
        bool constant_volume = false;
        uint8_t volume = 0;
        bool sweep_enabled = false;
        uint8_t sweep_period = 0;
        bool sweep_negate = false;
        uint8_t sweep_shift = 0;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint8_t sequence_pos = 0;
        uint8_t length_counter = 0;
        uint8_t envelope_counter = 0;
        uint8_t envelope_divider = 0;
        bool envelope_start = false;
        uint8_t sweep_divider = 0;
        bool sweep_reload = false;
    };
    Pulse m_pulse[2];

    // Triangle channel
    struct Triangle {
        bool enabled = false;
        bool control_flag = false;
        uint8_t linear_counter_reload = 0;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint8_t sequence_pos = 0;
        uint8_t length_counter = 0;
        uint8_t linear_counter = 0;
        bool linear_counter_reload_flag = false;
    };
    Triangle m_triangle;

    // Noise channel
    struct Noise {
        bool enabled = false;
        bool length_halt = false;
        bool constant_volume = false;
        uint8_t volume = 0;
        bool mode = false;
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint16_t shift_register = 1;
        uint8_t length_counter = 0;
        uint8_t envelope_counter = 0;
        uint8_t envelope_divider = 0;
        bool envelope_start = false;
    };
    Noise m_noise;

    // DMC channel with full DMA support
    struct DMC {
        bool enabled = false;
        bool irq_enabled = false;
        bool loop = false;
        uint8_t rate_index = 0;
        uint8_t output_level = 0;

        // Sample parameters (set by registers)
        uint16_t sample_address = 0xC000;
        uint16_t sample_length = 1;

        // Current playback state
        uint16_t current_address = 0xC000;  // Current read address
        uint16_t bytes_remaining = 0;       // Bytes left to read

        // Sample buffer (holds fetched sample byte)
        uint8_t sample_buffer = 0;
        bool sample_buffer_empty = true;

        // Output unit (shift register)
        uint8_t shift_register = 0;
        uint8_t bits_remaining = 0;  // Bits left in shift register (0-8)
        bool silence_flag = true;

        // Timer
        uint16_t timer = 0;
        uint16_t timer_period = 428;  // Default period

        // IRQ
        bool irq_pending = false;
    };
    DMC m_dmc;

    // DMC DMA state
    int m_dmc_dma_cycles = 0;      // Pending DMA cycles for CPU stall
    bool m_dmc_dma_pending = false; // A DMA read is pending

    // DMC rate table (in CPU cycles)
    static const uint16_t s_dmc_rate_table[16];

    // Audio output buffer - sized for ~1 frame of audio at 44.1kHz
    // NES runs at ~60.0988 FPS, so one frame = ~735 samples
    // Use 2048 to allow some headroom without adding excessive latency
    static constexpr size_t AUDIO_BUFFER_SIZE = 2048;
    std::array<float, AUDIO_BUFFER_SIZE * 2> m_audio_buffer;
    size_t m_audio_write_pos = 0;

    // Region configuration
    Region m_region = Region::NTSC;

    // Timing (varies by region)
    int m_cycles = 0;
    int m_sample_counter = 0;
    static constexpr int SAMPLE_RATE = 44100;
    int m_cpu_freq = 1789773;  // NTSC: 1789773, PAL: 1662607, Dendy: 1773448

    // Frame counter step thresholds (varies by region)
    // Per blargg's apu_test:
    // - NTSC length counters at: 14916, 29832 (mode 0); 14916, 37284 (mode 1)
    // - NTSC IRQ at: 29831 (1 cycle before length at step 4)
    int m_frame_step1 = 7458;   // Quarter frame 1: envelope/linear
    int m_frame_step2 = 14916;  // Quarter frame 2: envelope/linear, length/sweep
    int m_frame_step3 = 22374;  // Quarter frame 3: envelope/linear
    int m_frame_step4 = 29832;  // Quarter frame 4: envelope/linear, length/sweep
    int m_frame_irq_cycle = 29831;  // IRQ flag set (mode 0 only)
    int m_frame_step5 = 37284;  // 5-step mode: envelope/linear, length/sweep
    int m_frame_reset4 = 29833;
    int m_frame_reset5 = 37285;

    // High-pass and low-pass filter state (matching NES hardware characteristics)
    // The NES has a high-pass filter at ~37Hz and low-pass at ~14kHz
    float m_hp_filter_state = 0.0f;    // High-pass filter (removes DC offset)
    float m_lp_filter_state = 0.0f;    // Low-pass filter (anti-aliasing)

    // Pre-downsampling anti-aliasing filter (applied at CPU rate)
    // This is a more aggressive filter to prevent aliasing artifacts
    float m_aa_filter_state = 0.0f;

    // Higher-order filter for better anti-aliasing (2-pole Butterworth approximation)
    float m_aa_filter_state2 = 0.0f;

    // Sample accumulator for averaging (downsampling)
    float m_sample_accumulator = 0.0f;
    int m_sample_count = 0;

    // Previous output sample for interpolation during downsampling
    float m_prev_output_sample = 0.0f;

    // DC blocking filter state
    float m_dc_blocker_prev_in = 0.0f;
    float m_dc_blocker_prev_out = 0.0f;

    // DMC smoothing to reduce clicks from direct loads
    float m_dmc_smoothed_output = 0.0f;
    static constexpr float DMC_SMOOTH_FACTOR = 0.95f;  // Smooth rapid DMC changes

    // Expansion audio input (from mapper audio chips)
    float m_expansion_audio = 0.0f;
    float m_expansion_audio_smoothed = 0.0f;  // Smoothed expansion audio

    // Streaming audio callback and buffer
    AudioStreamCallback m_audio_callback;
    static constexpr size_t STREAM_BUFFER_SIZE = 64;  // Small buffer for low latency
    float m_stream_buffer[STREAM_BUFFER_SIZE * 2];    // Stereo
    size_t m_stream_pos = 0;

    // Lookup tables (use current region's tables via pointers)
    static const uint8_t s_length_table[32];
    static const uint16_t s_noise_period_ntsc[16];
    static const uint16_t s_noise_period_pal[16];
    static const uint16_t s_dmc_rate_ntsc[16];
    static const uint16_t s_dmc_rate_pal[16];
    static const uint8_t s_duty_table[4][8];
    static const uint8_t s_triangle_table[32];

    // Current region's lookup tables
    const uint16_t* m_noise_period_table = s_noise_period_ntsc;
    const uint16_t* m_dmc_rate_table_ptr = s_dmc_rate_ntsc;

    // Global CPU cycle counter for accurate jitter timing
    // This is set by the bus before APU register writes
    uint64_t m_global_cpu_cycle = 0;
};

} // namespace nes
