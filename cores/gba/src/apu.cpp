#include "apu.hpp"
#include "debug.hpp"
#include <cstring>
#include <algorithm>
#include <cstdio>

namespace gba {

// Debug statistics for audio tracking
static uint64_t s_debug_total_samples = 0;
static uint64_t s_debug_frame_count = 0;

// Duty patterns for pulse channels
// These patterns determine when the waveform is HIGH (1) vs LOW (0)
// The actual output will be converted to bipolar (-1 to +1) based on volume
const uint8_t APU::s_duty_table[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1},  // 12.5% - one high sample out of 8
    {0, 0, 0, 0, 0, 0, 1, 1},  // 25%   - two high samples (corrected pattern)
    {0, 0, 0, 0, 1, 1, 1, 1},  // 50%   - four high samples (corrected pattern)
    {1, 1, 1, 1, 1, 1, 0, 0},  // 75%   - six high samples (corrected pattern)
};

APU::APU() {
    m_wave.wave_ram.fill(0);
    reset();
}

APU::~APU() = default;

void APU::reset() {
    // Initialize to post-boot ROM state
    // NR52 = 0xF1 means sound is enabled (bit 7 = 1) and channel 1 is on (bit 0 = 1)
    m_nr50 = 0x77;
    m_nr51 = 0xF3;
    m_nr52 = 0xF1;
    m_enabled = true;  // Sound is enabled after boot ROM

    m_pulse1 = {};
    m_pulse2 = {};
    m_wave = {};
    m_noise = {};
    m_noise.lfsr = 0x7FFF;

    // Reset Direct Sound FIFOs
    for (auto& fifo : m_dsound_fifo) fifo.reset();
    for (auto& pipe : m_dsound_pipe) pipe = {};

    // Reset SOUNDCNT_H state
    m_soundcnt_h = 0;
    m_dmg_volume = 2;  // 100% by default
    m_dsound_a_vol = false;
    m_dsound_b_vol = false;
    m_dsound_a_left = m_dsound_a_right = false;
    m_dsound_b_left = m_dsound_b_right = false;
    m_dsound_a_timer = 0;
    m_dsound_b_timer = 0;

    m_frame_counter = 0;
    m_frame_counter_step = 0;
    m_cycles = 0;
    m_sample_counter = 0;
    m_audio_write_pos = 0;
}

