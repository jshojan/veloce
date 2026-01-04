#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace snes {

class SPC700;

// Sony S-DSP (Digital Signal Processor)
// 8 voice channels, BRR decoding, ADSR, echo
// Reference: fullsnes, anomie's DSP doc
class DSP {
public:
    DSP();
    ~DSP();

    void reset();

    // Connect to SPC700 for RAM access
    void connect_spc(SPC700* spc) { m_spc = spc; }

    // Step DSP (called at 32kHz)
    void step();

    // Register access
    uint8_t read_address() const { return m_address; }
    void write_address(uint8_t value) { m_address = value & 0x7F; }
    uint8_t read_data();
    void write_data(uint8_t value);

    // Get audio output (stereo, -32768 to 32767)
    int16_t get_output_left() const { return m_output_left; }
    int16_t get_output_right() const { return m_output_right; }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    void process_voice(int v);
    void decode_brr_block(int v);
    int16_t interpolate(int v);
    void process_envelope(int v);
    void process_echo();

    SPC700* m_spc = nullptr;

    // Register address
    uint8_t m_address = 0;

    // DSP registers (128 bytes)
    std::array<uint8_t, 128> m_regs;

    // Voice state
    struct Voice {
        // BRR decoding
        uint16_t src_addr;       // Source directory address
        uint16_t brr_addr;       // Current BRR block address
        int brr_offset;          // Offset within BRR block (0-15)
        bool brr_end;            // End flag from BRR header
        bool brr_loop;           // Loop flag from BRR header

        // Decoded samples (ring buffer of 12 samples for interpolation)
        std::array<int16_t, 12> samples;
        int sample_index;        // Current position in ring buffer

        // Pitch
        uint16_t pitch;          // 14-bit pitch value
        uint32_t pitch_counter;  // Fractional sample position (16.16 fixed point)

        // Envelope
        int envelope_mode;       // 0=release, 1=attack, 2=decay, 3=sustain
        int envelope_level;      // Current envelope value (0-0x7FF)
        int envelope_rate;       // Current envelope rate (index into rate table)
        int envelope_counter;    // Counter for rate-based envelope timing

        // ADSR/GAIN parameters
        uint8_t adsr1;
        uint8_t adsr2;
        uint8_t gain;

        // Output
        int16_t output;          // Current voice output
        int16_t outx;            // Output for OUTX register

        // Key state
        bool key_on;
        bool key_on_delay;       // Key-on needs 5 sample delay
        int key_on_counter;

        // BRR buffer (16 samples per BRR block)
        std::array<int16_t, 16> brr_buffer;
    };
    std::array<Voice, 8> m_voices;

    // Global state
    int16_t m_output_left = 0;
    int16_t m_output_right = 0;

    // Echo buffer
    std::array<int16_t, 8> m_echo_history_left;
    std::array<int16_t, 8> m_echo_history_right;
    int m_echo_history_index = 0;
    uint16_t m_echo_addr = 0;
    int m_echo_offset = 0;
    int m_echo_length = 0;

    // FIR filter (8-tap)
    std::array<int8_t, 8> m_fir_coefficients;

    // Noise generator
    int16_t m_noise_value = -0x4000;
    int m_noise_rate = 0;
    int m_noise_counter = 0;

    // Noise rate table (cycles per noise step)
    static const int NOISE_RATE_TABLE[32];

    // Envelope rate table
    static const int ENVELOPE_RATE_TABLE[32];

    // Gaussian interpolation table
    static const int16_t GAUSS_TABLE[512];

    // Sample counter for timing
    int m_sample_counter = 0;

    // Register indices
    static constexpr int REG_VOL_L    = 0x00;  // VxVOLL
    static constexpr int REG_VOL_R    = 0x01;  // VxVOLR
    static constexpr int REG_PITCH_L  = 0x02;  // VxPITCHL
    static constexpr int REG_PITCH_H  = 0x03;  // VxPITCHH
    static constexpr int REG_SRCN     = 0x04;  // VxSRCN
    static constexpr int REG_ADSR1    = 0x05;  // VxADSR1
    static constexpr int REG_ADSR2    = 0x06;  // VxADSR2
    static constexpr int REG_GAIN     = 0x07;  // VxGAIN
    static constexpr int REG_ENVX     = 0x08;  // VxENVX
    static constexpr int REG_OUTX     = 0x09;  // VxOUTX

    static constexpr int REG_MVOL_L   = 0x0C;  // MVOLL
    static constexpr int REG_MVOL_R   = 0x1C;  // MVOLR
    static constexpr int REG_EVOL_L   = 0x2C;  // EVOLL
    static constexpr int REG_EVOL_R   = 0x3C;  // EVOLR
    static constexpr int REG_KON      = 0x4C;  // KON
    static constexpr int REG_KOFF     = 0x5C;  // KOFF
    static constexpr int REG_FLG      = 0x6C;  // FLG
    static constexpr int REG_ENDX     = 0x7C;  // ENDX

    static constexpr int REG_EFB      = 0x0D;  // EFB
    static constexpr int REG_PMON     = 0x2D;  // PMON
    static constexpr int REG_NON      = 0x3D;  // NON
    static constexpr int REG_EON      = 0x4D;  // EON
    static constexpr int REG_DIR      = 0x5D;  // DIR
    static constexpr int REG_ESA      = 0x6D;  // ESA
    static constexpr int REG_EDL      = 0x7D;  // EDL

    static constexpr int REG_FIR_0    = 0x0F;  // FIR coefficients
};

} // namespace snes
