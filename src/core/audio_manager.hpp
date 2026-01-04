#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <functional>

namespace emu {

// Default audio settings
constexpr int DEFAULT_SAMPLE_RATE = 44100;
// Minimum SDL buffer for lowest latency. With streaming audio and
// dynamic rate control, we can use a very small buffer.
// 128 samples = ~2.9ms at 44100Hz (about 1/6 of a frame)
// This is the primary source of audio latency - keep it as small as possible.
constexpr int DEFAULT_BUFFER_SIZE = 128;

// Audio synchronization modes
enum class AudioSyncMode {
    // Audio callback drives emulation timing (lowest latency, best for most users)
    // The emulator runs when audio needs samples
    AudioDriven,

    // Emulator runs at fixed rate, audio uses dynamic resampling to compensate
    // (deterministic frame timing, good for TAS - slight pitch adjustment inaudible)
    DynamicRate,

    // Legacy mode: large buffer, fixed rate (high latency but simple)
    LargeBuffer
};

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Disable copy
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Initialize audio system
    bool initialize(int sample_rate = DEFAULT_SAMPLE_RATE,
                   int buffer_size = DEFAULT_BUFFER_SIZE);

    // Shutdown
    void shutdown();

    // Set audio sync mode
    void set_sync_mode(AudioSyncMode mode);
    AudioSyncMode get_sync_mode() const { return m_sync_mode; }

    // Push audio samples from emulator (for DynamicRate and LargeBuffer modes)
    void push_samples(const float* samples, size_t count);

    // Push audio samples with resampling from source rate to output rate
    void push_samples_resampled(const float* samples, size_t count, int source_rate);

    // For AudioDriven mode: set callback that produces samples on demand
    // The callback should run enough emulation to produce the requested samples
    using SampleCallback = std::function<void(size_t samples_needed)>;
    void set_sample_callback(SampleCallback callback);

    // Control
    void pause();
    void resume();
    void set_volume(float volume);  // 0.0 - 1.0
    float get_volume() const { return m_volume; }

    // Status
    bool is_initialized() const { return m_initialized; }
    int get_sample_rate() const { return m_sample_rate; }
    size_t get_buffered_samples() const;

    // Get current audio latency in milliseconds
    double get_latency_ms() const;

    // Check if buffer has enough samples to start playback without underruns
    bool is_buffer_ready() const;

    // Clear audio buffer
    void clear_buffer();

    // Dynamic rate control statistics (for debugging/display)
    double get_current_rate_adjustment() const { return m_rate_adjustment; }
    size_t get_underrun_count() const { return m_underrun_count; }
    size_t get_overrun_count() const { return m_overrun_count; }

private:
    // SDL audio callback
    static void audio_callback(void* userdata, uint8_t* stream, int len);
    void fill_audio_buffer(float* buffer, size_t samples);

    // Dynamic rate control
    void update_rate_control();
    float resample_with_rate_adjustment(float sample);

    // Ring buffer for audio samples (lock-free SPSC)
    static constexpr size_t RING_BUFFER_SIZE = 16384;  // ~370ms at 44kHz - smaller is fine with rate control
    float m_ring_buffer[RING_BUFFER_SIZE * 2];  // Stereo
    std::atomic<size_t> m_read_pos{0};
    std::atomic<size_t> m_write_pos{0};

    // Last sample for smooth underrun handling
    float m_last_sample_left = 0.0f;
    float m_last_sample_right = 0.0f;

    // State
    bool m_initialized = false;
    uint32_t m_device_id = 0;
    int m_sample_rate = DEFAULT_SAMPLE_RATE;
    int m_buffer_size = DEFAULT_BUFFER_SIZE;
    float m_volume = 1.0f;
    std::atomic<bool> m_paused{false};

    // Sync mode
    AudioSyncMode m_sync_mode = AudioSyncMode::DynamicRate;
    SampleCallback m_sample_callback;

    // Dynamic rate control state
    // Target: keep buffer at minimal level for low latency while avoiding underruns
    // Lower target = lower latency, but requires faster rate control response
    //
    // Note: These values are in FLOATS (individual L/R samples), not stereo pairs.
    // At 44100Hz stereo: 128 floats = 64 stereo pairs = ~1.5ms
    double m_rate_adjustment = 1.0;  // 1.0 = no adjustment, 1.001 = 0.1% faster
    static constexpr double MAX_RATE_ADJUSTMENT = 0.01;    // +/- 1% max (still inaudible)
    static constexpr size_t TARGET_BUFFER_SAMPLES = 128;   // ~1.5ms target buffer level (minimum latency)
    static constexpr size_t MIN_BUFFER_SAMPLES = 32;       // ~0.4ms minimum before rate increase
    static constexpr size_t MAX_BUFFER_SAMPLES = 256;      // ~2.9ms maximum before rate decrease

    // Resampler state for fractional sample interpolation
    float m_resample_accumulator = 0.0f;
    float m_prev_sample_left = 0.0f;
    float m_prev_sample_right = 0.0f;

    // Statistics
    std::atomic<size_t> m_underrun_count{0};
    std::atomic<size_t> m_overrun_count{0};

    // Audio buffer parameters for LargeBuffer mode (legacy)
    static constexpr size_t MIN_STARTUP_SAMPLES = 2048;  // ~46ms pre-buffer for legacy mode
};

} // namespace emu