void APU::step(int cycles) {
    if (!m_enabled) return;

    for (int i = 0; i < cycles; i++) {
        m_frame_counter++;

        // Frame sequencer clocks at 512 Hz (every 8192 T-cycles on GB @ 4.194304 MHz)
        // For GBA (16.78 MHz), it's every 32768 cycles
        int frame_period = (m_system_type == SystemType::GameBoyAdvance) ? 32768 : 8192;
        if (m_frame_counter >= frame_period) {
            m_frame_counter -= frame_period;
            clock_frame_sequencer();
        }

        // Step pulse 1 timer - always runs even when channel is disabled (for proper reload)
        // Timer clocks at CPU/4 rate, but we're already in T-cycles so check every cycle
        if (m_pulse1.timer > 0) {
            m_pulse1.timer--;
        }
        if (m_pulse1.timer == 0) {
            m_pulse1.timer = (2048 - m_pulse1.frequency) * 4;
            m_pulse1.sequence_pos = (m_pulse1.sequence_pos + 1) & 7;
        }

        // Step pulse 2 timer
        if (m_pulse2.timer > 0) {
            m_pulse2.timer--;
        }
        if (m_pulse2.timer == 0) {
            m_pulse2.timer = (2048 - m_pulse2.frequency) * 4;
            m_pulse2.sequence_pos = (m_pulse2.sequence_pos + 1) & 7;
        }

        // Step wave timer - wave channel clocks at CPU/2 rate
        if (m_wave.timer > 0) {
            m_wave.timer--;
        }
        if (m_wave.timer == 0) {
            m_wave.timer = (2048 - m_wave.frequency) * 2;
            m_wave.position = (m_wave.position + 1) & 31;
            uint8_t byte = m_wave.wave_ram[m_wave.position / 2];
            m_wave.sample_buffer = (m_wave.position & 1) ? (byte & 0x0F) : (byte >> 4);
        }

        // Step noise timer
        if (m_noise.timer > 0) {
            m_noise.timer--;
        }
        if (m_noise.timer == 0) {
            // Divisor table: r=0 -> 8, else r*16
            uint16_t divisor = m_noise.divisor_code == 0 ? 8 : (m_noise.divisor_code * 16);
            m_noise.timer = divisor << m_noise.clock_shift;

            // Clock LFSR - XOR bits 0 and 1
            uint16_t xor_result = (m_noise.lfsr & 1) ^ ((m_noise.lfsr >> 1) & 1);
            m_noise.lfsr = (m_noise.lfsr >> 1) | (xor_result << 14);
            if (m_noise.width_mode) {
                // 7-bit mode: also set bit 6
                m_noise.lfsr &= ~(1 << 6);
                m_noise.lfsr |= xor_result << 6;
            }
        }

        // Generate sample at target rate (~44100 Hz)
        // GB CPU: 4194304 Hz, target: 44100 Hz -> 4194304/44100 = ~95.1 cycles per sample
        // Use fractional accumulator for accurate timing
        m_sample_counter += 44100;
        int cpu_freq = (m_system_type == SystemType::GameBoyAdvance) ? 16777216 : 4194304;
        if (m_sample_counter >= cpu_freq) {
            m_sample_counter -= cpu_freq;

            float left, right;
            mix_output(left, right);

            // If streaming callback is set, use low-latency path
            if (m_audio_callback) {
                m_stream_buffer[m_stream_pos * 2] = left;
                m_stream_buffer[m_stream_pos * 2 + 1] = right;
                m_stream_pos++;

                // Flush when buffer is full (every 64 samples = ~1.5ms)
                if (m_stream_pos >= STREAM_BUFFER_SIZE) {
                    m_audio_callback(m_stream_buffer, m_stream_pos, 44100);
                    m_stream_pos = 0;
                }
            } else {
                // Legacy path: buffer until get_samples() is called
                if (m_audio_write_pos < AUDIO_BUFFER_SIZE) {
                    m_audio_buffer[m_audio_write_pos * 2] = left;
                    m_audio_buffer[m_audio_write_pos * 2 + 1] = right;
                    m_audio_write_pos++;
                }
            }
        }
    }
}

void APU::clock_frame_sequencer() {
    m_frame_counter_step = (m_frame_counter_step + 1) & 7;

    // Length counters clock at 256 Hz (steps 0, 2, 4, 6)
    if ((m_frame_counter_step & 1) == 0) {
        clock_length_counters();
    }

    // Envelope clocks at 64 Hz (step 7)
    if (m_frame_counter_step == 7) {
        clock_envelopes();
    }

    // Sweep clocks at 128 Hz (steps 2, 6)
    if (m_frame_counter_step == 2 || m_frame_counter_step == 6) {
        clock_sweep();
    }
}

void APU::clock_length_counters() {
    // Pulse 1
    if (m_pulse1.length_enable && m_pulse1.length_counter > 0) {
        m_pulse1.length_counter--;
        if (m_pulse1.length_counter == 0) {
            m_pulse1.enabled = false;
        }
    }

    // Pulse 2
    if (m_pulse2.length_enable && m_pulse2.length_counter > 0) {
        m_pulse2.length_counter--;
        if (m_pulse2.length_counter == 0) {
            m_pulse2.enabled = false;
        }
    }

    // Wave
    if (m_wave.length_enable && m_wave.length_counter > 0) {
        m_wave.length_counter--;
        if (m_wave.length_counter == 0) {
            m_wave.enabled = false;
        }
    }

    // Noise
    if (m_noise.length_enable && m_noise.length_counter > 0) {
        m_noise.length_counter--;
        if (m_noise.length_counter == 0) {
            m_noise.enabled = false;
        }
    }
}

