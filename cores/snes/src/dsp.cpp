#include "dsp.hpp"
#include "spc700.hpp"
#include "debug.hpp"
#include <cstring>
#include <algorithm>

namespace snes {

// Noise rate table (cycles between noise steps)
const int DSP::NOISE_RATE_TABLE[32] = {
    0, 2048, 1536, 1280, 1024, 768, 640, 512,
    384, 320, 256, 192, 160, 128, 96, 80,
    64, 48, 40, 32, 24, 20, 16, 12,
    10, 8, 6, 5, 4, 3, 2, 1
};

// Envelope rate table (cycles per envelope step)
const int DSP::ENVELOPE_RATE_TABLE[32] = {
    0x7FFFFFFF, 2048, 1536, 1280, 1024, 768, 640, 512,
    384, 320, 256, 192, 160, 128, 96, 80,
    64, 48, 40, 32, 24, 20, 16, 12,
    10, 8, 6, 5, 4, 3, 2, 1
};

// Gaussian interpolation table
const int16_t DSP::GAUSS_TABLE[512] = {
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
       1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,
       2,    2,    3,    3,    3,    3,    3,    4,    4,    4,    4,    4,    5,    5,    5,    5,
       6,    6,    6,    6,    7,    7,    7,    8,    8,    8,    9,    9,    9,   10,   10,   10,
      11,   11,   11,   12,   12,   13,   13,   14,   14,   15,   15,   15,   16,   16,   17,   17,
      18,   19,   19,   20,   20,   21,   21,   22,   23,   23,   24,   24,   25,   26,   27,   27,
      28,   29,   29,   30,   31,   32,   32,   33,   34,   35,   36,   36,   37,   38,   39,   40,
      41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54,   55,   56,
      58,   59,   60,   61,   62,   64,   65,   66,   67,   69,   70,   71,   73,   74,   76,   77,
      78,   80,   81,   83,   84,   86,   87,   89,   90,   92,   94,   95,   97,   99,  100,  102,
     104,  106,  107,  109,  111,  113,  115,  117,  118,  120,  122,  124,  126,  128,  130,  132,
     134,  137,  139,  141,  143,  145,  147,  150,  152,  154,  156,  159,  161,  163,  166,  168,
     171,  173,  175,  178,  180,  183,  186,  188,  191,  193,  196,  199,  201,  204,  207,  210,
     212,  215,  218,  221,  224,  227,  230,  233,  236,  239,  242,  245,  248,  251,  254,  257,
     260,  263,  267,  270,  273,  276,  280,  283,  286,  290,  293,  297,  300,  304,  307,  311,
     314,  318,  321,  325,  328,  332,  336,  339,  343,  347,  351,  354,  358,  362,  366,  370,
     374,  378,  381,  385,  389,  393,  397,  401,  405,  410,  414,  418,  422,  426,  430,  434,
     439,  443,  447,  451,  456,  460,  464,  469,  473,  477,  482,  486,  491,  495,  499,  504,
     508,  513,  517,  522,  527,  531,  536,  540,  545,  550,  554,  559,  563,  568,  573,  577,
     582,  587,  592,  596,  601,  606,  611,  615,  620,  625,  630,  635,  640,  644,  649,  654,
     659,  664,  669,  674,  678,  683,  688,  693,  698,  703,  708,  713,  718,  723,  728,  732,
     737,  742,  747,  752,  757,  762,  767,  772,  777,  782,  787,  792,  797,  802,  806,  811,
     816,  821,  826,  831,  836,  841,  846,  851,  855,  860,  865,  870,  875,  880,  884,  889,
     894,  899,  904,  908,  913,  918,  923,  927,  932,  937,  941,  946,  951,  955,  960,  965,
     969,  974,  978,  983,  988,  992,  997, 1001, 1005, 1010, 1014, 1019, 1023, 1027, 1032, 1036,
    1040, 1045, 1049, 1053, 1057, 1061, 1066, 1070, 1074, 1078, 1082, 1086, 1090, 1094, 1098, 1102,
    1106, 1109, 1113, 1117, 1121, 1125, 1128, 1132, 1136, 1139, 1143, 1146, 1150, 1153, 1157, 1160,
    1164, 1167, 1170, 1174, 1177, 1180, 1183, 1186, 1190, 1193, 1196, 1199, 1202, 1205, 1207, 1210,
    1213, 1216, 1219, 1221, 1224, 1227, 1229, 1232, 1234, 1237, 1239, 1241, 1244, 1246, 1248, 1251,
    1253, 1255, 1257, 1259, 1261, 1263, 1265, 1267, 1269, 1270, 1272, 1274, 1275, 1277, 1279, 1280,
    1282, 1283, 1284, 1286, 1287, 1288, 1290, 1291, 1292, 1293, 1294, 1295, 1296, 1297, 1297, 1298,
    1299, 1300, 1300, 1301, 1302, 1302, 1303, 1303, 1303, 1304, 1304, 1304, 1304, 1304, 1305, 1305
};

DSP::DSP() {
    reset();
}

DSP::~DSP() = default;

void DSP::reset() {
    m_address = 0;
    m_regs.fill(0);

    for (auto& voice : m_voices) {
        voice.src_addr = 0;
        voice.brr_addr = 0;
        voice.brr_offset = 0;
        voice.brr_end = false;
        voice.brr_loop = false;
        voice.samples.fill(0);
        voice.sample_index = 0;
        voice.pitch = 0;
        voice.pitch_counter = 0;
        voice.envelope_mode = 0;
        voice.envelope_level = 0;
        voice.envelope_rate = 0;
        voice.adsr1 = 0;
        voice.adsr2 = 0;
        voice.gain = 0;
        voice.output = 0;
        voice.outx = 0;
        voice.key_on = false;
        voice.key_on_delay = false;
        voice.key_on_counter = 0;
        voice.brr_buffer.fill(0);
    }

    m_output_left = 0;
    m_output_right = 0;

    m_echo_history_left.fill(0);
    m_echo_history_right.fill(0);
    m_echo_history_index = 0;
    m_echo_addr = 0;
    m_echo_offset = 0;
    m_echo_length = 0;

    m_fir_coefficients.fill(0);

    m_noise_value = -0x4000;
    m_noise_rate = 0;
    m_noise_counter = 0;

    m_sample_counter = 0;

    // Set default flags
    m_regs[REG_FLG] = 0xE0;  // Soft reset, mute, echo disable
}

uint8_t DSP::read_data() {
    if (m_address >= 128) return 0;

    switch (m_address & 0x0F) {
        case REG_ENVX & 0x0F: {
            int v = m_address >> 4;
            return (m_voices[v].envelope_level >> 4) & 0x7F;
        }
        case REG_OUTX & 0x0F: {
            int v = m_address >> 4;
            return (m_voices[v].outx >> 8) & 0xFF;
        }
        default:
            return m_regs[m_address];
    }
}

void DSP::write_data(uint8_t value) {
    if (m_address >= 128) return;

    m_regs[m_address] = value;

    int v = m_address >> 4;  // Voice number (0-7)

    switch (m_address) {
        case REG_KON:
            // Key-on voices
            for (int i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    m_voices[i].key_on = true;
                    m_voices[i].key_on_delay = true;
                    m_voices[i].key_on_counter = 5;
                }
            }
            break;

        case REG_KOFF:
            // Key-off voices
            for (int i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    m_voices[i].envelope_mode = 0;  // Release
                }
            }
            break;

