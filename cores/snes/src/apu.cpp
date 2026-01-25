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

    m_cycle_counter = 0;
    m_sample_counter = 0;  // SPC cycles until next DSP sample
    m_audio_buffer.fill(0);
    m_audio_write_pos = 0;
    m_last_left = 0;
    m_last_right = 0;
    m_stream_pos = 0;
}

void APU::step(int master_cycles) {
    // SPC700 runs at 1.024 MHz from its own 24.576 MHz crystal
    // Master clock is 21.477272 MHz
    // Ratio = 21477272 / 1024000 = 20.9739...
    // Using integer 21 is close enough (0.13% error)
    //
    // The SPC700's step() executes one full instruction and returns
    // the number of SPC cycles it consumed.
    //
    // DSP generates one sample every 32 SPC cycles.

    m_cycle_counter += master_cycles;

    while (m_cycle_counter >= MASTER_CYCLES_PER_SPC) {
        // Step SPC700 - this executes one instruction and returns cycles consumed
        int spc_cycles = m_spc->step();

        // Deduct the equivalent master cycles for the instruction that was executed
        m_cycle_counter -= spc_cycles * MASTER_CYCLES_PER_SPC;

        // Accumulate SPC cycles for DSP timing
        // DSP generates one sample every 32 SPC cycles (1.024 MHz / 32 = 32 kHz)
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

    // Save timing state
    data.push_back(m_cycle_counter & 0xFF);
    data.push_back((m_cycle_counter >> 8) & 0xFF);
    data.push_back(m_sample_counter & 0xFF);
    data.push_back((m_sample_counter >> 8) & 0xFF);
}

void APU::load_state(const uint8_t*& data, size_t& remaining) {
    m_spc->load_state(data, remaining);
    m_dsp->load_state(data, remaining);

    // Load timing state
    m_cycle_counter = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
    m_sample_counter = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;
}

} // namespace snes