void APU::clock_envelopes() {
    // Pulse 1 - envelope only runs when period > 0
    // Period of 0 means envelope is disabled (volume stays constant)
    if (m_pulse1.envelope_period > 0) {
        if (m_pulse1.envelope_counter > 0) {
            m_pulse1.envelope_counter--;
        }
        if (m_pulse1.envelope_counter == 0) {
            m_pulse1.envelope_counter = m_pulse1.envelope_period;
            if (m_pulse1.envelope_dir && m_pulse1.volume < 15) {
                m_pulse1.volume++;
            } else if (!m_pulse1.envelope_dir && m_pulse1.volume > 0) {
                m_pulse1.volume--;
            }
        }
    }

    // Pulse 2
    if (m_pulse2.envelope_period > 0) {
        if (m_pulse2.envelope_counter > 0) {
            m_pulse2.envelope_counter--;
        }
        if (m_pulse2.envelope_counter == 0) {
            m_pulse2.envelope_counter = m_pulse2.envelope_period;
            if (m_pulse2.envelope_dir && m_pulse2.volume < 15) {
                m_pulse2.volume++;
            } else if (!m_pulse2.envelope_dir && m_pulse2.volume > 0) {
                m_pulse2.volume--;
            }
        }
    }

    // Noise
    if (m_noise.envelope_period > 0) {
        if (m_noise.envelope_counter > 0) {
            m_noise.envelope_counter--;
        }
        if (m_noise.envelope_counter == 0) {
            m_noise.envelope_counter = m_noise.envelope_period;
            if (m_noise.envelope_dir && m_noise.volume < 15) {
                m_noise.volume++;
            } else if (!m_noise.envelope_dir && m_noise.volume > 0) {
                m_noise.volume--;
            }
        }
    }
}

void APU::clock_sweep() {
    if (!m_pulse1.sweep_enabled) return;

    if (m_pulse1.sweep_counter > 0) {
        m_pulse1.sweep_counter--;
    }

    if (m_pulse1.sweep_counter == 0) {
        // Reload counter - period of 0 is treated as 8
        m_pulse1.sweep_counter = m_pulse1.sweep_period > 0 ? m_pulse1.sweep_period : 8;

        // Only perform sweep calculation if period > 0
        if (m_pulse1.sweep_period > 0) {
            // Calculate new frequency
            uint16_t delta = m_pulse1.sweep_shadow >> m_pulse1.sweep_shift;
            uint16_t new_freq;

            if (m_pulse1.sweep_negate) {
                // Subtraction - note that GB uses one's complement, but this shouldn't matter much
                new_freq = m_pulse1.sweep_shadow - delta;
            } else {
                // Addition
                new_freq = m_pulse1.sweep_shadow + delta;
            }

            // Overflow check (frequency > 2047 disables channel)
            if (new_freq > 2047) {
                m_pulse1.enabled = false;
            } else if (m_pulse1.sweep_shift > 0) {
                // Only update frequency if shift > 0
                m_pulse1.sweep_shadow = new_freq;
                m_pulse1.frequency = new_freq;

                // Do another overflow check with the new frequency
                delta = new_freq >> m_pulse1.sweep_shift;
                if (!m_pulse1.sweep_negate) {
                    if (new_freq + delta > 2047) {
                        m_pulse1.enabled = false;
                    }
                }
            }
        }
    }
}

