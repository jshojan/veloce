#include "apu.hpp"
#include "bus.hpp"

#include <cstring>

namespace nes {

const uint8_t APU::s_length_table[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

const uint16_t APU::s_noise_period_table[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

const uint8_t APU::s_duty_table[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1}
};

const uint8_t APU::s_triangle_table[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
     0,  1,  2,  3,  4,  5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

APU::APU(Bus& bus) : m_bus(bus) {
    reset();
}

APU::~APU() = default;

void APU::reset() {
    m_frame_counter_mode = 0;
    m_frame_counter_step = 0;
    m_frame_counter_cycles = 0;
    m_irq_inhibit = false;
    m_frame_irq = false;

    m_pulse[0] = Pulse{};
    m_pulse[1] = Pulse{};
    m_triangle = Triangle{};
    m_noise = Noise{};
    m_noise.shift_register = 1;
    m_dmc = DMC{};

    m_audio_write_pos = 0;
    m_cycles = 0;
    m_sample_counter = 0;
    m_filter_state = 0.0f;
    m_sample_accumulator = 0.0f;
    m_sample_count = 0;
}

void APU::step(int cpu_cycles) {
    for (int i = 0; i < cpu_cycles; i++) {
        m_cycles++;

        // Clock triangle timer every CPU cycle
        if (m_triangle.timer == 0) {
            m_triangle.timer = m_triangle.timer_period;
            if (m_triangle.length_counter > 0 && m_triangle.linear_counter > 0) {
                m_triangle.sequence_pos = (m_triangle.sequence_pos + 1) & 31;
            }
        } else {
            m_triangle.timer--;
        }

        // Clock pulse and noise every 2 CPU cycles
        if ((m_cycles & 1) == 0) {
            for (int p = 0; p < 2; p++) {
                if (m_pulse[p].timer == 0) {
                    m_pulse[p].timer = m_pulse[p].timer_period;
                    m_pulse[p].sequence_pos = (m_pulse[p].sequence_pos + 1) & 7;
                } else {
                    m_pulse[p].timer--;
                }
            }

            if (m_noise.timer == 0) {
                m_noise.timer = m_noise.timer_period;
                uint16_t bit = m_noise.mode ?
                    ((m_noise.shift_register >> 6) ^ m_noise.shift_register) & 1 :
                    ((m_noise.shift_register >> 1) ^ m_noise.shift_register) & 1;
                m_noise.shift_register = (m_noise.shift_register >> 1) | (bit << 14);
            } else {
                m_noise.timer--;
            }
        }

        // Frame counter - more accurate step timing
        // 4-step mode: steps at 7457, 14913, 22371, 29829 (29830 resets)
        // 5-step mode: steps at 7457, 14913, 22371, 29829, 37281 (37282 resets)
        m_frame_counter_cycles++;
        bool should_clock = false;

        if (m_frame_counter_mode == 0) {
            // 4-step mode
            should_clock = (m_frame_counter_cycles == 7457 ||
                           m_frame_counter_cycles == 14913 ||
                           m_frame_counter_cycles == 22371 ||
                           m_frame_counter_cycles == 29829);
            if (m_frame_counter_cycles >= 29830) {
                m_frame_counter_cycles = 0;
            }
        } else {
            // 5-step mode
            should_clock = (m_frame_counter_cycles == 7457 ||
                           m_frame_counter_cycles == 14913 ||
                           m_frame_counter_cycles == 22371 ||
                           m_frame_counter_cycles == 37281);
            // Note: 29829 step is skipped in 5-step mode (no clock, just wait)
            if (m_frame_counter_cycles >= 37282) {
                m_frame_counter_cycles = 0;
            }
        }

        if (should_clock) {
            clock_frame_counter();
        }

        // Accumulate samples for averaging (anti-aliasing)
        float raw_sample = mix_output();
        m_sample_accumulator += raw_sample;
        m_sample_count++;

        // Output sample at target rate
        m_sample_counter += SAMPLE_RATE;
        if (m_sample_counter >= CPU_FREQ) {
            m_sample_counter -= CPU_FREQ;

            // Average accumulated samples (with safety check)
            float sample = (m_sample_count > 0) ? (m_sample_accumulator / m_sample_count) : 0.0f;
            m_sample_accumulator = 0.0f;
            m_sample_count = 0;

            // Apply low-pass filter (first-order IIR)
            m_filter_state = m_filter_state + FILTER_ALPHA * (sample - m_filter_state);
            sample = m_filter_state;

            // Write sample to buffer, wrapping around if buffer is full
            // This prevents audio dropouts by maintaining continuous output
            if (m_audio_write_pos < AUDIO_BUFFER_SIZE * 2 - 1) {
                m_audio_buffer[m_audio_write_pos++] = sample;
                m_audio_buffer[m_audio_write_pos++] = sample;  // Stereo
            } else {
                // Buffer full - this indicates the consumer isn't keeping up
                // Keep the most recent samples by shifting buffer contents
                // This is expensive but preferable to audio glitches
                size_t shift_amount = AUDIO_BUFFER_SIZE / 2;  // Shift half the buffer
                for (size_t j = 0; j < AUDIO_BUFFER_SIZE * 2 - shift_amount * 2; j++) {
                    m_audio_buffer[j] = m_audio_buffer[j + shift_amount * 2];
                }
                m_audio_write_pos = AUDIO_BUFFER_SIZE * 2 - shift_amount * 2;
                m_audio_buffer[m_audio_write_pos++] = sample;
                m_audio_buffer[m_audio_write_pos++] = sample;
            }
        }
    }
}

void APU::clock_frame_counter() {
    m_frame_counter_step++;

    if (m_frame_counter_mode == 0) {
        // 4-step mode: envelope/linear counter on all steps, length/sweep on steps 2 and 4
        // Step 1 (7457):   envelope, linear counter
        // Step 2 (14913):  envelope, linear counter, length, sweep
        // Step 3 (22371):  envelope, linear counter
        // Step 4 (29829):  envelope, linear counter, length, sweep, IRQ
        clock_envelopes();
        if (m_frame_counter_step == 2 || m_frame_counter_step == 4) {
            clock_length_counters();
            clock_sweeps();
        }
        if (m_frame_counter_step >= 4) {
            m_frame_counter_step = 0;
            if (!m_irq_inhibit) {
                m_frame_irq = true;
            }
        }
    } else {
        // 5-step mode: same as 4-step but step 4 is skipped, step 5 clocks length/sweep
        // Step 1 (7457):   envelope, linear counter
        // Step 2 (14913):  envelope, linear counter, length, sweep
        // Step 3 (22371):  envelope, linear counter
        // Step 4 (29829):  (skipped - no clock happens here)
        // Step 5 (37281):  envelope, linear counter, length, sweep (no IRQ)
        clock_envelopes();
        if (m_frame_counter_step == 2 || m_frame_counter_step == 4) {
            clock_length_counters();
            clock_sweeps();
        }
        if (m_frame_counter_step >= 4) {
            m_frame_counter_step = 0;
        }
    }
}

void APU::clock_length_counters() {
    for (int p = 0; p < 2; p++) {
        if (!m_pulse[p].length_halt && m_pulse[p].length_counter > 0) {
            m_pulse[p].length_counter--;
        }
    }

    if (!m_triangle.control_flag && m_triangle.length_counter > 0) {
        m_triangle.length_counter--;
    }

    if (!m_noise.length_halt && m_noise.length_counter > 0) {
        m_noise.length_counter--;
    }
}

void APU::clock_envelopes() {
    for (int p = 0; p < 2; p++) {
        if (m_pulse[p].envelope_start) {
            m_pulse[p].envelope_start = false;
            m_pulse[p].envelope_counter = 15;
            m_pulse[p].envelope_divider = m_pulse[p].volume;
        } else if (m_pulse[p].envelope_divider == 0) {
            m_pulse[p].envelope_divider = m_pulse[p].volume;
            if (m_pulse[p].envelope_counter > 0) {
                m_pulse[p].envelope_counter--;
            } else if (m_pulse[p].length_halt) {
                m_pulse[p].envelope_counter = 15;
            }
        } else {
            m_pulse[p].envelope_divider--;
        }
    }

    if (m_noise.envelope_start) {
        m_noise.envelope_start = false;
        m_noise.envelope_counter = 15;
        m_noise.envelope_divider = m_noise.volume;
    } else if (m_noise.envelope_divider == 0) {
        m_noise.envelope_divider = m_noise.volume;
        if (m_noise.envelope_counter > 0) {
            m_noise.envelope_counter--;
        } else if (m_noise.length_halt) {
            m_noise.envelope_counter = 15;
        }
    } else {
        m_noise.envelope_divider--;
    }

    // Triangle linear counter
    if (m_triangle.linear_counter_reload_flag) {
        m_triangle.linear_counter = m_triangle.linear_counter_reload;
    } else if (m_triangle.linear_counter > 0) {
        m_triangle.linear_counter--;
    }
    if (!m_triangle.control_flag) {
        m_triangle.linear_counter_reload_flag = false;
    }
}

void APU::clock_sweeps() {
    for (int p = 0; p < 2; p++) {
        if (m_pulse[p].sweep_divider == 0 && m_pulse[p].sweep_enabled) {
            uint16_t change = m_pulse[p].timer_period >> m_pulse[p].sweep_shift;
            if (m_pulse[p].sweep_negate) {
                m_pulse[p].timer_period -= change;
                if (p == 0) m_pulse[p].timer_period--;
            } else {
                m_pulse[p].timer_period += change;
            }
        }

        if (m_pulse[p].sweep_divider == 0 || m_pulse[p].sweep_reload) {
            m_pulse[p].sweep_divider = m_pulse[p].sweep_period;
            m_pulse[p].sweep_reload = false;
        } else {
            m_pulse[p].sweep_divider--;
        }
    }
}

float APU::mix_output() {
    float pulse_out = 0;
    float tnd_out = 0;

    // Pulse channels
    for (int p = 0; p < 2; p++) {
        if (m_pulse[p].length_counter > 0 && m_pulse[p].timer_period >= 8 &&
            m_pulse[p].timer_period <= 0x7FF) {
            uint8_t volume = m_pulse[p].constant_volume ?
                m_pulse[p].volume : m_pulse[p].envelope_counter;
            if (s_duty_table[m_pulse[p].duty][m_pulse[p].sequence_pos]) {
                pulse_out += volume;
            }
        }
    }
    pulse_out = 0.00752f * pulse_out;

    // Triangle
    float triangle = 0;
    if (m_triangle.length_counter > 0 && m_triangle.linear_counter > 0 &&
        m_triangle.timer_period >= 2) {
        triangle = s_triangle_table[m_triangle.sequence_pos];
    }

    // Noise
    float noise = 0;
    if (m_noise.length_counter > 0 && !(m_noise.shift_register & 1)) {
        noise = m_noise.constant_volume ? m_noise.volume : m_noise.envelope_counter;
    }

    // DMC (simplified)
    float dmc = m_dmc.output_level;

    tnd_out = 0.00851f * triangle + 0.00494f * noise + 0.00335f * dmc;

    return pulse_out + tnd_out;
}

uint8_t APU::cpu_read(uint16_t address) {
    if (address == 0x4015) {
        uint8_t status = 0;
        if (m_pulse[0].length_counter > 0) status |= 0x01;
        if (m_pulse[1].length_counter > 0) status |= 0x02;
        if (m_triangle.length_counter > 0) status |= 0x04;
        if (m_noise.length_counter > 0) status |= 0x08;
        if (m_frame_irq) status |= 0x40;
        m_frame_irq = false;
        return status;
    }
    return 0;
}

void APU::cpu_write(uint16_t address, uint8_t value) {
    switch (address) {
        // Pulse 1
        case 0x4000:
            m_pulse[0].duty = (value >> 6) & 3;
            m_pulse[0].length_halt = (value & 0x20) != 0;
            m_pulse[0].constant_volume = (value & 0x10) != 0;
            m_pulse[0].volume = value & 0x0F;
            break;
        case 0x4001:
            m_pulse[0].sweep_enabled = (value & 0x80) != 0;
            m_pulse[0].sweep_period = (value >> 4) & 7;
            m_pulse[0].sweep_negate = (value & 0x08) != 0;
            m_pulse[0].sweep_shift = value & 7;
            m_pulse[0].sweep_reload = true;
            break;
        case 0x4002:
            m_pulse[0].timer_period = (m_pulse[0].timer_period & 0x700) | value;
            break;
        case 0x4003:
            m_pulse[0].timer_period = (m_pulse[0].timer_period & 0xFF) | ((value & 7) << 8);
            m_pulse[0].length_counter = s_length_table[value >> 3];
            m_pulse[0].sequence_pos = 0;
            m_pulse[0].envelope_start = true;
            break;

        // Pulse 2
        case 0x4004:
            m_pulse[1].duty = (value >> 6) & 3;
            m_pulse[1].length_halt = (value & 0x20) != 0;
            m_pulse[1].constant_volume = (value & 0x10) != 0;
            m_pulse[1].volume = value & 0x0F;
            break;
        case 0x4005:
            m_pulse[1].sweep_enabled = (value & 0x80) != 0;
            m_pulse[1].sweep_period = (value >> 4) & 7;
            m_pulse[1].sweep_negate = (value & 0x08) != 0;
            m_pulse[1].sweep_shift = value & 7;
            m_pulse[1].sweep_reload = true;
            break;
        case 0x4006:
            m_pulse[1].timer_period = (m_pulse[1].timer_period & 0x700) | value;
            break;
        case 0x4007:
            m_pulse[1].timer_period = (m_pulse[1].timer_period & 0xFF) | ((value & 7) << 8);
            m_pulse[1].length_counter = s_length_table[value >> 3];
            m_pulse[1].sequence_pos = 0;
            m_pulse[1].envelope_start = true;
            break;

        // Triangle
        case 0x4008:
            m_triangle.control_flag = (value & 0x80) != 0;
            m_triangle.linear_counter_reload = value & 0x7F;
            break;
        case 0x400A:
            m_triangle.timer_period = (m_triangle.timer_period & 0x700) | value;
            break;
        case 0x400B:
            m_triangle.timer_period = (m_triangle.timer_period & 0xFF) | ((value & 7) << 8);
            m_triangle.length_counter = s_length_table[value >> 3];
            m_triangle.linear_counter_reload_flag = true;
            break;

        // Noise
        case 0x400C:
            m_noise.length_halt = (value & 0x20) != 0;
            m_noise.constant_volume = (value & 0x10) != 0;
            m_noise.volume = value & 0x0F;
            break;
        case 0x400E:
            m_noise.mode = (value & 0x80) != 0;
            m_noise.timer_period = s_noise_period_table[value & 0x0F];
            break;
        case 0x400F:
            m_noise.length_counter = s_length_table[value >> 3];
            m_noise.envelope_start = true;
            break;

        // DMC
        case 0x4010:
            m_dmc.irq_enabled = (value & 0x80) != 0;
            m_dmc.loop = (value & 0x40) != 0;
            m_dmc.rate = value & 0x0F;
            break;
        case 0x4011:
            m_dmc.output_level = value & 0x7F;
            break;
        case 0x4012:
            m_dmc.sample_address = 0xC000 | (value << 6);
            break;
        case 0x4013:
            m_dmc.sample_length = (value << 4) + 1;
            break;

        // Status
        case 0x4015:
            m_pulse[0].enabled = (value & 0x01) != 0;
            m_pulse[1].enabled = (value & 0x02) != 0;
            m_triangle.enabled = (value & 0x04) != 0;
            m_noise.enabled = (value & 0x08) != 0;
            m_dmc.enabled = (value & 0x10) != 0;

            if (!m_pulse[0].enabled) m_pulse[0].length_counter = 0;
            if (!m_pulse[1].enabled) m_pulse[1].length_counter = 0;
            if (!m_triangle.enabled) m_triangle.length_counter = 0;
            if (!m_noise.enabled) m_noise.length_counter = 0;
            break;

        // Frame counter
        case 0x4017:
            m_frame_counter_mode = (value & 0x80) ? 1 : 0;
            m_irq_inhibit = (value & 0x40) != 0;
            if (m_irq_inhibit) m_frame_irq = false;
            m_frame_counter_step = 0;
            if (m_frame_counter_mode == 1) {
                clock_envelopes();
                clock_length_counters();
                clock_sweeps();
            }
            break;
    }
}

size_t APU::get_samples(float* buffer, size_t max_samples) {
    size_t samples = m_audio_write_pos / 2;
    if (samples > max_samples) samples = max_samples;

    // Copy requested samples to output buffer
    size_t samples_to_copy = samples * 2;
    for (size_t i = 0; i < samples_to_copy; i++) {
        buffer[i] = m_audio_buffer[i];
    }

    // Move remaining samples to the beginning of the buffer
    // This prevents audio discontinuities when buffer isn't fully consumed
    size_t remaining = m_audio_write_pos - samples_to_copy;
    if (remaining > 0) {
        for (size_t i = 0; i < remaining; i++) {
            m_audio_buffer[i] = m_audio_buffer[samples_to_copy + i];
        }
    }
    m_audio_write_pos = remaining;

    return samples;
}

// Serialization helpers
namespace {
    template<typename T>
    void write_value(std::vector<uint8_t>& data, T value) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
        data.insert(data.end(), ptr, ptr + sizeof(T));
    }

    template<typename T>
    bool read_value(const uint8_t*& data, size_t& remaining, T& value) {
        if (remaining < sizeof(T)) return false;
        std::memcpy(&value, data, sizeof(T));
        data += sizeof(T);
        remaining -= sizeof(T);
        return true;
    }
}

void APU::save_state(std::vector<uint8_t>& data) {
    // Frame counter
    write_value(data, m_frame_counter_mode);
    write_value(data, m_frame_counter_step);
    write_value(data, m_frame_counter_cycles);
    write_value(data, static_cast<uint8_t>(m_irq_inhibit ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_frame_irq ? 1 : 0));

    // Pulse channels
    for (int i = 0; i < 2; i++) {
        write_value(data, static_cast<uint8_t>(m_pulse[i].enabled ? 1 : 0));
        write_value(data, m_pulse[i].duty);
        write_value(data, static_cast<uint8_t>(m_pulse[i].length_halt ? 1 : 0));
        write_value(data, static_cast<uint8_t>(m_pulse[i].constant_volume ? 1 : 0));
        write_value(data, m_pulse[i].volume);
        write_value(data, static_cast<uint8_t>(m_pulse[i].sweep_enabled ? 1 : 0));
        write_value(data, m_pulse[i].sweep_period);
        write_value(data, static_cast<uint8_t>(m_pulse[i].sweep_negate ? 1 : 0));
        write_value(data, m_pulse[i].sweep_shift);
        write_value(data, m_pulse[i].timer_period);
        write_value(data, m_pulse[i].timer);
        write_value(data, m_pulse[i].sequence_pos);
        write_value(data, m_pulse[i].length_counter);
        write_value(data, m_pulse[i].envelope_counter);
        write_value(data, m_pulse[i].envelope_divider);
        write_value(data, static_cast<uint8_t>(m_pulse[i].envelope_start ? 1 : 0));
        write_value(data, m_pulse[i].sweep_divider);
        write_value(data, static_cast<uint8_t>(m_pulse[i].sweep_reload ? 1 : 0));
    }

    // Triangle channel
    write_value(data, static_cast<uint8_t>(m_triangle.enabled ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_triangle.control_flag ? 1 : 0));
    write_value(data, m_triangle.linear_counter_reload);
    write_value(data, m_triangle.timer_period);
    write_value(data, m_triangle.timer);
    write_value(data, m_triangle.sequence_pos);
    write_value(data, m_triangle.length_counter);
    write_value(data, m_triangle.linear_counter);
    write_value(data, static_cast<uint8_t>(m_triangle.linear_counter_reload_flag ? 1 : 0));

    // Noise channel
    write_value(data, static_cast<uint8_t>(m_noise.enabled ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_noise.length_halt ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_noise.constant_volume ? 1 : 0));
    write_value(data, m_noise.volume);
    write_value(data, static_cast<uint8_t>(m_noise.mode ? 1 : 0));
    write_value(data, m_noise.timer_period);
    write_value(data, m_noise.timer);
    write_value(data, m_noise.shift_register);
    write_value(data, m_noise.length_counter);
    write_value(data, m_noise.envelope_counter);
    write_value(data, m_noise.envelope_divider);
    write_value(data, static_cast<uint8_t>(m_noise.envelope_start ? 1 : 0));

    // DMC channel
    write_value(data, static_cast<uint8_t>(m_dmc.enabled ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_dmc.irq_enabled ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_dmc.loop ? 1 : 0));
    write_value(data, m_dmc.rate);
    write_value(data, m_dmc.output_level);
    write_value(data, m_dmc.sample_address);
    write_value(data, m_dmc.sample_length);

    // Timing
    write_value(data, m_cycles);
    write_value(data, m_sample_counter);
    write_value(data, m_filter_state);
}

void APU::load_state(const uint8_t*& data, size_t& remaining) {
    // Frame counter
    read_value(data, remaining, m_frame_counter_mode);
    read_value(data, remaining, m_frame_counter_step);
    read_value(data, remaining, m_frame_counter_cycles);
    uint8_t flag;
    read_value(data, remaining, flag);
    m_irq_inhibit = flag != 0;
    read_value(data, remaining, flag);
    m_frame_irq = flag != 0;

    // Pulse channels
    for (int i = 0; i < 2; i++) {
        read_value(data, remaining, flag);
        m_pulse[i].enabled = flag != 0;
        read_value(data, remaining, m_pulse[i].duty);
        read_value(data, remaining, flag);
        m_pulse[i].length_halt = flag != 0;
        read_value(data, remaining, flag);
        m_pulse[i].constant_volume = flag != 0;
        read_value(data, remaining, m_pulse[i].volume);
        read_value(data, remaining, flag);
        m_pulse[i].sweep_enabled = flag != 0;
        read_value(data, remaining, m_pulse[i].sweep_period);
        read_value(data, remaining, flag);
        m_pulse[i].sweep_negate = flag != 0;
        read_value(data, remaining, m_pulse[i].sweep_shift);
        read_value(data, remaining, m_pulse[i].timer_period);
        read_value(data, remaining, m_pulse[i].timer);
        read_value(data, remaining, m_pulse[i].sequence_pos);
        read_value(data, remaining, m_pulse[i].length_counter);
        read_value(data, remaining, m_pulse[i].envelope_counter);
        read_value(data, remaining, m_pulse[i].envelope_divider);
        read_value(data, remaining, flag);
        m_pulse[i].envelope_start = flag != 0;
        read_value(data, remaining, m_pulse[i].sweep_divider);
        read_value(data, remaining, flag);
        m_pulse[i].sweep_reload = flag != 0;
    }

    // Triangle channel
    read_value(data, remaining, flag);
    m_triangle.enabled = flag != 0;
    read_value(data, remaining, flag);
    m_triangle.control_flag = flag != 0;
    read_value(data, remaining, m_triangle.linear_counter_reload);
    read_value(data, remaining, m_triangle.timer_period);
    read_value(data, remaining, m_triangle.timer);
    read_value(data, remaining, m_triangle.sequence_pos);
    read_value(data, remaining, m_triangle.length_counter);
    read_value(data, remaining, m_triangle.linear_counter);
    read_value(data, remaining, flag);
    m_triangle.linear_counter_reload_flag = flag != 0;

    // Noise channel
    read_value(data, remaining, flag);
    m_noise.enabled = flag != 0;
    read_value(data, remaining, flag);
    m_noise.length_halt = flag != 0;
    read_value(data, remaining, flag);
    m_noise.constant_volume = flag != 0;
    read_value(data, remaining, m_noise.volume);
    read_value(data, remaining, flag);
    m_noise.mode = flag != 0;
    read_value(data, remaining, m_noise.timer_period);
    read_value(data, remaining, m_noise.timer);
    read_value(data, remaining, m_noise.shift_register);
    read_value(data, remaining, m_noise.length_counter);
    read_value(data, remaining, m_noise.envelope_counter);
    read_value(data, remaining, m_noise.envelope_divider);
    read_value(data, remaining, flag);
    m_noise.envelope_start = flag != 0;

    // DMC channel
    read_value(data, remaining, flag);
    m_dmc.enabled = flag != 0;
    read_value(data, remaining, flag);
    m_dmc.irq_enabled = flag != 0;
    read_value(data, remaining, flag);
    m_dmc.loop = flag != 0;
    read_value(data, remaining, m_dmc.rate);
    read_value(data, remaining, m_dmc.output_level);
    read_value(data, remaining, m_dmc.sample_address);
    read_value(data, remaining, m_dmc.sample_length);

    // Timing
    read_value(data, remaining, m_cycles);
    read_value(data, remaining, m_sample_counter);
    read_value(data, remaining, m_filter_state);

    // Clear audio buffer on load
    m_audio_write_pos = 0;
    m_sample_accumulator = 0.0f;
    m_sample_count = 0;
}

} // namespace nes
