#include "audio_manager.hpp"

#include <SDL.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace emu {

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::initialize(int sample_rate, int buffer_size) {
    m_sample_rate = sample_rate;
    m_buffer_size = buffer_size;

    // Set up audio specification
    SDL_AudioSpec desired, obtained;
    std::memset(&desired, 0, sizeof(desired));

    desired.freq = sample_rate;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;  // Stereo
    desired.samples = buffer_size;
    desired.callback = audio_callback;
    desired.userdata = this;

    // Open audio device
    m_device_id = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (m_device_id == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        return false;
    }

    // Verify we got what we wanted
    if (obtained.format != AUDIO_F32SYS) {
        std::cerr << "Warning: Audio format mismatch" << std::endl;
    }

    m_sample_rate = obtained.freq;
    m_buffer_size = obtained.samples;

    // Clear ring buffer
    std::memset(m_ring_buffer, 0, sizeof(m_ring_buffer));
    m_read_pos = 0;
    m_write_pos = 0;

    // Reset rate control state
    m_rate_adjustment = 1.0;
    m_resample_accumulator = 0.0f;
    m_prev_sample_left = 0.0f;
    m_prev_sample_right = 0.0f;
    m_underrun_count = 0;
    m_overrun_count = 0;

    // Start audio paused
    m_paused = true;

    m_initialized = true;

    double latency_ms = (static_cast<double>(m_buffer_size) / m_sample_rate) * 1000.0;
    std::cout << "Audio manager initialized: " << m_sample_rate << " Hz, "
              << m_buffer_size << " sample buffer (~" << latency_ms << "ms SDL latency)" << std::endl;

    return true;
}

void AudioManager::shutdown() {
    if (m_device_id) {
        SDL_CloseAudioDevice(m_device_id);
        m_device_id = 0;
    }
    m_initialized = false;
}

void AudioManager::set_sync_mode(AudioSyncMode mode) {
    m_sync_mode = mode;

    // Reset rate control when switching modes
    m_rate_adjustment = 1.0;
    m_resample_accumulator = 0.0f;

    const char* mode_name = "Unknown";
    switch (mode) {
        case AudioSyncMode::AudioDriven: mode_name = "AudioDriven (lowest latency)"; break;
        case AudioSyncMode::DynamicRate: mode_name = "DynamicRate (deterministic timing)"; break;
        case AudioSyncMode::LargeBuffer: mode_name = "LargeBuffer (legacy)"; break;
    }
    std::cout << "Audio sync mode: " << mode_name << std::endl;
}

void AudioManager::set_sample_callback(SampleCallback callback) {
    m_sample_callback = std::move(callback);
}

void AudioManager::push_samples(const float* samples, size_t count) {
    if (!m_initialized || !samples || count == 0) return;

    const size_t buffer_capacity = RING_BUFFER_SIZE * 2;  // Stereo samples
    size_t write_pos = m_write_pos.load(std::memory_order_relaxed);
    size_t read_pos = m_read_pos.load(std::memory_order_acquire);

    for (size_t i = 0; i < count; i++) {
        size_t next_write = (write_pos + 1) % buffer_capacity;
        // Check for buffer overflow - drop samples if full
        if (next_write == read_pos) {
            m_overrun_count.fetch_add(1, std::memory_order_relaxed);
            break;  // Buffer full, drop remaining samples
        }
        m_ring_buffer[write_pos] = samples[i] * m_volume;
        write_pos = next_write;
    }

    m_write_pos.store(write_pos, std::memory_order_release);
}