void APU::mix_output(float& left, float& right) {
    left = 0.0f;
    right = 0.0f;

    // Game Boy audio mixing:
    // Each channel outputs a value 0-15 through a DAC which produces analog voltage.
    // For proper audio, we need centered square/wave output.
    //
    // For pulse channels: the waveform alternates between volume level and 0.
    // To center it: output = duty_high ? +amplitude : -amplitude
    // where amplitude = volume / 15.0 (normalized to 0.0 - 1.0)
    //
    // This produces a proper centered square wave with no DC offset.

    // Pulse 1 - check if DAC is enabled (NR12 upper 5 bits != 0)
    bool pulse1_dac = (m_pulse1.envelope_initial > 0) || m_pulse1.envelope_dir;
    if (pulse1_dac && m_pulse1.enabled) {
        // Generate centered square wave: +volume when high, -volume when low
        float amplitude = m_pulse1.volume / 15.0f;
        float sample = s_duty_table[m_pulse1.duty][m_pulse1.sequence_pos] ? amplitude : -amplitude;
        if (m_nr51 & 0x10) left += sample;
        if (m_nr51 & 0x01) right += sample;
    }

    // Pulse 2
    bool pulse2_dac = (m_pulse2.envelope_initial > 0) || m_pulse2.envelope_dir;
    if (pulse2_dac && m_pulse2.enabled) {
        float amplitude = m_pulse2.volume / 15.0f;
        float sample = s_duty_table[m_pulse2.duty][m_pulse2.sequence_pos] ? amplitude : -amplitude;
        if (m_nr51 & 0x20) left += sample;
        if (m_nr51 & 0x02) right += sample;
    }

    // Wave channel - uses 4-bit samples from wave RAM (0-15)
    // Already a waveform, just needs centering around 0
    if (m_wave.dac_enabled && m_wave.enabled) {
        float sample_value = 0.0f;
        if (m_wave.volume_code > 0) {
            // Volume: 0=mute, 1=100%, 2=50%, 3=25%
            int shift = m_wave.volume_code - 1;
            int raw_sample = m_wave.sample_buffer >> shift;
            // Center the 0-15 range around 0: (sample - 7.5) / 7.5
            sample_value = (raw_sample - 7.5f) / 7.5f;
        }
        if (m_nr51 & 0x40) left += sample_value;
        if (m_nr51 & 0x04) right += sample_value;
    }

    // Noise channel - binary output based on LFSR
    bool noise_dac = (m_noise.envelope_initial > 0) || m_noise.envelope_dir;
    if (noise_dac && m_noise.enabled) {
        // LFSR bit 0: 0 = high output, 1 = low output
        // Generate centered noise: +volume when high, -volume when low
        float amplitude = m_noise.volume / 15.0f;
        float sample = (m_noise.lfsr & 1) ? -amplitude : amplitude;
        if (m_nr51 & 0x80) left += sample;
        if (m_nr51 & 0x08) right += sample;
    }

    // Apply master volume (SO1 and SO2)
    // NR50 bits 6-4: left volume (0-7), bits 2-0: right volume (0-7)
    int left_vol = (m_nr50 >> 4) & 7;
    int right_vol = m_nr50 & 7;

    // Scale: divide by 4 channels to prevent clipping, then apply volume (1-8)/8
    // The division by 4 normalizes for when all channels are active
    left = (left / 4.0f) * ((left_vol + 1) / 8.0f);
    right = (right / 4.0f) * ((right_vol + 1) / 8.0f);

    // Apply DMG volume ratio for GBA mode
    if (m_system_type == SystemType::GameBoyAdvance) {
        float dmg_scale = 0.0f;
        switch (m_dmg_volume) {
            case 0: dmg_scale = 0.25f; break;
            case 1: dmg_scale = 0.50f; break;
            case 2: dmg_scale = 1.00f; break;
            case 3: dmg_scale = 0.00f; break;  // Prohibited, treat as mute
        }
        left *= dmg_scale;
        right *= dmg_scale;

        // Add Direct Sound channels (GBA only)
        // Use linear interpolation between samples to reduce aliasing artifacts
        // Direct Sound samples are signed 8-bit, normalize to -1.0 to +1.0

        // Interpolation step: output_rate / typical_input_rate
        // Most GBA games use timer rates around 16384-32768 Hz
        // We use a fixed step that provides smooth interpolation
        constexpr float INTERP_STEP = 0.4f;  // ~44100/16384 samples per step

        // Channel A interpolation
        DSPipe& pipe_a = m_dsound_pipe[0];
        float t_a = std::min(pipe_a.interp_pos, 1.0f);
        float raw_a = pipe_a.prev_sample + (pipe_a.sample - pipe_a.prev_sample) * t_a;
        float ds_a = raw_a / 128.0f;
        pipe_a.interp_pos += INTERP_STEP;

        // Channel B interpolation
        DSPipe& pipe_b = m_dsound_pipe[1];
        float t_b = std::min(pipe_b.interp_pos, 1.0f);
        float raw_b = pipe_b.prev_sample + (pipe_b.sample - pipe_b.prev_sample) * t_b;
        float ds_b = raw_b / 128.0f;
        pipe_b.interp_pos += INTERP_STEP;

        // Apply volume (50% or 100%)
        if (!m_dsound_a_vol) ds_a *= 0.5f;
        if (!m_dsound_b_vol) ds_b *= 0.5f;

        // Mix into left/right channels based on enable flags
        if (m_dsound_a_left) left += ds_a;
        if (m_dsound_a_right) right += ds_a;
        if (m_dsound_b_left) left += ds_b;
        if (m_dsound_b_right) right += ds_b;
    }

    // Clamp to prevent clipping (shouldn't be necessary with proper scaling, but safety)
    left = std::clamp(left, -1.0f, 1.0f);
    right = std::clamp(right, -1.0f, 1.0f);
}