        case REG_FLG:
            m_noise_rate = value & 0x1F;
            break;

        case REG_ENDX:
            // Clear ENDX (write any value)
            m_regs[REG_ENDX] = 0;
            break;

        // FIR coefficients
        case 0x0F: m_fir_coefficients[0] = static_cast<int8_t>(value); break;
        case 0x1F: m_fir_coefficients[1] = static_cast<int8_t>(value); break;
        case 0x2F: m_fir_coefficients[2] = static_cast<int8_t>(value); break;
        case 0x3F: m_fir_coefficients[3] = static_cast<int8_t>(value); break;
        case 0x4F: m_fir_coefficients[4] = static_cast<int8_t>(value); break;
        case 0x5F: m_fir_coefficients[5] = static_cast<int8_t>(value); break;
        case 0x6F: m_fir_coefficients[6] = static_cast<int8_t>(value); break;
        case 0x7F: m_fir_coefficients[7] = static_cast<int8_t>(value); break;

        default:
            // Voice registers
            if (v < 8) {
                switch (m_address & 0x0F) {
                    case REG_SRCN & 0x0F:
                        // Source number - load from directory
                        {
                            uint16_t dir = m_regs[REG_DIR] << 8;
                            uint16_t entry = dir + (value * 4);
                            if (m_spc) {
                                const uint8_t* ram = m_spc->get_ram();
                                m_voices[v].src_addr = ram[entry] | (ram[entry + 1] << 8);
                            }
                        }
                        break;
                    case REG_ADSR1 & 0x0F:
                        m_voices[v].adsr1 = value;
                        break;
                    case REG_ADSR2 & 0x0F:
                        m_voices[v].adsr2 = value;
                        break;
                    case REG_GAIN & 0x0F:
                        m_voices[v].gain = value;
                        break;
                }
            }
            break;
    }
}