void AudioManager::push_samples_resampled(const float* samples, size_t count, int source_rate) {
    if (!m_initialized || !samples || count == 0) return;

    // If source rate matches output rate, just push directly
    if (source_rate == m_sample_rate) {
        push_samples(samples, count);
        return;
    }

    // Linear interpolation resampling
    // Ratio = source_rate / target_rate
    // If source is 32000 and target is 44100, ratio = 0.7256
    // We need to produce more output samples than input
    double ratio = static_cast<double>(source_rate) / static_cast<double>(m_sample_rate);

    const size_t buffer_capacity = RING_BUFFER_SIZE * 2;
    size_t write_pos = m_write_pos.load(std::memory_order_relaxed);
    size_t read_pos = m_read_pos.load(std::memory_order_acquire);

    // count is stereo sample count (pairs), so count*2 is individual samples
    size_t input_samples = count / 2;  // Number of stereo pairs in input
    size_t input_idx = 0;

    while (input_idx < input_samples) {
        // Calculate output samples for this input position
        while (m_resample_accumulator < 1.0 && input_idx < input_samples) {
            // Get current and next samples for interpolation
            size_t curr = input_idx * 2;
            size_t next = std::min((input_idx + 1) * 2, count - 2);

            float curr_left = samples[curr];
            float curr_right = samples[curr + 1];
            float next_left = samples[next];
            float next_right = samples[next + 1];

            // Linear interpolation
            float t = static_cast<float>(m_resample_accumulator);
            float out_left = curr_left + t * (next_left - curr_left);
            float out_right = curr_right + t * (next_right - curr_right);

            // Push to buffer
            size_t next_write_l = (write_pos + 1) % buffer_capacity;
            if (next_write_l == read_pos) {
                m_overrun_count.fetch_add(1, std::memory_order_relaxed);
                goto done;  // Buffer full
            }
            m_ring_buffer[write_pos] = out_left * m_volume;
            write_pos = next_write_l;

            size_t next_write_r = (write_pos + 1) % buffer_capacity;
            if (next_write_r == read_pos) {
                m_overrun_count.fetch_add(1, std::memory_order_relaxed);
                goto done;  // Buffer full
            }
            m_ring_buffer[write_pos] = out_right * m_volume;
            write_pos = next_write_r;

            m_resample_accumulator += ratio;
        }

        // Move to next input sample
        while (m_resample_accumulator >= 1.0 && input_idx < input_samples) {
            m_resample_accumulator -= 1.0;
            input_idx++;
        }
    }

done:
    m_write_pos.store(write_pos, std::memory_order_release);
}

void AudioManager::audio_callback(void* userdata, uint8_t* stream, int len) {
    AudioManager* self = static_cast<AudioManager*>(userdata);
    float* buffer = reinterpret_cast<float*>(stream);
    size_t samples = len / sizeof(float);

    self->fill_audio_buffer(buffer, samples);
}

void AudioManager::update_rate_control() {
    // Get current buffer level
    size_t buffered = get_buffered_samples();

    // Calculate error from target (in samples)
    double error = static_cast<double>(buffered) - static_cast<double>(TARGET_BUFFER_SAMPLES);

    // Proportional-Integral (PI) control for smooth, responsive rate adjustment
    // The proportional term responds quickly to deviations
    // The integral term (accumulated in m_rate_adjustment) prevents steady-state error
    //
    // When buffer is HIGH (positive error):
    //   - We need to consume samples FASTER -> rate_adjustment > 1.0
    // When buffer is LOW (negative error):
    //   - We need to consume samples SLOWER -> rate_adjustment < 1.0

    // Proportional gain: more aggressive for faster response
    // 0.0001 means 500 samples of error = 5% adjustment contribution
    double p_gain = 0.0001;

    // Calculate proportional term
    double p_term = error * p_gain;

    // Use fast exponential smoothing for quick response to buffer changes
    // 0.85/0.15 means we adapt quickly to prevent buffer drift
    double smoothing = 0.85;
    m_rate_adjustment = m_rate_adjustment * smoothing + (1.0 + p_term) * (1.0 - smoothing);

    // Clamp to maximum adjustment range
    m_rate_adjustment = std::clamp(m_rate_adjustment, 1.0 - MAX_RATE_ADJUSTMENT, 1.0 + MAX_RATE_ADJUSTMENT);
}