uint8_t APU::read_register(uint16_t address) {
    switch (address & 0xFF) {
        // Pulse 1
        case 0x10: return 0x80 | (m_pulse1.sweep_period << 4) | (m_pulse1.sweep_negate ? 0x08 : 0) | m_pulse1.sweep_shift;
        case 0x11: return (m_pulse1.duty << 6) | 0x3F;
        case 0x12: return (m_pulse1.envelope_initial << 4) | (m_pulse1.envelope_dir ? 0x08 : 0) | m_pulse1.envelope_period;
        case 0x13: return 0xFF;  // Write-only
        case 0x14: return (m_pulse1.length_enable ? 0x40 : 0) | 0xBF;

        // Pulse 2
        case 0x16: return (m_pulse2.duty << 6) | 0x3F;
        case 0x17: return (m_pulse2.envelope_initial << 4) | (m_pulse2.envelope_dir ? 0x08 : 0) | m_pulse2.envelope_period;
        case 0x18: return 0xFF;
        case 0x19: return (m_pulse2.length_enable ? 0x40 : 0) | 0xBF;

        // Wave
        case 0x1A: return (m_wave.dac_enabled ? 0x80 : 0) | 0x7F;
        case 0x1B: return 0xFF;
        case 0x1C: return (m_wave.volume_code << 5) | 0x9F;
        case 0x1D: return 0xFF;
        case 0x1E: return (m_wave.length_enable ? 0x40 : 0) | 0xBF;

        // Noise
        case 0x20: return 0xFF;
        case 0x21: return (m_noise.envelope_initial << 4) | (m_noise.envelope_dir ? 0x08 : 0) | m_noise.envelope_period;
        case 0x22: return (m_noise.clock_shift << 4) | (m_noise.width_mode ? 0x08 : 0) | m_noise.divisor_code;
        case 0x23: return (m_noise.length_enable ? 0x40 : 0) | 0xBF;

        // Control
        case 0x24: return m_nr50;
        case 0x25: return m_nr51;
        case 0x26:
            return (m_enabled ? 0x80 : 0) |
                   (m_pulse1.enabled ? 0x01 : 0) |
                   (m_pulse2.enabled ? 0x02 : 0) |
                   (m_wave.enabled ? 0x04 : 0) |
                   (m_noise.enabled ? 0x08 : 0) | 0x70;

        // Wave RAM
        case 0x30 ... 0x3F:
            return m_wave.wave_ram[(address & 0xFF) - 0x30];

        default:
            return 0xFF;
    }
}