void DSP::step() {
    if (!m_spc) return;

    const uint8_t* ram = m_spc->get_ram();

    // Check for soft reset
    if (m_regs[REG_FLG] & 0x80) {
        m_output_left = 0;
        m_output_right = 0;
        return;
    }

    // Update noise generator
    if (m_noise_rate > 0) {
        m_noise_counter++;
        if (m_noise_counter >= NOISE_RATE_TABLE[m_noise_rate]) {
            m_noise_counter = 0;
            // LFSR noise
            int bit = (m_noise_value ^ (m_noise_value >> 1)) & 1;
            m_noise_value = (m_noise_value >> 1) | (bit << 14);
            m_noise_value = static_cast<int16_t>(m_noise_value << 1) >> 1;  // Sign extend
        }
    }

    // Process all voices
    int32_t left_sum = 0;
    int32_t right_sum = 0;
    int32_t echo_left_sum = 0;
    int32_t echo_right_sum = 0;

    uint8_t pmon = m_regs[REG_PMON];
    uint8_t non = m_regs[REG_NON];
    uint8_t eon = m_regs[REG_EON];

    int16_t prev_outx = 0;

    for (int v = 0; v < 8; v++) {
        auto& voice = m_voices[v];

        // Handle key-on delay
        if (voice.key_on_delay) {
            voice.key_on_counter--;
            if (voice.key_on_counter <= 0) {
                voice.key_on_delay = false;
                voice.key_on = false;

                // Initialize voice
                uint16_t dir = m_regs[REG_DIR] << 8;
                uint8_t srcn = m_regs[(v << 4) + REG_SRCN];
                uint16_t entry = dir + (srcn * 4);

                voice.brr_addr = ram[entry] | (ram[entry + 1] << 8);
                voice.brr_offset = 0;
                voice.samples.fill(0);
                voice.sample_index = 0;
                voice.pitch_counter = 0;
                voice.envelope_level = 0;
                voice.envelope_mode = 1;  // Attack

                // Clear ENDX bit
                m_regs[REG_ENDX] &= ~(1 << v);
            }
            continue;
        }

        // Get pitch
        uint16_t pitch = m_regs[(v << 4) + REG_PITCH_L] |
                        ((m_regs[(v << 4) + REG_PITCH_H] & 0x3F) << 8);

        // Apply pitch modulation
        if ((pmon & (1 << v)) && v > 0) {
            int32_t mod = (prev_outx * pitch) >> 15;
            pitch = static_cast<uint16_t>(std::clamp(pitch + mod, 0, 0x3FFF));
        }

        voice.pitch = pitch;

        // Advance pitch counter
        voice.pitch_counter += pitch;

        // Decode new BRR samples if needed
        while (voice.pitch_counter >= 0x1000) {
            voice.pitch_counter -= 0x1000;

            // Advance sample position
            voice.brr_offset++;
            if (voice.brr_offset >= 16) {
                voice.brr_offset = 0;

                // Check for end of BRR block
                if (voice.brr_end) {
                    m_regs[REG_ENDX] |= (1 << v);

                    if (voice.brr_loop) {
                        // Get loop address from directory
                        uint16_t dir = m_regs[REG_DIR] << 8;
                        uint8_t srcn = m_regs[(v << 4) + REG_SRCN];
                        uint16_t entry = dir + (srcn * 4);
                        voice.brr_addr = ram[entry + 2] | (ram[entry + 3] << 8);
                    } else {
                        // End voice
                        voice.envelope_mode = 0;
                        voice.envelope_level = 0;
                    }
                } else {
                    voice.brr_addr += 9;
                }

                // Decode next BRR block
                decode_brr_block(v);
            }

            // Store sample in ring buffer
            voice.samples[voice.sample_index] = voice.brr_buffer[voice.brr_offset >> 2];
            voice.sample_index = (voice.sample_index + 1) % 12;
        }

        // Interpolate sample
        int16_t sample;
        if (non & (1 << v)) {
            // Use noise instead of sample
            sample = m_noise_value;
        } else {
            sample = interpolate(v);
        }

        // Process envelope
        process_envelope(v);

        // Apply envelope
        int32_t output = (sample * voice.envelope_level) >> 11;
        output = std::clamp(output, -32768, 32767);

        voice.output = static_cast<int16_t>(output);
        voice.outx = voice.output;
        prev_outx = voice.outx;

        // Get volume
        int8_t vol_l = static_cast<int8_t>(m_regs[(v << 4) + REG_VOL_L]);
        int8_t vol_r = static_cast<int8_t>(m_regs[(v << 4) + REG_VOL_R]);

        // Mix into output
        left_sum += (voice.output * vol_l) >> 7;
        right_sum += (voice.output * vol_r) >> 7;

        // Mix into echo buffer
        if (eon & (1 << v)) {
            echo_left_sum += (voice.output * vol_l) >> 7;
            echo_right_sum += (voice.output * vol_r) >> 7;
        }
    }

    // Process echo
    if (!(m_regs[REG_FLG] & 0x20)) {  // Echo not disabled
        process_echo();

        // Get echo output and apply FIR filter
        int32_t fir_left = 0;
        int32_t fir_right = 0;
        for (int i = 0; i < 8; i++) {
            int idx = (m_echo_history_index + i) % 8;
            fir_left += m_echo_history_left[idx] * m_fir_coefficients[i];
            fir_right += m_echo_history_right[idx] * m_fir_coefficients[i];
        }
        fir_left >>= 6;
        fir_right >>= 6;

        // Apply echo volume
        int8_t evol_l = static_cast<int8_t>(m_regs[REG_EVOL_L]);
        int8_t evol_r = static_cast<int8_t>(m_regs[REG_EVOL_R]);
        left_sum += (fir_left * evol_l) >> 7;
        right_sum += (fir_right * evol_r) >> 7;

        // Write echo feedback
        if (!(m_regs[REG_FLG] & 0x20)) {
            int8_t efb = static_cast<int8_t>(m_regs[REG_EFB]);
            int32_t echo_out_l = echo_left_sum + ((fir_left * efb) >> 7);
            int32_t echo_out_r = echo_right_sum + ((fir_right * efb) >> 7);

            echo_out_l = std::clamp(echo_out_l, -32768, 32767);
            echo_out_r = std::clamp(echo_out_r, -32768, 32767);

            // Write to echo buffer
            uint16_t esa = m_regs[REG_ESA] << 8;
            uint16_t echo_addr = esa + m_echo_offset;

            if (!(m_regs[REG_FLG] & 0x20)) {
                // Echo writes are disabled when bit 5 is set
                uint8_t* spc_ram = m_spc->get_ram();
                spc_ram[echo_addr] = echo_out_l & 0xFF;
                spc_ram[echo_addr + 1] = (echo_out_l >> 8) & 0xFF;
                spc_ram[echo_addr + 2] = echo_out_r & 0xFF;
                spc_ram[echo_addr + 3] = (echo_out_r >> 8) & 0xFF;
            }
        }
    }

    // Apply master volume
    int8_t mvol_l = static_cast<int8_t>(m_regs[REG_MVOL_L]);
    int8_t mvol_r = static_cast<int8_t>(m_regs[REG_MVOL_R]);

    left_sum = (left_sum * mvol_l) >> 7;
    right_sum = (right_sum * mvol_r) >> 7;

    // Clamp and check mute
    if (m_regs[REG_FLG] & 0x40) {
        m_output_left = 0;
        m_output_right = 0;
    } else {
        m_output_left = static_cast<int16_t>(std::clamp(left_sum, -32768, 32767));
        m_output_right = static_cast<int16_t>(std::clamp(right_sum, -32768, 32767));
    }

    m_sample_counter++;
}