void AudioManager::fill_audio_buffer(float* buffer, size_t samples) {
    if (m_paused.load(std::memory_order_relaxed)) {
        // Fade out smoothly when paused
        for (size_t i = 0; i < samples; i += 2) {
            float fade = 1.0f - static_cast<float>(i) / samples;
            buffer[i] = m_last_sample_left * fade;
            buffer[i + 1] = m_last_sample_right * fade;
        }
        m_last_sample_left = 0.0f;
        m_last_sample_right = 0.0f;
        return;
    }

    // For AudioDriven mode, request samples from emulator
    if (m_sync_mode == AudioSyncMode::AudioDriven && m_sample_callback) {
        size_t buffered = get_buffered_samples();
        size_t needed = (samples / 2) + m_buffer_size;  // Request enough for this callback plus one more

        if (buffered < needed) {
            // Request emulator to produce more samples
            m_sample_callback(needed - buffered);
        }
    }

    // Update dynamic rate control (for DynamicRate mode, but also provides stats for others)
    if (m_sync_mode == AudioSyncMode::DynamicRate) {
        update_rate_control();
    }

    const size_t buffer_capacity = RING_BUFFER_SIZE * 2;
    size_t read_pos = m_read_pos.load(std::memory_order_relaxed);
    size_t write_pos = m_write_pos.load(std::memory_order_acquire);

    // For DynamicRate mode, we resample to handle rate adjustment
    if (m_sync_mode == AudioSyncMode::DynamicRate) {
        for (size_t i = 0; i < samples; i += 2) {
            // Accumulate fractional sample position
            m_resample_accumulator += static_cast<float>(m_rate_adjustment);

            while (m_resample_accumulator >= 1.0f) {
                m_resample_accumulator -= 1.0f;

                // Calculate available samples
                size_t available;
                if (write_pos >= read_pos) {
                    available = write_pos - read_pos;
                } else {
                    available = buffer_capacity - read_pos + write_pos;
                }

                if (available >= 2) {
                    m_prev_sample_left = m_last_sample_left;
                    m_prev_sample_right = m_last_sample_right;
                    m_last_sample_left = m_ring_buffer[read_pos];
                    read_pos = (read_pos + 1) % buffer_capacity;
                    m_last_sample_right = m_ring_buffer[read_pos];
                    read_pos = (read_pos + 1) % buffer_capacity;
                } else {
                    // Underrun - fade samples toward zero to minimize clicking
                    m_underrun_count.fetch_add(1, std::memory_order_relaxed);

                    // Move prev toward current, and current toward zero
                    m_prev_sample_left = m_last_sample_left;
                    m_prev_sample_right = m_last_sample_right;
                    m_last_sample_left *= 0.95f;
                    m_last_sample_right *= 0.95f;
                }
            }

            // Linear interpolation between samples for smooth resampling
            float t = m_resample_accumulator;
            buffer[i] = m_prev_sample_left * (1.0f - t) + m_last_sample_left * t;
            buffer[i + 1] = m_prev_sample_right * (1.0f - t) + m_last_sample_right * t;
        }
    } else {
        // Simple mode (AudioDriven or LargeBuffer) - no resampling
        for (size_t i = 0; i < samples; i += 2) {
            // Calculate available samples (need at least 2 for stereo pair)
            size_t available;
            if (write_pos >= read_pos) {
                available = write_pos - read_pos;
            } else {
                available = buffer_capacity - read_pos + write_pos;
            }

            if (available >= 2) {
                float new_left = m_ring_buffer[read_pos];
                read_pos = (read_pos + 1) % buffer_capacity;
                float new_right = m_ring_buffer[read_pos];
                read_pos = (read_pos + 1) % buffer_capacity;

                // Apply interpolation to smooth sample transitions
                // This reduces high-frequency artifacts from sample discontinuities
                buffer[i] = 0.5f * (new_left + m_last_sample_left);
                buffer[i + 1] = 0.5f * (new_right + m_last_sample_right);

                m_last_sample_left = new_left;
                m_last_sample_right = new_right;
            } else {
                // Underrun - fade toward zero gradually to minimize clicking
                // Instead of holding the last sample indefinitely (which can cause DC offset),
                // we fade it toward zero over time
                m_underrun_count.fetch_add(1, std::memory_order_relaxed);

                // Exponential decay toward zero
                m_last_sample_left *= 0.95f;
                m_last_sample_right *= 0.95f;

                buffer[i] = m_last_sample_left;
                buffer[i + 1] = m_last_sample_right;
            }
        }
    }

    m_read_pos.store(read_pos, std::memory_order_release);
}