void APU::write_register(uint16_t address, uint8_t value) {
    // If APU is disabled, only NR52 can be written
    if (!m_enabled && (address & 0xFF) != 0x26) {
        return;
    }

    switch (address & 0xFF) {
        // Pulse 1
        case 0x10:
            m_pulse1.sweep_period = (value >> 4) & 7;
            m_pulse1.sweep_negate = value & 0x08;
            m_pulse1.sweep_shift = value & 7;
            break;
        case 0x11:
            m_pulse1.duty = (value >> 6) & 3;
            m_pulse1.length_counter = 64 - (value & 0x3F);
            break;
        case 0x12:
            m_pulse1.envelope_initial = (value >> 4) & 0xF;
            m_pulse1.envelope_dir = value & 0x08;
            m_pulse1.envelope_period = value & 7;
            if ((value & 0xF8) == 0) {
                m_pulse1.enabled = false;
            }
            break;
        case 0x13:
            m_pulse1.frequency = (m_pulse1.frequency & 0x700) | value;
            break;
        case 0x14:
            m_pulse1.frequency = (m_pulse1.frequency & 0xFF) | ((value & 7) << 8);
            m_pulse1.length_enable = value & 0x40;
            if (value & 0x80) {
                // Trigger - only enable if DAC is on
                bool dac_on = (m_pulse1.envelope_initial > 0) || m_pulse1.envelope_dir;
                m_pulse1.enabled = dac_on;
                if (m_pulse1.length_counter == 0) m_pulse1.length_counter = 64;
                m_pulse1.timer = (2048 - m_pulse1.frequency) * 4;
                m_pulse1.volume = m_pulse1.envelope_initial;
                m_pulse1.envelope_counter = m_pulse1.envelope_period > 0 ? m_pulse1.envelope_period : 8;
                m_pulse1.sweep_shadow = m_pulse1.frequency;
                m_pulse1.sweep_counter = m_pulse1.sweep_period > 0 ? m_pulse1.sweep_period : 8;
                m_pulse1.sweep_enabled = m_pulse1.sweep_period > 0 || m_pulse1.sweep_shift > 0;

                // Perform overflow check on trigger if shift > 0
                if (m_pulse1.sweep_shift > 0) {
                    uint16_t delta = m_pulse1.frequency >> m_pulse1.sweep_shift;
                    if (!m_pulse1.sweep_negate && m_pulse1.frequency + delta > 2047) {
                        m_pulse1.enabled = false;
                    }
                }
            }
            break;

        // Pulse 2
        case 0x16:
            m_pulse2.duty = (value >> 6) & 3;
            m_pulse2.length_counter = 64 - (value & 0x3F);
            break;
        case 0x17:
            m_pulse2.envelope_initial = (value >> 4) & 0xF;
            m_pulse2.envelope_dir = value & 0x08;
            m_pulse2.envelope_period = value & 7;
            if ((value & 0xF8) == 0) {
                m_pulse2.enabled = false;
            }
            break;
        case 0x18:
            m_pulse2.frequency = (m_pulse2.frequency & 0x700) | value;
            break;
        case 0x19:
            m_pulse2.frequency = (m_pulse2.frequency & 0xFF) | ((value & 7) << 8);
            m_pulse2.length_enable = value & 0x40;
            if (value & 0x80) {
                // Trigger - only enable if DAC is on
                bool dac_on = (m_pulse2.envelope_initial > 0) || m_pulse2.envelope_dir;
                m_pulse2.enabled = dac_on;
                if (m_pulse2.length_counter == 0) m_pulse2.length_counter = 64;
                m_pulse2.timer = (2048 - m_pulse2.frequency) * 4;
                m_pulse2.volume = m_pulse2.envelope_initial;
                m_pulse2.envelope_counter = m_pulse2.envelope_period > 0 ? m_pulse2.envelope_period : 8;
            }
            break;

        // Wave
        case 0x1A:
            m_wave.dac_enabled = value & 0x80;
            if (!m_wave.dac_enabled) m_wave.enabled = false;
            break;
        case 0x1B:
            m_wave.length_counter = 256 - value;
            break;
        case 0x1C:
            m_wave.volume_code = (value >> 5) & 3;
            break;
        case 0x1D:
            m_wave.frequency = (m_wave.frequency & 0x700) | value;
            break;
        case 0x1E:
            m_wave.frequency = (m_wave.frequency & 0xFF) | ((value & 7) << 8);
            m_wave.length_enable = value & 0x40;
            if (value & 0x80) {
                m_wave.enabled = m_wave.dac_enabled;
                if (m_wave.length_counter == 0) m_wave.length_counter = 256;
                m_wave.timer = (2048 - m_wave.frequency) * 2;
                m_wave.position = 0;
            }
            break;

        // Noise
        case 0x20:
            m_noise.length_counter = 64 - (value & 0x3F);
            break;
        case 0x21:
            m_noise.envelope_initial = (value >> 4) & 0xF;
            m_noise.envelope_dir = value & 0x08;
            m_noise.envelope_period = value & 7;
            if ((value & 0xF8) == 0) {
                m_noise.enabled = false;
            }
            break;
        case 0x22:
            m_noise.clock_shift = (value >> 4) & 0xF;
            m_noise.width_mode = value & 0x08;
            m_noise.divisor_code = value & 7;
            break;
        case 0x23:
            m_noise.length_enable = value & 0x40;
            if (value & 0x80) {
                // Trigger - only enable if DAC is on
                bool dac_on = (m_noise.envelope_initial > 0) || m_noise.envelope_dir;
                m_noise.enabled = dac_on;
                if (m_noise.length_counter == 0) m_noise.length_counter = 64;
                uint16_t divisor = m_noise.divisor_code == 0 ? 8 : (m_noise.divisor_code * 16);
                m_noise.timer = divisor << m_noise.clock_shift;
                m_noise.volume = m_noise.envelope_initial;
                m_noise.envelope_counter = m_noise.envelope_period > 0 ? m_noise.envelope_period : 8;
                m_noise.lfsr = 0x7FFF;
            }
            break;

        // Control
        case 0x24:
            m_nr50 = value;
            break;
        case 0x25:
            m_nr51 = value;
            break;
        case 0x26:
            m_enabled = value & 0x80;
            if (!m_enabled) {
                // Disable all channels
                m_pulse1.enabled = false;
                m_pulse2.enabled = false;
                m_wave.enabled = false;
                m_noise.enabled = false;
            }
            break;

        // Wave RAM
        case 0x30 ... 0x3F:
            m_wave.wave_ram[(address & 0xFF) - 0x30] = value;
            break;
    }
}