void DSP::decode_brr_block(int v) {
    if (!m_spc) return;

    const uint8_t* ram = m_spc->get_ram();
    auto& voice = m_voices[v];

    // Read BRR header
    uint8_t header = ram[voice.brr_addr];
    int shift = header >> 4;
    int filter = (header >> 2) & 0x03;
    voice.brr_loop = (header & 0x02) != 0;
    voice.brr_end = (header & 0x01) != 0;

    // Get previous samples for filter
    int prev1 = voice.samples[(voice.sample_index + 11) % 12];
    int prev2 = voice.samples[(voice.sample_index + 10) % 12];

    // Decode 16 samples (4 at a time)
    for (int i = 0; i < 4; i++) {
        uint8_t byte = ram[(voice.brr_addr + 1 + i * 2) & 0xFFFF];
        uint8_t byte2 = ram[(voice.brr_addr + 2 + i * 2) & 0xFFFF];

        for (int j = 0; j < 4; j++) {
            int nibble;
            if (j < 2) {
                nibble = (j == 0) ? (byte >> 4) : (byte & 0x0F);
            } else {
                nibble = (j == 2) ? (byte2 >> 4) : (byte2 & 0x0F);
            }

            // Sign extend nibble
            if (nibble >= 8) nibble -= 16;

            // Apply shift
            int sample;
            if (shift <= 12) {
                sample = (nibble << shift) >> 1;
            } else {
                sample = (nibble >> 3) << 12;  // Shift 13-15 are invalid
            }

            // Apply filter
            switch (filter) {
                case 0:
                    break;
                case 1:
                    sample += prev1 + ((-prev1) >> 4);
                    break;
                case 2:
                    sample += (prev1 << 1) + ((-((prev1 << 1) + prev1)) >> 5) -
                              prev2 + ((prev2) >> 4);
                    break;
                case 3:
                    sample += (prev1 << 1) + ((-(prev1 + (prev1 << 2) + (prev1 << 3))) >> 6) -
                              prev2 + (((prev2 << 1) + prev2) >> 4);
                    break;
            }

            // Clamp
            sample = std::clamp(sample, -32768, 32767);
            sample = static_cast<int16_t>(sample << 1) >> 1;  // Clip to 15-bit signed

            voice.brr_buffer[i] = static_cast<int16_t>(sample);

            prev2 = prev1;
            prev1 = sample;
        }
    }
}

