#include "apu.hpp"
#include "bus.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nes {

const uint8_t APU::s_length_table[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

// NTSC noise period table
const uint16_t APU::s_noise_period_ntsc[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

// PAL noise period table (different values for PAL timing)
const uint16_t APU::s_noise_period_pal[16] = {
    4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778
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

// DMC rate table (NTSC, in CPU cycles)
const uint16_t APU::s_dmc_rate_ntsc[16] = {
    428, 380, 340, 320, 286, 254, 226, 214,
    190, 160, 142, 128, 106,  84,  72,  54
};

// DMC rate table (PAL, in CPU cycles)
const uint16_t APU::s_dmc_rate_pal[16] = {
    398, 354, 316, 298, 276, 236, 210, 198,
    176, 148, 132, 118,  98,  78,  66,  50
};

APU::APU(Bus& bus) : m_bus(bus) {
    reset();
}

APU::~APU() = default;

void APU::set_cpu_cycle(uint64_t cycle) {
    m_global_cpu_cycle = cycle;
}

void APU::set_region(Region region) {
    m_region = region;
    switch (region) {
        case Region::NTSC:
            m_cpu_freq = 1789773;
            m_noise_period_table = s_noise_period_ntsc;
            m_dmc_rate_table_ptr = s_dmc_rate_ntsc;
            // NTSC frame counter timing (in CPU cycles after $4017 write)
            // Per blargg's apu_test:
            // - Length counters clocked at: 14916, 29832 (mode 0); 14916, 37284 (mode 1)
            // - IRQ flag set at: 29831 (1 cycle before length clock at step 4)
            // Note: envelope/linear counters are clocked at all 4 steps
            m_frame_step1 = 7458;   // Quarter frame 1: envelope/linear
            m_frame_step2 = 14916;  // Quarter frame 2: envelope/linear, length/sweep
            m_frame_step3 = 22374;  // Quarter frame 3: envelope/linear
            m_frame_step4 = 29832;  // Quarter frame 4: envelope/linear, length/sweep
            m_frame_irq_cycle = 29831;  // IRQ flag set 1 cycle before step 4
            m_frame_step5 = 37284;  // 5-step mode only: envelope/linear, length/sweep
            m_frame_reset4 = 29833; // Reset after step 4 + 1
            m_frame_reset5 = 37285;
            break;
        case Region::PAL:
            m_cpu_freq = 1662607;
            m_noise_period_table = s_noise_period_pal;
            m_dmc_rate_table_ptr = s_dmc_rate_pal;
            // PAL frame counter timing (scaled from NTSC)
            m_frame_step1 = 8315;
            m_frame_step2 = 16629;
            m_frame_step3 = 24943;
            m_frame_step4 = 33257;
            m_frame_irq_cycle = 33256;
            m_frame_step5 = 41571;
            m_frame_reset4 = 33258;
            m_frame_reset5 = 41572;
            break;
        case Region::Dendy:
            // Dendy uses PAL-like timing but with slightly different clock
            m_cpu_freq = 1773448;
            m_noise_period_table = s_noise_period_pal;  // Uses PAL tables
            m_dmc_rate_table_ptr = s_dmc_rate_pal;
            // Dendy uses PAL frame counter timing
            m_frame_step1 = 8315;
            m_frame_step2 = 16629;
            m_frame_step3 = 24943;
            m_frame_step4 = 33257;
            m_frame_irq_cycle = 33256;
            m_frame_step5 = 41571;
            m_frame_reset4 = 33258;
            m_frame_reset5 = 41572;
            break;
    }

    // Update DMC timer period with new rate table
    m_dmc.timer_period = m_dmc_rate_table_ptr[m_dmc.rate_index];
}

void APU::reset() {
    m_frame_counter_mode = 0;
    m_frame_counter_step = 0;
    m_frame_counter_cycles = 0;
    m_irq_inhibit = false;
    m_frame_irq = false;
    m_frame_counter_reset_delay = 0;
    m_frame_counter_reset_pending = false;
    m_pending_frame_counter_mode = 0;

    m_pulse[0] = Pulse{};
    m_pulse[1] = Pulse{};
    m_triangle = Triangle{};
    m_noise = Noise{};
    m_noise.shift_register = 1;

    // Initialize DMC properly
    m_dmc = DMC{};
    m_dmc.sample_address = 0xC000;
    m_dmc.sample_length = 1;
    m_dmc.current_address = 0xC000;
    m_dmc.bytes_remaining = 0;
    m_dmc.sample_buffer_empty = true;
    m_dmc.bits_remaining = 8;  // Start with full cycle to avoid underflow
    m_dmc.silence_flag = true;
    m_dmc.timer_period = m_dmc_rate_table_ptr[0];
    m_dmc.timer = m_dmc_rate_table_ptr[0] - 1;  // Timer counts from (period-1) to 0

    m_dmc_dma_cycles = 0;
    m_dmc_dma_pending = false;

    m_audio_write_pos = 0;
    m_cycles = 0;
    m_sample_counter = 0;

    // Reset all filter states
    m_hp_filter_state = 0.0f;
    m_lp_filter_state = 0.0f;
    m_aa_filter_state = 0.0f;
    m_aa_filter_state2 = 0.0f;
    m_sample_accumulator = 0.0f;
    m_sample_count = 0;
    m_prev_output_sample = 0.0f;

    // Reset DC blocker
    m_dc_blocker_prev_in = 0.0f;
    m_dc_blocker_prev_out = 0.0f;

    // Reset DMC smoothing
    m_dmc_smoothed_output = 0.0f;

    // Reset expansion audio
    m_expansion_audio = 0.0f;
    m_expansion_audio_smoothed = 0.0f;
}

void APU::step(int cpu_cycles) {
    for (int i = 0; i < cpu_cycles; i++) {
        m_cycles++;

        // Handle pending frame counter reset (from $4017 write)
        // The reset happens after a 3-4 cycle delay depending on CPU cycle parity
        if (m_frame_counter_reset_pending) {
            m_frame_counter_reset_delay--;
            if (m_frame_counter_reset_delay <= 0) {
                m_frame_counter_reset_pending = false;
                m_frame_counter_mode = m_pending_frame_counter_mode;
                m_frame_counter_step = 0;
                m_frame_counter_cycles = 0;

                // In 5-step mode, clock counters immediately when reset takes effect
                if (m_frame_counter_mode == 1) {
                    clock_envelopes();
                    clock_length_counters();
                    clock_sweeps();
                }
            }
        }

        // Clock triangle timer every CPU cycle
        if (m_triangle.timer == 0) {
            m_triangle.timer = m_triangle.timer_period;
            if (m_triangle.length_counter > 0 && m_triangle.linear_counter > 0) {
                m_triangle.sequence_pos = (m_triangle.sequence_pos + 1) & 31;
            }
        } else {
            m_triangle.timer--;
        }

        // Clock DMC timer every CPU cycle
        clock_dmc();

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

        // Frame counter - accurate step timing per blargg's apu_test
        // IRQ is set 1 cycle before the step 4 length clock
        m_frame_counter_cycles++;

        // Check for IRQ (mode 0 only)
        if (m_frame_counter_mode == 0 &&
            m_frame_counter_cycles == m_frame_irq_cycle &&
            !m_irq_inhibit) {
            m_frame_irq = true;
        }

        // Check for frame counter clock events
        bool should_clock = false;
        if (m_frame_counter_mode == 0) {
            // 4-step mode
            should_clock = (m_frame_counter_cycles == m_frame_step1 ||
                           m_frame_counter_cycles == m_frame_step2 ||
                           m_frame_counter_cycles == m_frame_step3 ||
                           m_frame_counter_cycles == m_frame_step4);
            if (m_frame_counter_cycles >= m_frame_reset4) {
                m_frame_counter_cycles = 0;
            }
        } else {
            // 5-step mode
            should_clock = (m_frame_counter_cycles == m_frame_step1 ||
                           m_frame_counter_cycles == m_frame_step2 ||
                           m_frame_counter_cycles == m_frame_step3 ||
                           m_frame_counter_cycles == m_frame_step5);
            // Note: step4 is skipped in 5-step mode (no clock, just wait)
            if (m_frame_counter_cycles >= m_frame_reset5) {
                m_frame_counter_cycles = 0;
            }
        }

        if (should_clock) {
            clock_frame_counter();
        }

        // Get raw mixed output
        float raw_sample = mix_output();

        // Apply anti-aliasing low-pass filter BEFORE downsampling
        // This is critical to prevent aliasing artifacts
        // Use a 2-pole filter for better roll-off (~12dB/octave)
        // Cutoff frequency ~15kHz at 1.79MHz sampling rate
        // Alpha = 2 * pi * fc / fs = 2 * pi * 15000 / 1789773 = 0.053
        static constexpr float AA_ALPHA = 0.053f;
        m_aa_filter_state = m_aa_filter_state + AA_ALPHA * (raw_sample - m_aa_filter_state);
        m_aa_filter_state2 = m_aa_filter_state2 + AA_ALPHA * (m_aa_filter_state - m_aa_filter_state2);
        float filtered_sample = m_aa_filter_state2;

        // Accumulate filtered samples for averaging (box filter for downsampling)
        m_sample_accumulator += filtered_sample;
        m_sample_count++;

        // Output sample at target rate (uses region-specific CPU frequency)
        m_sample_counter += SAMPLE_RATE;
        if (m_sample_counter >= m_cpu_freq) {
            m_sample_counter -= m_cpu_freq;

            // Average accumulated samples (box filter - simple but effective)
            float sample = (m_sample_count > 0) ? (m_sample_accumulator / m_sample_count) : 0.0f;
            m_sample_accumulator = 0.0f;
            m_sample_count = 0;

            // Apply high-pass filter to remove DC offset (~37Hz cutoff like real NES)
            // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
            // Alpha = 1 / (1 + 2*pi*fc/fs) = 1 / (1 + 2*pi*37/44100) = 0.9947
            static constexpr float HP_ALPHA = 0.9947f;
            float hp_output = HP_ALPHA * (m_hp_filter_state + sample - m_dc_blocker_prev_in);
            m_dc_blocker_prev_in = sample;
            m_hp_filter_state = hp_output;
            sample = hp_output;

            // Apply gentle low-pass filter for final output smoothing (~14kHz)
            // Alpha = 2 * pi * fc / fs = 2 * pi * 14000 / 44100 = 0.667
            static constexpr float LP_ALPHA = 0.5f;  // Slightly gentler for smoothness
            m_lp_filter_state = m_lp_filter_state + LP_ALPHA * (sample - m_lp_filter_state);
            sample = m_lp_filter_state;

            // Soft clipping to prevent harsh distortion if signal exceeds range
            // Use tanh-style soft clipping
            if (sample > 0.9f) {
                sample = 0.9f + 0.1f * std::tanh((sample - 0.9f) * 10.0f);
            } else if (sample < -0.9f) {
                sample = -0.9f + 0.1f * std::tanh((sample + 0.9f) * 10.0f);
            }

            // Linear interpolation with previous sample to reduce aliasing in final output
            // This provides additional smoothing for sample rate conversion
            float interp_sample = 0.5f * (sample + m_prev_output_sample);
            m_prev_output_sample = sample;
            sample = interp_sample;

            // If streaming callback is set, use low-latency path
            if (m_audio_callback) {
                m_stream_buffer[m_stream_pos * 2] = sample;
                m_stream_buffer[m_stream_pos * 2 + 1] = sample;  // Stereo
                m_stream_pos++;

                // Flush when buffer is full (every 64 samples = ~1.5ms)
                if (m_stream_pos >= STREAM_BUFFER_SIZE) {
                    m_audio_callback(m_stream_buffer, m_stream_pos, SAMPLE_RATE);
                    m_stream_pos = 0;
                }
            } else {
                // Legacy path: buffer until get_samples() is called
                if (m_audio_write_pos < AUDIO_BUFFER_SIZE * 2 - 1) {
                    m_audio_buffer[m_audio_write_pos++] = sample;
                    m_audio_buffer[m_audio_write_pos++] = sample;  // Stereo
                }
            }
        }
    }
}

void APU::clock_frame_counter() {
    m_frame_counter_step++;

    if (m_frame_counter_mode == 0) {
        // 4-step mode: envelope/linear counter on all steps, length/sweep on steps 2 and 4
        // Step 1: envelope, linear counter
        // Step 2: envelope, linear counter, length, sweep
        // Step 3: envelope, linear counter
        // Step 4: envelope, linear counter, length, sweep
        // Note: IRQ is handled separately in step() at m_frame_irq_cycle
        clock_envelopes();
        if (m_frame_counter_step == 2 || m_frame_counter_step == 4) {
            clock_length_counters();
            clock_sweeps();
        }
        if (m_frame_counter_step >= 4) {
            m_frame_counter_step = 0;
        }
    } else {
        // 5-step mode: same as 4-step but step 4 is skipped, step 5 clocks length/sweep
        // Step 1: envelope, linear counter
        // Step 2: envelope, linear counter, length, sweep
        // Step 3: envelope, linear counter
        // Step 4: (skipped in 5-step mode - but we call this for step 5)
        // Note: In 5-step mode there is no frame IRQ
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

void APU::clock_dmc() {
    // The DMC timer ticks every CPU cycle
    if (m_dmc.timer > 0) {
        m_dmc.timer--;
        return;
    }

    // Timer expired, reload and process output
    // Reload with period - 1 because the timer counts from (period-1) down to 0,
    // which is exactly 'period' states (cycles) per output clock
    m_dmc.timer = m_dmc.timer_period - 1;

    // Memory reader: fill sample buffer if empty and bytes remaining
    // This can happen at any time the buffer is empty, not just during output cycles
    if (m_dmc.sample_buffer_empty && m_dmc.bytes_remaining > 0) {
        dmc_fetch_sample();
    }

    // Output unit sequence (per nesdev wiki):
    // 1. If silence flag is clear, adjust output level based on bit 0
    // 2. Shift register right
    // 3. Decrement bits_remaining
    // 4. If bits_remaining becomes 0, start new output cycle

    // Step 1: Output a bit (if not in silence mode)
    if (!m_dmc.silence_flag) {
        // Bit 0 of shift register determines +2 or -2
        if (m_dmc.shift_register & 1) {
            // Increment output level (clamped to 127)
            if (m_dmc.output_level <= 125) {
                m_dmc.output_level += 2;
            }
        } else {
            // Decrement output level (clamped to 0)
            if (m_dmc.output_level >= 2) {
                m_dmc.output_level -= 2;
            }
        }
    }

    // Step 2: Shift the register right
    m_dmc.shift_register >>= 1;

    // Step 3: Decrement bits remaining
    m_dmc.bits_remaining--;

    // Step 4: If bits_remaining becomes 0, start new output cycle
    if (m_dmc.bits_remaining == 0) {
        m_dmc.bits_remaining = 8;

        if (m_dmc.sample_buffer_empty) {
            // No sample available - enter silence mode
            m_dmc.silence_flag = true;
        } else {
            // Load shift register from sample buffer
            m_dmc.silence_flag = false;
            m_dmc.shift_register = m_dmc.sample_buffer;
            m_dmc.sample_buffer_empty = true;
        }
    }
}

void APU::dmc_fetch_sample() {
    // Perform DMA read from memory
    m_dmc.sample_buffer = m_bus.cpu_read(m_dmc.current_address);
    m_dmc.sample_buffer_empty = false;

    // Advance address (wrapping from $FFFF to $8000)
    m_dmc.current_address++;
    if (m_dmc.current_address == 0) {
        m_dmc.current_address = 0x8000;
    }

    // Decrement bytes remaining
    m_dmc.bytes_remaining--;

    // If we've finished the sample
    if (m_dmc.bytes_remaining == 0) {
        if (m_dmc.loop) {
            // Restart sample
            m_dmc.current_address = m_dmc.sample_address;
            m_dmc.bytes_remaining = m_dmc.sample_length;
        } else {
            // Trigger IRQ if enabled
            if (m_dmc.irq_enabled) {
                m_dmc.irq_pending = true;
            }
        }
    }

    // DMA read steals 1-4 CPU cycles:
    // 1 cycle if no pending writes
    // 2 cycles if CPU is writing during get cycle
    // 3 cycles if CPU halted for OAM DMA and ready to read
    // 4 cycles if CPU halted for OAM DMA during put cycle
    // We approximate with 4 cycles (worst case)
    m_dmc_dma_cycles += 4;
    m_dmc_dma_pending = true;
}

int APU::get_dmc_dma_cycles() {
    int cycles = m_dmc_dma_cycles;
    m_dmc_dma_cycles = 0;
    m_dmc_dma_pending = false;
    return cycles;
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

    // DMC - apply smoothing to reduce clicks from direct loads ($4011 writes)
    // The smoothing factor controls how quickly we approach the target level
    // A value of 0.95 means we reach 95% of the change in about 20 samples
    // at CPU rate, which is very fast but eliminates instant jumps
    float dmc_target = static_cast<float>(m_dmc.output_level);
    m_dmc_smoothed_output = m_dmc_smoothed_output + (1.0f - DMC_SMOOTH_FACTOR) * (dmc_target - m_dmc_smoothed_output);
    float dmc = m_dmc_smoothed_output;

    tnd_out = 0.00851f * triangle + 0.00494f * noise + 0.00335f * dmc;

    // Mix in expansion audio (from mapper chips like VRC6, Sunsoft 5B, N163, MMC5)
    // Apply smoothing to expansion audio to prevent clicks when it changes suddenly
    m_expansion_audio_smoothed = m_expansion_audio_smoothed +
        0.1f * (m_expansion_audio - m_expansion_audio_smoothed);
    float expansion = m_expansion_audio_smoothed * 0.35f;  // Slightly lower to prevent clipping

    // Calculate total output with headroom for peaks
    // Scale down slightly to prevent clipping when all channels are at max
    float total = (pulse_out + tnd_out) * 0.9f + expansion;

    return total;
}

uint8_t APU::cpu_read(uint16_t address) {
    if (address == 0x4015) {
        uint8_t status = 0;
        if (m_pulse[0].length_counter > 0) status |= 0x01;
        if (m_pulse[1].length_counter > 0) status |= 0x02;
        if (m_triangle.length_counter > 0) status |= 0x04;
        if (m_noise.length_counter > 0) status |= 0x08;
        if (m_dmc.bytes_remaining > 0) status |= 0x10;  // DMC active
        if (m_frame_irq) status |= 0x40;
        if (m_dmc.irq_pending) status |= 0x80;

        // Reading $4015 clears frame IRQ flag
        m_frame_irq = false;
        // Note: DMC IRQ is NOT cleared by reading $4015
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
            // Length counter is only reloaded if channel is enabled
            if (m_pulse[0].enabled) {
                m_pulse[0].length_counter = s_length_table[value >> 3];
            }
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
            // Length counter is only reloaded if channel is enabled
            if (m_pulse[1].enabled) {
                m_pulse[1].length_counter = s_length_table[value >> 3];
            }
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
            // Length counter is only reloaded if channel is enabled
            if (m_triangle.enabled) {
                m_triangle.length_counter = s_length_table[value >> 3];
            }
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
            m_noise.timer_period = m_noise_period_table[value & 0x0F];
            break;
        case 0x400F:
            // Length counter is only reloaded if channel is enabled
            if (m_noise.enabled) {
                m_noise.length_counter = s_length_table[value >> 3];
            }
            m_noise.envelope_start = true;
            break;

        // DMC
        case 0x4010:
            m_dmc.irq_enabled = (value & 0x80) != 0;
            m_dmc.loop = (value & 0x40) != 0;
            m_dmc.rate_index = value & 0x0F;
            m_dmc.timer_period = m_dmc_rate_table_ptr[m_dmc.rate_index];
            // If IRQ disabled, clear pending IRQ
            if (!m_dmc.irq_enabled) {
                m_dmc.irq_pending = false;
            }
            break;
        case 0x4011:
            // Direct load to output level
            m_dmc.output_level = value & 0x7F;
            break;
        case 0x4012:
            // Sample address = $C000 + (A * 64)
            m_dmc.sample_address = 0xC000 | (static_cast<uint16_t>(value) << 6);
            break;
        case 0x4013:
            // Sample length = (L * 16) + 1
            m_dmc.sample_length = (static_cast<uint16_t>(value) << 4) + 1;
            break;

        // Status
        case 0x4015:
            m_pulse[0].enabled = (value & 0x01) != 0;
            m_pulse[1].enabled = (value & 0x02) != 0;
            m_triangle.enabled = (value & 0x04) != 0;
            m_noise.enabled = (value & 0x08) != 0;

            if (!m_pulse[0].enabled) m_pulse[0].length_counter = 0;
            if (!m_pulse[1].enabled) m_pulse[1].length_counter = 0;
            if (!m_triangle.enabled) m_triangle.length_counter = 0;
            if (!m_noise.enabled) m_noise.length_counter = 0;

            // DMC enable/disable
            // Clear DMC IRQ flag when writing to $4015
            m_dmc.irq_pending = false;

            if (value & 0x10) {
                // Enable DMC
                if (m_dmc.bytes_remaining == 0) {
                    // If sample is inactive, restart it
                    m_dmc.current_address = m_dmc.sample_address;
                    m_dmc.bytes_remaining = m_dmc.sample_length;
                }
                // Per nesdev wiki: "Any time the sample buffer is in an empty state
                // and bytes remaining is not zero (including just after a write to
                // $4015 that enables the channel...), the memory reader fills it."
                if (m_dmc.sample_buffer_empty && m_dmc.bytes_remaining > 0) {
                    dmc_fetch_sample();
                }
            } else {
                // Disable DMC - set bytes remaining to 0
                m_dmc.bytes_remaining = 0;
            }
            break;

        // Frame counter
        case 0x4017:
            // Per nesdev wiki and blargg's apu_test:
            // Writing to $4017 resets the frame counter with a delay:
            // - On odd CPU cycle: reset happens 3 cycles later
            // - On even CPU cycle: reset happens 4 cycles later
            // The APU runs at half CPU speed (every 2 CPU cycles = 1 APU cycle),
            // so we track based on whether total CPU cycles is even/odd.
            //
            // For accurate timing, we defer the reset using a countdown.
            // CRITICAL: Use the global CPU cycle counter for jitter test accuracy.
            // The bus sets this before calling cpu_write to ensure we see the
            // exact cycle when the write occurred.
            m_irq_inhibit = (value & 0x40) != 0;
            if (m_irq_inhibit) m_frame_irq = false;

            // Store the pending mode - actual mode change happens after delay
            m_pending_frame_counter_mode = (value & 0x80) ? 1 : 0;

            // Determine delay based on CPU cycle parity
            // Use global CPU cycle for accurate timing - m_global_cpu_cycle is set
            // by the bus right before this write
            // On odd CPU cycle: delay is 3 cycles
            // On even CPU cycle: delay is 4 cycles
            m_frame_counter_reset_delay = (m_global_cpu_cycle & 1) ? 3 : 4;
            m_frame_counter_reset_pending = true;

            // Note: In 5-step mode, the immediate clock happens AFTER the delay
            // expires (when reset actually occurs), not immediately on write.
            // This is handled in the step() function.
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
    // Frame counter reset delay fields
    write_value(data, m_frame_counter_reset_delay);
    write_value(data, static_cast<uint8_t>(m_frame_counter_reset_pending ? 1 : 0));
    write_value(data, m_pending_frame_counter_mode);

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

    // DMC channel (expanded for full DMA support)
    write_value(data, static_cast<uint8_t>(m_dmc.enabled ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_dmc.irq_enabled ? 1 : 0));
    write_value(data, static_cast<uint8_t>(m_dmc.loop ? 1 : 0));
    write_value(data, m_dmc.rate_index);
    write_value(data, m_dmc.output_level);
    write_value(data, m_dmc.sample_address);
    write_value(data, m_dmc.sample_length);
    write_value(data, m_dmc.current_address);
    write_value(data, m_dmc.bytes_remaining);
    write_value(data, m_dmc.sample_buffer);
    write_value(data, static_cast<uint8_t>(m_dmc.sample_buffer_empty ? 1 : 0));
    write_value(data, m_dmc.shift_register);
    write_value(data, m_dmc.bits_remaining);
    write_value(data, static_cast<uint8_t>(m_dmc.silence_flag ? 1 : 0));
    write_value(data, m_dmc.timer);
    write_value(data, m_dmc.timer_period);
    write_value(data, static_cast<uint8_t>(m_dmc.irq_pending ? 1 : 0));

    // Timing
    write_value(data, m_cycles);
    write_value(data, m_sample_counter);

    // Filter states
    write_value(data, m_hp_filter_state);
    write_value(data, m_lp_filter_state);
    write_value(data, m_aa_filter_state);
    write_value(data, m_aa_filter_state2);
    write_value(data, m_prev_output_sample);
    write_value(data, m_dc_blocker_prev_in);
    write_value(data, m_dc_blocker_prev_out);
    write_value(data, m_dmc_smoothed_output);
    write_value(data, m_expansion_audio_smoothed);
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
    // Frame counter reset delay fields
    read_value(data, remaining, m_frame_counter_reset_delay);
    read_value(data, remaining, flag);
    m_frame_counter_reset_pending = flag != 0;
    read_value(data, remaining, m_pending_frame_counter_mode);

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

    // DMC channel (expanded for full DMA support)
    read_value(data, remaining, flag);
    m_dmc.enabled = flag != 0;
    read_value(data, remaining, flag);
    m_dmc.irq_enabled = flag != 0;
    read_value(data, remaining, flag);
    m_dmc.loop = flag != 0;
    read_value(data, remaining, m_dmc.rate_index);
    read_value(data, remaining, m_dmc.output_level);
    read_value(data, remaining, m_dmc.sample_address);
    read_value(data, remaining, m_dmc.sample_length);
    read_value(data, remaining, m_dmc.current_address);
    read_value(data, remaining, m_dmc.bytes_remaining);
    read_value(data, remaining, m_dmc.sample_buffer);
    read_value(data, remaining, flag);
    m_dmc.sample_buffer_empty = flag != 0;
    read_value(data, remaining, m_dmc.shift_register);
    read_value(data, remaining, m_dmc.bits_remaining);
    read_value(data, remaining, flag);
    m_dmc.silence_flag = flag != 0;
    read_value(data, remaining, m_dmc.timer);
    read_value(data, remaining, m_dmc.timer_period);
    read_value(data, remaining, flag);
    m_dmc.irq_pending = flag != 0;

    // Timing
    read_value(data, remaining, m_cycles);
    read_value(data, remaining, m_sample_counter);

    // Filter states - try to read them, but they may not exist in old save states
    if (remaining >= sizeof(float) * 9) {
        read_value(data, remaining, m_hp_filter_state);
        read_value(data, remaining, m_lp_filter_state);
        read_value(data, remaining, m_aa_filter_state);
        read_value(data, remaining, m_aa_filter_state2);
        read_value(data, remaining, m_prev_output_sample);
        read_value(data, remaining, m_dc_blocker_prev_in);
        read_value(data, remaining, m_dc_blocker_prev_out);
        read_value(data, remaining, m_dmc_smoothed_output);
        read_value(data, remaining, m_expansion_audio_smoothed);
    } else {
        // Old save state format - initialize filter states to reasonable values
        m_hp_filter_state = 0.0f;
        m_lp_filter_state = 0.0f;
        m_aa_filter_state = 0.0f;
        m_aa_filter_state2 = 0.0f;
        m_prev_output_sample = 0.0f;
        m_dc_blocker_prev_in = 0.0f;
        m_dc_blocker_prev_out = 0.0f;
        m_dmc_smoothed_output = static_cast<float>(m_dmc.output_level);
        m_expansion_audio_smoothed = m_expansion_audio;
    }

    // Fade out current audio buffer smoothly to prevent pop on state load
    // Apply a quick fade-out to the existing buffer content
    for (size_t i = 0; i < m_audio_write_pos; i++) {
        float fade = 1.0f - static_cast<float>(i) / static_cast<float>(m_audio_write_pos + 1);
        m_audio_buffer[i] *= fade * fade;  // Quadratic fade for smoother transition
    }

    // Reset write position but keep some fade-out samples for smooth transition
    size_t keep_samples = std::min(m_audio_write_pos, size_t(64));  // Keep ~1.5ms of fade
    if (keep_samples > 0 && m_audio_write_pos > keep_samples) {
        // Move fade-out samples to beginning
        for (size_t i = 0; i < keep_samples; i++) {
            m_audio_buffer[i] = m_audio_buffer[m_audio_write_pos - keep_samples + i];
        }
        m_audio_write_pos = keep_samples;
    } else {
        m_audio_write_pos = 0;
    }

    m_sample_accumulator = 0.0f;
    m_sample_count = 0;
    m_expansion_audio = 0.0f;
    m_dmc_dma_cycles = 0;
    m_dmc_dma_pending = false;
}

} // namespace nes