size_t APU::get_samples(float* buffer, size_t max_samples) {
    size_t samples = std::min(m_audio_write_pos, max_samples);
    std::memcpy(buffer, m_audio_buffer.data(), samples * 2 * sizeof(float));

    // Debug logging once per second (every 60 frames)
    if (is_debug_mode()) {
        s_debug_total_samples += samples;
        s_debug_frame_count++;

        if (s_debug_frame_count % 60 == 0) {
            float avg_samples = static_cast<float>(s_debug_total_samples) / 60.0f;
            fprintf(stderr, "[APU] Avg samples/frame: %.1f (expected ~735 for GB)\n", avg_samples);
            fprintf(stderr, "[APU] CH1: enabled=%d vol=%d freq=%d duty=%d timer=%d\n",
                   m_pulse1.enabled, m_pulse1.volume, m_pulse1.frequency,
                   m_pulse1.duty, m_pulse1.timer);
            fprintf(stderr, "[APU] CH2: enabled=%d vol=%d freq=%d duty=%d\n",
                   m_pulse2.enabled, m_pulse2.volume, m_pulse2.frequency, m_pulse2.duty);
            fprintf(stderr, "[APU] CH3 (wave): enabled=%d dac=%d vol_code=%d freq=%d\n",
                   m_wave.enabled, m_wave.dac_enabled, m_wave.volume_code, m_wave.frequency);
            fprintf(stderr, "[APU] CH4 (noise): enabled=%d vol=%d lfsr=0x%04X\n",
                   m_noise.enabled, m_noise.volume, m_noise.lfsr);
            fprintf(stderr, "[APU] NR50=0x%02X NR51=0x%02X NR52=0x%02X enabled=%d\n",
                   m_nr50, m_nr51, m_nr52, m_enabled);
            s_debug_total_samples = 0;
        }
    }

    m_audio_write_pos = 0;
    return samples;
}

void APU::save_state(std::vector<uint8_t>& data) {
    // Save control registers
    data.push_back(m_nr50);
    data.push_back(m_nr51);
    data.push_back(m_nr52);
    data.push_back(m_enabled ? 1 : 0);

    // Save wave RAM
    data.insert(data.end(), m_wave.wave_ram.begin(), m_wave.wave_ram.end());

    // Simplified - full state would save all channel data
}

void APU::load_state(const uint8_t*& data, size_t& remaining) {
    m_nr50 = *data++; remaining--;
    m_nr51 = *data++; remaining--;
    m_nr52 = *data++; remaining--;
    m_enabled = (*data++ != 0); remaining--;

    std::memcpy(m_wave.wave_ram.data(), data, 16);
    data += 16;
    remaining -= 16;
}

// ============================================================================
// Direct Sound Implementation (GBA-specific)
// ============================================================================

void APU::write_fifo_a(uint32_t value) {
    m_dsound_fifo[0].write_word(value);
}

void APU::write_fifo_b(uint32_t value) {
    m_dsound_fifo[1].write_word(value);
}