int16_t DSP::interpolate(int v) {
    auto& voice = m_voices[v];

    // Get fractional position
    int frac = (voice.pitch_counter >> 4) & 0xFF;

    // Get 4 samples for interpolation
    int idx = voice.sample_index;
    int16_t s0 = voice.samples[(idx + 8) % 12];
    int16_t s1 = voice.samples[(idx + 9) % 12];
    int16_t s2 = voice.samples[(idx + 10) % 12];
    int16_t s3 = voice.samples[(idx + 11) % 12];

    // Gaussian interpolation
    int32_t out = (GAUSS_TABLE[255 - frac] * s0) >> 11;
    out += (GAUSS_TABLE[511 - frac] * s1) >> 11;
    out += (GAUSS_TABLE[256 + frac] * s2) >> 11;
    out += (GAUSS_TABLE[frac] * s3) >> 11;

    return static_cast<int16_t>(std::clamp(out, -32768, 32767));
}

void DSP::process_envelope(int v) {
    auto& voice = m_voices[v];

    // Check if using ADSR or GAIN mode
    bool use_adsr = (voice.adsr1 & 0x80) != 0;

    if (use_adsr) {
        // ADSR mode
        int rate;
        switch (voice.envelope_mode) {
            case 1:  // Attack
                rate = ((voice.adsr1 & 0x0F) << 1) + 1;
                if (voice.envelope_level >= 0x7E0) {
                    voice.envelope_level = 0x7FF;
                    voice.envelope_mode = 2;  // Decay
                } else {
                    voice.envelope_level += (rate == 31) ? 1024 : 32;
                }
                break;

            case 2:  // Decay
                rate = ((voice.adsr1 >> 4) & 0x07) * 2 + 16;
                voice.envelope_level -= ((voice.envelope_level - 1) >> 8) + 1;
                if (voice.envelope_level <= ((voice.adsr2 >> 5) + 1) * 0x100) {
                    voice.envelope_mode = 3;  // Sustain
                }
                break;

            case 3:  // Sustain
                rate = voice.adsr2 & 0x1F;
                if (rate != 0) {
                    voice.envelope_level -= ((voice.envelope_level - 1) >> 8) + 1;
                }
                break;

            case 0:  // Release
            default:
                voice.envelope_level -= 8;
                if (voice.envelope_level < 0) voice.envelope_level = 0;
                break;
        }
    } else {
        // GAIN mode
        uint8_t gain = voice.gain;
        if (gain < 0x80) {
            // Direct mode
            voice.envelope_level = gain << 4;
        } else {
            int mode = (gain >> 5) & 0x03;
            int rate = gain & 0x1F;

            switch (mode) {
                case 0:  // Linear decrease
                    voice.envelope_level -= 32;
                    break;
                case 1:  // Exponential decrease
                    voice.envelope_level -= ((voice.envelope_level - 1) >> 8) + 1;
                    break;
                case 2:  // Linear increase
                    voice.envelope_level += 32;
                    break;
                case 3:  // Bent line increase
                    if (voice.envelope_level < 0x600) {
                        voice.envelope_level += 32;
                    } else {
                        voice.envelope_level += 8;
                    }
                    break;
            }
        }

        if (voice.envelope_mode == 0) {
            // Release
            voice.envelope_level -= 8;
        }
    }

    // Clamp envelope
    voice.envelope_level = std::clamp(voice.envelope_level, 0, 0x7FF);
}