void AudioManager::pause() {
    if (m_device_id) {
        SDL_PauseAudioDevice(m_device_id, 1);
        m_paused = true;
    }
}

void AudioManager::resume() {
    if (m_device_id) {
        SDL_PauseAudioDevice(m_device_id, 0);
        m_paused = false;
    }
}

void AudioManager::set_volume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

size_t AudioManager::get_buffered_samples() const {
    size_t read_pos = m_read_pos.load();
    size_t write_pos = m_write_pos.load();
    const size_t buffer_capacity = RING_BUFFER_SIZE * 2;

    if (write_pos >= read_pos) {
        return write_pos - read_pos;
    } else {
        return buffer_capacity - read_pos + write_pos;
    }
}

double AudioManager::get_latency_ms() const {
    // Total latency = ring buffer samples + SDL buffer
    // get_buffered_samples() returns float count (L+R individual samples)
    // Divide by 2 to get stereo pair count (actual audio samples)
    size_t ring_buffer_samples = get_buffered_samples() / 2;
    // SDL buffer is already in samples (stereo pairs)
    size_t total_samples = ring_buffer_samples + m_buffer_size;
    return (static_cast<double>(total_samples) / m_sample_rate) * 1000.0;
}

void AudioManager::clear_buffer() {
    // Don't immediately zero everything - fade out the last samples
    // to prevent clicking on buffer clear
    // The audio callback will naturally fade to zero via underrun handling

    m_read_pos.store(0, std::memory_order_relaxed);
    m_write_pos.store(0, std::memory_order_relaxed);

    // Keep last sample values for smooth transition - they'll fade via underrun handling
    // Don't reset these to zero:
    // m_last_sample_left = 0.0f;
    // m_last_sample_right = 0.0f;

    m_prev_sample_left = m_last_sample_left;
    m_prev_sample_right = m_last_sample_right;
    m_resample_accumulator = 0.0f;
    m_rate_adjustment = 1.0;
    std::memset(m_ring_buffer, 0, sizeof(m_ring_buffer));
}

bool AudioManager::is_buffer_ready() const {
    if (!m_initialized) return false;

    // The minimum buffer threshold depends on the sync mode.
    // We want to start playback as soon as possible to minimize latency,
    // while ensuring we have enough samples to avoid immediate underrun.
    //
    // get_buffered_samples() returns count of floats (L/R individual samples).
    // At 44100Hz stereo: 256 floats = 128 stereo pairs = ~2.9ms
    size_t min_samples;
    switch (m_sync_mode) {
        case AudioSyncMode::AudioDriven:
            // For audio-driven, we can start almost immediately
            // Just need enough for one SDL callback (m_buffer_size * 2 for stereo floats)
            min_samples = m_buffer_size * 2;
            break;
        case AudioSyncMode::DynamicRate:
            // For dynamic rate, start quickly - rate control will adapt.
            // We only need enough to fill one SDL callback without underrun.
            // m_buffer_size is 256 samples, so we need 256*2 = 512 floats minimum.
            // Start with exactly one SDL buffer worth to minimize latency.
            min_samples = m_buffer_size * 2;  // ~5.8ms - one SDL buffer, rate control compensates
            break;
        case AudioSyncMode::LargeBuffer:
        default:
            // Legacy mode needs more buffer for stability
            min_samples = MIN_STARTUP_SAMPLES * 2;  // ~46ms pre-buffer
            break;
    }

    return get_buffered_samples() >= min_samples;
}

} // namespace emu