void APU::write_soundcnt_h(uint16_t value) {
    m_soundcnt_h = value;

    // Bits 0-1: DMG volume ratio (0=25%, 1=50%, 2=100%, 3=prohibited)
    m_dmg_volume = value & 3;

    // Bit 2: Direct Sound A volume (0=50%, 1=100%)
    m_dsound_a_vol = (value & 0x04) != 0;

    // Bit 3: Direct Sound B volume (0=50%, 1=100%)
    m_dsound_b_vol = (value & 0x08) != 0;

    // Bit 8: Direct Sound A enable right
    m_dsound_a_right = (value & 0x100) != 0;

    // Bit 9: Direct Sound A enable left
    m_dsound_a_left = (value & 0x200) != 0;

    // Bit 10: Direct Sound A timer select (0=Timer0, 1=Timer1)
    m_dsound_a_timer = (value & 0x400) ? 1 : 0;

    // Bit 11: Direct Sound A FIFO reset
    if (value & 0x800) {
        m_dsound_fifo[0].reset();
        m_dsound_pipe[0] = {};
    }

    // Bit 12: Direct Sound B enable right
    m_dsound_b_right = (value & 0x1000) != 0;

    // Bit 13: Direct Sound B enable left
    m_dsound_b_left = (value & 0x2000) != 0;

    // Bit 14: Direct Sound B timer select (0=Timer0, 1=Timer1)
    m_dsound_b_timer = (value & 0x4000) ? 1 : 0;

    // Bit 15: Direct Sound B FIFO reset
    if (value & 0x8000) {
        m_dsound_fifo[1].reset();
        m_dsound_pipe[1] = {};
    }

    if (is_debug_mode()) {
        fprintf(stderr, "[APU] SOUNDCNT_H = 0x%04X: DMG_vol=%d, A_vol=%d%%, A_timer=%d, B_vol=%d%%, B_timer=%d\n",
                value, m_dmg_volume, m_dsound_a_vol ? 100 : 50, m_dsound_a_timer,
                m_dsound_b_vol ? 100 : 50, m_dsound_b_timer);
    }
}

uint16_t APU::read_soundcnt_h() const {
    // Reconstruct from state (reset bits 11 and 15 are write-only)
    uint16_t value = m_dmg_volume;
    if (m_dsound_a_vol) value |= 0x04;
    if (m_dsound_b_vol) value |= 0x08;
    if (m_dsound_a_right) value |= 0x100;
    if (m_dsound_a_left) value |= 0x200;
    if (m_dsound_a_timer) value |= 0x400;
    if (m_dsound_b_right) value |= 0x1000;
    if (m_dsound_b_left) value |= 0x2000;
    if (m_dsound_b_timer) value |= 0x4000;
    return value;
}

void APU::consume_fifo_sample(int idx) {
    DSPipe& pipe = m_dsound_pipe[idx];
    DSFIFO& fifo = m_dsound_fifo[idx];

    // If pipe is empty, try to refill from FIFO
    if (pipe.bytes_left == 0) {
        if (!fifo.empty()) {
            pipe.word = fifo.read_word();
            pipe.bytes_left = 4;
        } else {
            // FIFO underrun - output silence (or last sample)
            pipe.prev_sample = pipe.sample;
            pipe.sample = 0;
            pipe.interp_pos = 0.0f;
            return;
        }
    }

    // Save previous sample for interpolation
    pipe.prev_sample = pipe.sample;

    // Consume one byte from the pipe (samples are signed 8-bit)
    pipe.sample = static_cast<int8_t>(pipe.word & 0xFF);
    pipe.word >>= 8;
    pipe.bytes_left--;

    // Reset interpolation position - we just got a new sample
    pipe.interp_pos = 0.0f;

    // Request DMA refill when FIFO is half-empty (4 or fewer words)
    if (fifo.size() <= 4 && m_request_fifo_dma) {
        m_request_fifo_dma(idx);
    }
}

void APU::on_timer_overflow(int timer_id) {
    // Check if Direct Sound A uses this timer
    if (m_dsound_a_timer == timer_id) {
        consume_fifo_sample(0);
    }

    // Check if Direct Sound B uses this timer
    if (m_dsound_b_timer == timer_id) {
        consume_fifo_sample(1);
    }
}

} // namespace gba