void DSP::process_echo() {
    if (!m_spc) return;

    const uint8_t* ram = m_spc->get_ram();

    // Calculate echo buffer parameters
    uint16_t esa = m_regs[REG_ESA] << 8;
    int edl = m_regs[REG_EDL] & 0x0F;
    m_echo_length = edl ? (edl * 0x800) : 4;  // 0 = 4 bytes

    // Read from echo buffer
    uint16_t echo_addr = esa + m_echo_offset;
    int16_t echo_l = ram[echo_addr] | (ram[echo_addr + 1] << 8);
    int16_t echo_r = ram[echo_addr + 2] | (ram[echo_addr + 3] << 8);

    // Store in history buffer
    m_echo_history_left[m_echo_history_index] = echo_l;
    m_echo_history_right[m_echo_history_index] = echo_r;
    m_echo_history_index = (m_echo_history_index + 1) % 8;

    // Advance echo offset
    m_echo_offset += 4;
    if (m_echo_offset >= m_echo_length) {
        m_echo_offset = 0;
    }
}

void DSP::save_state(std::vector<uint8_t>& data) {
    data.push_back(m_address);
    data.insert(data.end(), m_regs.begin(), m_regs.end());

    // Save voice state (simplified)
    for (const auto& voice : m_voices) {
        data.push_back(voice.brr_addr & 0xFF);
        data.push_back(voice.brr_addr >> 8);
        data.push_back(voice.envelope_level & 0xFF);
        data.push_back((voice.envelope_level >> 8) & 0xFF);
        data.push_back(voice.envelope_mode);
    }

    // Save echo state
    data.push_back(m_echo_offset & 0xFF);
    data.push_back((m_echo_offset >> 8) & 0xFF);
}

void DSP::load_state(const uint8_t*& data, size_t& remaining) {
    m_address = *data++; remaining--;
    std::memcpy(m_regs.data(), data, m_regs.size());
    data += m_regs.size(); remaining -= m_regs.size();

    // Load voice state
    for (auto& voice : m_voices) {
        voice.brr_addr = data[0] | (data[1] << 8);
        data += 2; remaining -= 2;
        voice.envelope_level = data[0] | (data[1] << 8);
        data += 2; remaining -= 2;
        voice.envelope_mode = *data++; remaining--;
    }

    // Load echo state
    m_echo_offset = data[0] | (data[1] << 8);
    data += 2; remaining -= 2;

    // Recalculate derived values
    m_noise_rate = m_regs[REG_FLG] & 0x1F;
    for (int i = 0; i < 8; i++) {
        m_fir_coefficients[i] = static_cast<int8_t>(m_regs[0x0F + (i << 4)]);
    }
}

} // namespace snes
