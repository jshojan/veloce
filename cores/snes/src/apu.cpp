#include "apu.hpp"
#include "spc700.hpp"
#include "dsp.hpp"
#include "debug.hpp"
#include <cstring>

namespace snes {

APU::APU() {
    m_spc = std::make_unique<SPC700>();
    m_dsp = std::make_unique<DSP>();

    m_spc->connect_dsp(m_dsp.get());
    m_dsp->connect_spc(m_spc.get());

    reset();
}

APU::~APU() = default;

void APU::reset() {
    m_spc->reset();
    m_dsp->reset();

    m_spc_accumulator = 0;
    m_sample_counter = 0;  // SPC cycles until next DSP sample
    m_audio_buffer.fill(0);
    m_audio_write_pos = 0;
    m_last_left = 0;
    m_last_right = 0;
    m_stream_pos = 0;
}

void APU::step(int master_cycles) {
    // Fixed-point clock ratio (see apu.hpp): m_spc_accumulator tracks
    // elapsed SPC cycles in Q32.32 so the long-run average is exactly
    // MASTER_CLOCK_HZ / SPC_CLOCK_HZ (~20.948) master cycles per SPC cycle,
    // instead of a fixed integer divisor that drifts the APU's clock away
    // from the CPU/PPU's.
    //
    // The SPC700's step() executes one full instruction and returns
    // the number of SPC cycles it consumed.
    //
    // DSP generates one sample every 32 SPC cycles.

    m_spc_accumulator += static_cast<int64_t>(master_cycles) * SPC_CYCLE_RATIO_Q32;

    while (m_spc_accumulator >= (int64_t(1) << 32)) {
        // Step SPC700 - this executes one instruction and returns cycles consumed
        int spc_cycles = m_spc->step();

        // Deduct the equivalent fixed-point SPC cycles for the instruction
        // that was executed (may leave the accumulator transiently negative
        // when an instruction takes more than one SPC cycle - that's fine,
        // it is a signed 64-bit accumulator and recovers on the next call).
        m_spc_accumulator -= static_cast<int64_t>(spc_cycles) << 32;

        // Accumulate SPC cycles for DSP timing
        // DSP generates one sample every 32 SPC cycles (1,025,280 Hz / 32 = 32,040 Hz)
        m_sample_counter += spc_cycles;

        while (m_sample_counter >= 32) {
            m_sample_counter -= 32;

            m_dsp->step();

            // Get DSP output
            int16_t left = m_dsp->get_output_left();
            int16_t right = m_dsp->get_output_right();

            // Convert to float (-1.0 to 1.0)
            float left_f = left / 32768.0f;
            float right_f = right / 32768.0f;

            // If streaming callback is set, use low-latency path
            if (m_audio_callback) {
                m_stream_buffer[m_stream_pos * 2] = left_f;
                m_stream_buffer[m_stream_pos * 2 + 1] = right_f;
                m_stream_pos++;

                // Flush when buffer is full
                if (m_stream_pos >= STREAM_BUFFER_SIZE) {
                    m_audio_callback(m_stream_buffer, m_stream_pos, DSP_RATE);
                    m_stream_pos = 0;
                }
            } else {
                // Legacy path: buffer until get_samples() is called
                if (m_audio_write_pos < AUDIO_BUFFER_SIZE) {
                    m_audio_buffer[m_audio_write_pos * 2] = left_f;
                    m_audio_buffer[m_audio_write_pos * 2 + 1] = right_f;
                    m_audio_write_pos++;
                }
            }

            m_last_left = left;
            m_last_right = right;
        }
    }
}

uint8_t APU::read_port(int port) {
    return m_spc->cpu_read_port(port & 3);
}

void APU::write_port(int port, uint8_t value) {
    m_spc->cpu_write_port(port & 3, value);
}

size_t APU::get_samples(float* buffer, size_t max_samples) {
    size_t samples_to_copy = std::min(m_audio_write_pos, max_samples);

    if (samples_to_copy > 0) {
        std::memcpy(buffer, m_audio_buffer.data(), samples_to_copy * 2 * sizeof(float));
    }

    // Reset buffer position
    m_audio_write_pos = 0;

    return samples_to_copy;
}

void APU::flush_audio() {
    // Flush any remaining samples in the streaming buffer
    // This should be called at the end of each frame to prevent audio lag
    if (m_audio_callback && m_stream_pos > 0) {
        m_audio_callback(m_stream_buffer, m_stream_pos, DSP_RATE);
        m_stream_pos = 0;
    }
}

void APU::save_state(std::vector<uint8_t>& data) {
    m_spc->save_state(data);
    m_dsp->save_state(data);

    // Save timing state as explicit signed two's complement.
    // m_spc_accumulator is transiently negative right after step() deducts
    // an SPC instruction's cycles (the budget is only refilled on the next
    // call), so it must round-trip as a full signed 64-bit value.
    uint64_t accumulator_bits = static_cast<uint64_t>(m_spc_accumulator);
    for (int i = 0; i < 8; i++) {
        data.push_back((accumulator_bits >> (i * 8)) & 0xFF);
    }

    uint32_t sample_counter_bits = static_cast<uint32_t>(m_sample_counter);
    data.push_back(sample_counter_bits & 0xFF);
    data.push_back((sample_counter_bits >> 8) & 0xFF);
    data.push_back((sample_counter_bits >> 16) & 0xFF);
    data.push_back((sample_counter_bits >> 24) & 0xFF);
}

void APU::load_state(const uint8_t*& data, size_t& remaining) {
    m_spc->load_state(data, remaining);
    m_dsp->load_state(data, remaining);

    // Load timing state (see save_state for why this must be signed).
    uint64_t accumulator_bits = 0;
    for (int i = 0; i < 8; i++) {
        accumulator_bits |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    m_spc_accumulator = static_cast<int64_t>(accumulator_bits);
    data += 8; remaining -= 8;

    uint32_t sample_counter_bits = static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16) |
        (static_cast<uint32_t>(data[3]) << 24);
    m_sample_counter = static_cast<int32_t>(sample_counter_bits);
    data += 4; remaining -= 4;
}

} // namespace snes
