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

void AudioManager::audio_callback(void* userdata, uint8_t* stream, int len) {
    AudioManager* self = static_cast<AudioManager*>(userdata);
    float* buffer = reinterpret_cast<float*>(stream);
    size_t samples = len / sizeof(float);

    self->fill_audio_buffer(buffer, samples);
}

void AudioManager::update_rate_control() {
    // Get current buffer level
    size_t buffered = get_buffered_samples();

    // Calculate error from target
    double error = static_cast<double>(buffered) - static_cast<double>(TARGET_BUFFER_SAMPLES);

    // Use proportional control with damping
    // Negative error (buffer low) -> increase rate (consume slower)
    // Positive error (buffer high) -> decrease rate (consume faster)
    // The rate adjustment affects how fast we consume samples, not produce
    double adjustment = error * 0.00001;  // Very gentle adjustment

    // Apply smooth exponential moving average to prevent jitter
    m_rate_adjustment = m_rate_adjustment * 0.99 + (1.0 + adjustment) * 0.01;

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
    // Total latency = buffered samples + SDL buffer
    size_t total_samples = get_buffered_samples() / 2;  // Convert stereo to mono count
    total_samples += m_buffer_size;  // Add SDL's internal buffer
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

    // The minimum buffer threshold depends on the sync mode
    size_t min_samples;
    switch (m_sync_mode) {
        case AudioSyncMode::AudioDriven:
            // For audio-driven, we can start almost immediately
            min_samples = m_buffer_size;  // Just enough for one SDL callback
            break;
        case AudioSyncMode::DynamicRate:
            // For dynamic rate, we want some buffer to allow rate adjustments
            min_samples = MIN_BUFFER_SAMPLES * 2;  // ~24ms
            break;
        case AudioSyncMode::LargeBuffer:
        default:
            // Legacy mode needs more buffer
            min_samples = MIN_STARTUP_SAMPLES * 2;  // ~92ms
            break;
    }

    return get_buffered_samples() >= min_samples;
}

} // namespace emu
