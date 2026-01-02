#include "mapper_085.hpp"
#include <cstring>
#include <algorithm>

namespace nes {

// ========================================
// VRC7 Built-in Instrument Patches
// ========================================
// These are the 15 preset instrument patches from the VRC7
// Based on nesdev wiki OPLL patch set documentation
// Each patch: [AM/VIB/EG/KSR/MULT x2, KSL/TL, KSL/WF+WF/FB, AR/DR x2, SL/RR x2]
const uint8_t Mapper085::s_instrument_patches[15][8] = {
    // Patch 1: Bell
    {0x03, 0x21, 0x05, 0x06, 0xE8, 0x81, 0x42, 0x27},
    // Patch 2: Guitar
    {0x13, 0x41, 0x14, 0x0D, 0xD8, 0xF6, 0x23, 0x12},
    // Patch 3: Piano
    {0x11, 0x11, 0x08, 0x08, 0xFA, 0xB2, 0x20, 0x12},
    // Patch 4: Flute
    {0x31, 0x61, 0x0C, 0x07, 0xA8, 0x64, 0x61, 0x27},
    // Patch 5: Clarinet
    {0x32, 0x21, 0x1E, 0x06, 0xE1, 0x76, 0x01, 0x28},
    // Patch 6: Rattling Bell
    {0x02, 0x01, 0x06, 0x00, 0xA5, 0xE2, 0x35, 0x0F},
    // Patch 7: Trumpet
    {0x21, 0x61, 0x1D, 0x07, 0x82, 0x81, 0x11, 0x07},
    // Patch 8: Reed Organ
    {0x23, 0x21, 0x22, 0x17, 0xA2, 0x72, 0x01, 0x17},
    // Patch 9: Soft Bell
    {0x35, 0x11, 0x25, 0x00, 0x40, 0x73, 0x72, 0x01},
    // Patch 10: Xylophone
    {0xB5, 0x01, 0x0F, 0x0F, 0xA8, 0xA5, 0x51, 0x02},
    // Patch 11: Vibraphone
    {0x17, 0xC1, 0x24, 0x07, 0xF8, 0xF8, 0x22, 0x12},
    // Patch 12: Brass
    {0x71, 0x23, 0x11, 0x06, 0x65, 0x74, 0x18, 0x16},
    // Patch 13: Bass Guitar
    {0x01, 0x02, 0xD3, 0x05, 0xC9, 0x95, 0x03, 0x02},
    // Patch 14: Synthesizer
    {0x61, 0x63, 0x0C, 0x00, 0x94, 0xC0, 0x33, 0xF6},
    // Patch 15: Chorus
    {0x21, 0x72, 0x0D, 0x00, 0xC1, 0xD5, 0x56, 0x06}
};

// ========================================
// Log-sine table (256 entries)
// ========================================
// This table contains log2(sin(x)) * 256 for x = 0 to pi/2
// Used for the half-sine waveform of OPLL
// Format: 12-bit values representing -log2(sin(phase)) * 256
const uint16_t Mapper085::s_log_sin_table[256] = {
    0x859, 0x6C3, 0x607, 0x58B, 0x52E, 0x4E4, 0x4A6, 0x471,
    0x443, 0x41A, 0x3F5, 0x3D3, 0x3B5, 0x398, 0x37E, 0x365,
    0x34E, 0x339, 0x324, 0x311, 0x2FF, 0x2ED, 0x2DC, 0x2CD,
    0x2BD, 0x2AF, 0x2A0, 0x293, 0x286, 0x279, 0x26D, 0x261,
    0x256, 0x24B, 0x240, 0x236, 0x22C, 0x222, 0x218, 0x20F,
    0x206, 0x1FD, 0x1F5, 0x1EC, 0x1E4, 0x1DC, 0x1D4, 0x1CD,
    0x1C5, 0x1BE, 0x1B7, 0x1B0, 0x1A9, 0x1A2, 0x19B, 0x195,
    0x18F, 0x188, 0x182, 0x17C, 0x177, 0x171, 0x16B, 0x166,
    0x160, 0x15B, 0x155, 0x150, 0x14B, 0x146, 0x141, 0x13C,
    0x137, 0x133, 0x12E, 0x129, 0x125, 0x121, 0x11C, 0x118,
    0x114, 0x10F, 0x10B, 0x107, 0x103, 0x0FF, 0x0FB, 0x0F8,
    0x0F4, 0x0F0, 0x0EC, 0x0E9, 0x0E5, 0x0E2, 0x0DE, 0x0DB,
    0x0D7, 0x0D4, 0x0D1, 0x0CD, 0x0CA, 0x0C7, 0x0C4, 0x0C1,
    0x0BE, 0x0BB, 0x0B8, 0x0B5, 0x0B2, 0x0AF, 0x0AC, 0x0A9,
    0x0A7, 0x0A4, 0x0A1, 0x09F, 0x09C, 0x099, 0x097, 0x094,
    0x092, 0x08F, 0x08D, 0x08A, 0x088, 0x086, 0x083, 0x081,
    0x07F, 0x07D, 0x07A, 0x078, 0x076, 0x074, 0x072, 0x070,
    0x06E, 0x06C, 0x06A, 0x068, 0x066, 0x064, 0x062, 0x060,
    0x05E, 0x05C, 0x05B, 0x059, 0x057, 0x055, 0x053, 0x052,
    0x050, 0x04E, 0x04D, 0x04B, 0x04A, 0x048, 0x046, 0x045,
    0x043, 0x042, 0x040, 0x03F, 0x03E, 0x03C, 0x03B, 0x039,
    0x038, 0x037, 0x035, 0x034, 0x033, 0x031, 0x030, 0x02F,
    0x02E, 0x02D, 0x02B, 0x02A, 0x029, 0x028, 0x027, 0x026,
    0x025, 0x024, 0x023, 0x022, 0x021, 0x020, 0x01F, 0x01E,
    0x01D, 0x01C, 0x01B, 0x01A, 0x019, 0x018, 0x017, 0x017,
    0x016, 0x015, 0x014, 0x014, 0x013, 0x012, 0x011, 0x011,
    0x010, 0x00F, 0x00F, 0x00E, 0x00D, 0x00D, 0x00C, 0x00C,
    0x00B, 0x00A, 0x00A, 0x009, 0x009, 0x008, 0x008, 0x007,
    0x007, 0x007, 0x006, 0x006, 0x005, 0x005, 0x005, 0x004,
    0x004, 0x004, 0x003, 0x003, 0x003, 0x002, 0x002, 0x002,
    0x002, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001,
    0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000
};

// ========================================
// Exponential table (256 entries)
// ========================================
// Converts log2 values back to linear
// exp_table[x] = 2^((255-x)/256) * 1024
const uint16_t Mapper085::s_exp_table[256] = {
    0x3FF, 0x3F5, 0x3EB, 0x3E1, 0x3D7, 0x3CD, 0x3C3, 0x3B9,
    0x3B0, 0x3A6, 0x39D, 0x393, 0x38A, 0x381, 0x378, 0x36F,
    0x366, 0x35D, 0x354, 0x34B, 0x342, 0x339, 0x331, 0x328,
    0x320, 0x317, 0x30F, 0x307, 0x2FE, 0x2F6, 0x2EE, 0x2E6,
    0x2DE, 0x2D6, 0x2CE, 0x2C6, 0x2BF, 0x2B7, 0x2AF, 0x2A8,
    0x2A0, 0x299, 0x291, 0x28A, 0x283, 0x27C, 0x274, 0x26D,
    0x266, 0x25F, 0x258, 0x251, 0x24B, 0x244, 0x23D, 0x237,
    0x230, 0x22A, 0x223, 0x21D, 0x216, 0x210, 0x20A, 0x204,
    0x1FE, 0x1F8, 0x1F2, 0x1EC, 0x1E6, 0x1E0, 0x1DA, 0x1D4,
    0x1CF, 0x1C9, 0x1C3, 0x1BE, 0x1B8, 0x1B3, 0x1AD, 0x1A8,
    0x1A3, 0x19D, 0x198, 0x193, 0x18E, 0x189, 0x184, 0x17F,
    0x17A, 0x175, 0x170, 0x16B, 0x166, 0x162, 0x15D, 0x158,
    0x154, 0x14F, 0x14B, 0x146, 0x142, 0x13D, 0x139, 0x135,
    0x130, 0x12C, 0x128, 0x124, 0x120, 0x11C, 0x118, 0x114,
    0x110, 0x10C, 0x108, 0x104, 0x100, 0x0FC, 0x0F9, 0x0F5,
    0x0F1, 0x0EE, 0x0EA, 0x0E7, 0x0E3, 0x0E0, 0x0DC, 0x0D9,
    0x0D6, 0x0D2, 0x0CF, 0x0CC, 0x0C9, 0x0C5, 0x0C2, 0x0BF,
    0x0BC, 0x0B9, 0x0B6, 0x0B3, 0x0B0, 0x0AD, 0x0AA, 0x0A7,
    0x0A5, 0x0A2, 0x09F, 0x09C, 0x09A, 0x097, 0x094, 0x092,
    0x08F, 0x08D, 0x08A, 0x088, 0x085, 0x083, 0x080, 0x07E,
    0x07C, 0x079, 0x077, 0x075, 0x073, 0x070, 0x06E, 0x06C,
    0x06A, 0x068, 0x066, 0x064, 0x062, 0x060, 0x05E, 0x05C,
    0x05A, 0x058, 0x056, 0x054, 0x052, 0x051, 0x04F, 0x04D,
    0x04B, 0x04A, 0x048, 0x046, 0x045, 0x043, 0x041, 0x040,
    0x03E, 0x03D, 0x03B, 0x03A, 0x038, 0x037, 0x036, 0x034,
    0x033, 0x031, 0x030, 0x02F, 0x02D, 0x02C, 0x02B, 0x02A,
    0x028, 0x027, 0x026, 0x025, 0x024, 0x022, 0x021, 0x020,
    0x01F, 0x01E, 0x01D, 0x01C, 0x01B, 0x01A, 0x019, 0x018,
    0x017, 0x016, 0x015, 0x014, 0x013, 0x013, 0x012, 0x011,
    0x010, 0x00F, 0x00F, 0x00E, 0x00D, 0x00D, 0x00C, 0x00B,
    0x00B, 0x00A, 0x00A, 0x009, 0x008, 0x008, 0x007, 0x007,
    0x006, 0x006, 0x006, 0x005, 0x005, 0x004, 0x004, 0x004
};

// Multiplier table: MULT value (0-15) -> actual frequency multiplier * 2
const uint8_t Mapper085::s_multiplier_table[16] = {
    1,    // 0 -> 0.5 (stored as 1 for x/2)
    2,    // 1 -> 1
    4,    // 2 -> 2
    6,    // 3 -> 3
    8,    // 4 -> 4
    10,   // 5 -> 5
    12,   // 6 -> 6
    14,   // 7 -> 7
    16,   // 8 -> 8
    18,   // 9 -> 9
    20,   // 10 -> 10
    20,   // 11 -> 10 (duplicate)
    24,   // 12 -> 12
    24,   // 13 -> 12 (duplicate)
    30,   // 14 -> 15
    30    // 15 -> 15 (duplicate)
};

// Key scale level table (octave/note -> attenuation in dB * 4)
const uint8_t Mapper085::s_ksl_table[8][16] = {
    {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},   // Octave 0
    {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},   // Octave 1
    {0,  0,  0,  0,  0,  0,  0,  0,  0,  8, 12, 16, 20, 24, 28, 32},   // Octave 2
    {0,  0,  0,  0,  0, 12, 20, 28, 32, 40, 44, 48, 52, 56, 60, 64},   // Octave 3
    {0,  0,  0, 20, 32, 44, 52, 60, 64, 72, 76, 80, 84, 88, 92, 96},   // Octave 4
    {0,  0, 32, 52, 64, 76, 84, 92, 96,104,108,112,116,120,124,128},   // Octave 5
    {0, 32, 64, 84, 96,108,116,124,128,136,140,144,148,152,156,160},   // Octave 6
    {0, 64, 96,116,128,140,148,156,160,168,172,176,180,184,188,192}    // Octave 7
};

// Attack rate increment table
const uint8_t Mapper085::s_attack_table[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8
};

// Decay rate increment table
const uint8_t Mapper085::s_decay_table[64] = {
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6
};

Mapper085::Mapper085(std::vector<uint8_t>& prg_rom,
                     std::vector<uint8_t>& chr_rom,
                     std::vector<uint8_t>& prg_ram,
                     MirrorMode mirror,
                     bool has_chr_ram)
{
    m_prg_rom = &prg_rom;
    m_chr_rom = &chr_rom;
    m_prg_ram = &prg_ram;
    m_mirror_mode = mirror;
    m_has_chr_ram = has_chr_ram;

    reset();
}

void Mapper085::reset() {
    // PRG banking
    for (int i = 0; i < 3; i++) {
        m_prg_bank[i] = 0;
    }

    // CHR banking
    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = i;
    }

    // IRQ
    m_irq_latch = 0;
    m_irq_counter = 0;
    m_irq_enabled = false;
    m_irq_enabled_after_ack = false;
    m_irq_pending = false;
    m_irq_mode_cycle = false;
    m_irq_prescaler = 0;

    // Audio
    m_audio_silence = false;

    // FM synthesis
    m_fm_address = 0;

    for (int i = 0; i < 8; i++) {
        m_custom_instrument[i] = 0;
    }

    for (int i = 0; i < 6; i++) {
        m_fnum_low[i] = 0;
        m_channel_ctrl[i] = 0;
        m_channel_vol[i] = 0;
    }

    // FM channel state
    for (int i = 0; i < FM_CHANNELS; i++) {
        m_phase_mod[i] = 0;
        m_phase_car[i] = 0;
        m_env_state_mod[i] = EnvState::Off;
        m_env_state_car[i] = EnvState::Off;
        m_env_level_mod[i] = 0x3FF;  // Max attenuation (silence)
        m_env_level_car[i] = 0x3FF;
        m_env_counter_mod[i] = 0;
        m_env_counter_car[i] = 0;
        m_prev_key_on[i] = false;
        m_feedback_mod[i][0] = 0;
        m_feedback_mod[i][1] = 0;
    }

    m_fm_clock_counter = 0;

    update_prg_banks();
    update_chr_banks();
}

void Mapper085::update_prg_banks() {
    size_t prg_size = m_prg_rom->size();
    size_t num_8k_banks = prg_size / 0x2000;
    if (num_8k_banks == 0) num_8k_banks = 1;

    // Three switchable 8KB banks
    for (int i = 0; i < 3; i++) {
        m_prg_bank_offset[i] = (m_prg_bank[i] % num_8k_banks) * 0x2000;
    }

    // Fixed last bank at $E000
    m_prg_bank_offset[3] = (num_8k_banks - 1) * 0x2000;
}

void Mapper085::update_chr_banks() {
    if (m_chr_rom->empty()) return;

    size_t chr_size = m_chr_rom->size();
    size_t num_1k_banks = chr_size / 0x400;
    if (num_1k_banks == 0) num_1k_banks = 1;

    for (int i = 0; i < 8; i++) {
        m_chr_bank_offset[i] = (m_chr_bank[i] % num_1k_banks) * 0x400;
    }
}

const uint8_t* Mapper085::get_instrument(int patch) const {
    if (patch == 0) {
        return m_custom_instrument;
    } else if (patch >= 1 && patch <= 15) {
        return s_instrument_patches[patch - 1];
    }
    return s_instrument_patches[0];  // Default to patch 1
}

uint8_t Mapper085::cpu_read(uint16_t address) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            return (*m_prg_ram)[address & 0x1FFF];
        }
        return 0;
    }

    // PRG ROM: $8000-$FFFF (four 8KB banks)
    if (address >= 0x8000) {
        int bank = (address - 0x8000) / 0x2000;
        uint32_t offset = m_prg_bank_offset[bank] + (address & 0x1FFF);
        if (offset < m_prg_rom->size()) {
            return (*m_prg_rom)[offset];
        }
    }

    return 0;
}

void Mapper085::cpu_write(uint16_t address, uint8_t value) {
    // PRG RAM: $6000-$7FFF
    if (address >= 0x6000 && address < 0x8000) {
        if (!m_prg_ram->empty()) {
            (*m_prg_ram)[address & 0x1FFF] = value;
        }
        return;
    }

    // VRC7 registers
    // The VRC7 has a weird address mapping:
    // $8000: PRG bank 0 (8KB at $8000)
    // $8010: PRG bank 1 (8KB at $A000)
    // $9000: PRG bank 2 (8KB at $C000)
    // $9010: Audio address latch
    // $9030: Audio data
    // $A000: CHR bank 0
    // $A010: CHR bank 1
    // $B000: CHR bank 2
    // $B010: CHR bank 3
    // $C000: CHR bank 4
    // $C010: CHR bank 5
    // $D000: CHR bank 6
    // $D010: CHR bank 7
    // $E000: Mirroring + audio silence
    // $E010: IRQ latch
    // $F000: IRQ control
    // $F010: IRQ acknowledge

    // Extract register address using VRC7 decoding
    // A4 and A3 determine sub-register within major address
    uint16_t reg = address & 0xF038;

    // PRG bank 0: $8000
    if (reg == 0x8000) {
        m_prg_bank[0] = value & 0x3F;
        update_prg_banks();
        return;
    }

    // PRG bank 1: $8010 or $8008 or $8018
    if ((address & 0xF030) == 0x8010 || (address & 0xF038) == 0x8008 || (address & 0xF038) == 0x8018) {
        m_prg_bank[1] = value & 0x3F;
        update_prg_banks();
        return;
    }

    // PRG bank 2: $9000
    if (reg == 0x9000) {
        m_prg_bank[2] = value & 0x3F;
        update_prg_banks();
        return;
    }

    // Audio address latch: $9010 or $9008 or $9018
    if ((address & 0xF030) == 0x9010 || (address & 0xF038) == 0x9008 || (address & 0xF038) == 0x9018) {
        m_fm_address = value;
        return;
    }

    // Audio data: $9030 or $9028 or $9038
    if ((address & 0xF030) == 0x9030 || (address & 0xF038) == 0x9028 || (address & 0xF038) == 0x9038) {
        fm_write_register(m_fm_address, value);
        return;
    }

    // CHR banks
    if (address >= 0xA000 && address < 0xE000) {
        int chr_reg = ((address - 0xA000) >> 12) * 2;
        if ((address & 0x0030) == 0x0010 || (address & 0x0038) == 0x0008 || (address & 0x0038) == 0x0018) {
            chr_reg++;
        }
        if (chr_reg < 8) {
            m_chr_bank[chr_reg] = value;
            update_chr_banks();
        }
        return;
    }

    // Mirroring + audio silence: $E000
    if (reg == 0xE000) {
        switch (value & 0x03) {
            case 0: m_mirror_mode = MirrorMode::Vertical; break;
            case 1: m_mirror_mode = MirrorMode::Horizontal; break;
            case 2: m_mirror_mode = MirrorMode::SingleScreen0; break;
            case 3: m_mirror_mode = MirrorMode::SingleScreen1; break;
        }
        m_audio_silence = (value & 0x40) != 0;
        return;
    }

    // IRQ latch: $E010 or $E008 or $E018
    if ((address & 0xF030) == 0xE010 || (address & 0xF038) == 0xE008 || (address & 0xF038) == 0xE018) {
        m_irq_latch = value;
        return;
    }

    // IRQ control: $F000
    if (reg == 0xF000) {
        m_irq_enabled_after_ack = (value & 0x01) != 0;
        m_irq_enabled = (value & 0x02) != 0;
        m_irq_mode_cycle = (value & 0x04) != 0;

        if (m_irq_enabled) {
            m_irq_counter = m_irq_latch;
            m_irq_prescaler = 0;
        }

        m_irq_pending = false;
        return;
    }

    // IRQ acknowledge: $F010 or $F008 or $F018
    if ((address & 0xF030) == 0xF010 || (address & 0xF038) == 0xF008 || (address & 0xF038) == 0xF018) {
        m_irq_pending = false;
        m_irq_enabled = m_irq_enabled_after_ack;
        return;
    }
}

void Mapper085::fm_write_register(uint8_t reg, uint8_t value) {
    // Custom instrument: $00-$07
    if (reg <= 0x07) {
        m_custom_instrument[reg] = value;
        // Update any channels using custom instrument
        for (int ch = 0; ch < FM_CHANNELS; ch++) {
            if ((m_channel_vol[ch] >> 4) == 0) {
                update_channel_instrument(ch);
            }
        }
        return;
    }

    // F-Number low: $10-$15
    if (reg >= 0x10 && reg <= 0x15) {
        int ch = reg - 0x10;
        m_fnum_low[ch] = value;
        return;
    }

    // Channel control (sustain/key on/block/fnum high): $20-$25
    if (reg >= 0x20 && reg <= 0x25) {
        int ch = reg - 0x20;
        bool old_key_on = (m_channel_ctrl[ch] & 0x10) != 0;
        m_channel_ctrl[ch] = value;
        bool new_key_on = (value & 0x10) != 0;

        // Key on edge detection
        if (new_key_on && !old_key_on) {
            // Key on - start attack phase
            m_env_state_mod[ch] = EnvState::Attack;
            m_env_state_car[ch] = EnvState::Attack;
            m_env_counter_mod[ch] = 0;
            m_env_counter_car[ch] = 0;
            // Reset phase on key on for cleaner sound
            m_phase_mod[ch] = 0;
            m_phase_car[ch] = 0;
        } else if (!new_key_on && old_key_on) {
            // Key off - start release phase
            m_env_state_mod[ch] = EnvState::Release;
            m_env_state_car[ch] = EnvState::Release;
        }

        m_prev_key_on[ch] = new_key_on;
        return;
    }

    // Channel volume/instrument: $30-$35
    if (reg >= 0x30 && reg <= 0x35) {
        int ch = reg - 0x30;
        m_channel_vol[ch] = value;
        update_channel_instrument(ch);
        return;
    }
}

void Mapper085::update_channel_instrument(int ch) {
    // This could be used to cache instrument parameters per channel
    // For now, we read instrument data directly in calculate_channel_output
    (void)ch;
}

void Mapper085::clock_fm() {
    // Process envelope generators for all channels
    for (int ch = 0; ch < FM_CHANNELS; ch++) {
        const uint8_t* inst = get_instrument(m_channel_vol[ch] >> 4);

        // Get envelope parameters from instrument
        uint8_t mod_adsr1 = inst[4];  // AR/DR for modulator
        uint8_t mod_adsr2 = inst[6];  // SL/RR for modulator
        uint8_t car_adsr1 = inst[5];  // AR/DR for carrier
        uint8_t car_adsr2 = inst[7];  // SL/RR for carrier

        uint8_t mod_ar = (mod_adsr1 >> 4) & 0x0F;
        uint8_t mod_dr = mod_adsr1 & 0x0F;
        uint8_t mod_sl = (mod_adsr2 >> 4) & 0x0F;
        uint8_t mod_rr = mod_adsr2 & 0x0F;

        uint8_t car_ar = (car_adsr1 >> 4) & 0x0F;
        uint8_t car_dr = car_adsr1 & 0x0F;
        uint8_t car_sl = (car_adsr2 >> 4) & 0x0F;
        uint8_t car_rr = car_adsr2 & 0x0F;

        // Get EG type flags
        bool mod_eg = (inst[0] & 0x20) != 0;  // EG type (sustain behavior)
        bool car_eg = (inst[1] & 0x20) != 0;

        // Sustain flag from channel control
        bool sustain = (m_channel_ctrl[ch] & 0x20) != 0;

        // Process modulator envelope
        m_env_counter_mod[ch]++;
        switch (m_env_state_mod[ch]) {
            case EnvState::Attack:
                if (mod_ar == 15) {
                    m_env_level_mod[ch] = 0;
                    m_env_state_mod[ch] = EnvState::Decay;
                } else if (mod_ar > 0 && (m_env_counter_mod[ch] & ((1 << (14 - mod_ar)) - 1)) == 0) {
                    uint16_t step = (0x3FF - m_env_level_mod[ch]) >> 2;
                    if (step == 0) step = 1;
                    m_env_level_mod[ch] = (m_env_level_mod[ch] > step) ? m_env_level_mod[ch] - step : 0;
                    if (m_env_level_mod[ch] == 0) {
                        m_env_state_mod[ch] = EnvState::Decay;
                    }
                }
                break;

            case EnvState::Decay: {
                uint16_t sl_level = mod_sl << 6;  // Sustain level (0-960)
                if (mod_dr > 0 && (m_env_counter_mod[ch] & ((1 << (14 - mod_dr)) - 1)) == 0) {
                    m_env_level_mod[ch] += 4;
                    if (m_env_level_mod[ch] >= sl_level) {
                        m_env_level_mod[ch] = sl_level;
                        m_env_state_mod[ch] = mod_eg ? EnvState::Sustain : EnvState::Release;
                    }
                }
                break;
            }

            case EnvState::Sustain:
                // Stay at sustain level until key off
                if (!sustain) {
                    m_env_state_mod[ch] = EnvState::Release;
                }
                break;

            case EnvState::Release:
                if (mod_rr > 0 && (m_env_counter_mod[ch] & ((1 << (14 - mod_rr)) - 1)) == 0) {
                    m_env_level_mod[ch] += 4;
                    if (m_env_level_mod[ch] >= 0x3FF) {
                        m_env_level_mod[ch] = 0x3FF;
                        m_env_state_mod[ch] = EnvState::Off;
                    }
                }
                break;

            case EnvState::Off:
                m_env_level_mod[ch] = 0x3FF;
                break;
        }

        // Process carrier envelope (similar to modulator)
        m_env_counter_car[ch]++;
        switch (m_env_state_car[ch]) {
            case EnvState::Attack:
                if (car_ar == 15) {
                    m_env_level_car[ch] = 0;
                    m_env_state_car[ch] = EnvState::Decay;
                } else if (car_ar > 0 && (m_env_counter_car[ch] & ((1 << (14 - car_ar)) - 1)) == 0) {
                    uint16_t step = (0x3FF - m_env_level_car[ch]) >> 2;
                    if (step == 0) step = 1;
                    m_env_level_car[ch] = (m_env_level_car[ch] > step) ? m_env_level_car[ch] - step : 0;
                    if (m_env_level_car[ch] == 0) {
                        m_env_state_car[ch] = EnvState::Decay;
                    }
                }
                break;

            case EnvState::Decay: {
                uint16_t sl_level = car_sl << 6;
                if (car_dr > 0 && (m_env_counter_car[ch] & ((1 << (14 - car_dr)) - 1)) == 0) {
                    m_env_level_car[ch] += 4;
                    if (m_env_level_car[ch] >= sl_level) {
                        m_env_level_car[ch] = sl_level;
                        m_env_state_car[ch] = car_eg ? EnvState::Sustain : EnvState::Release;
                    }
                }
                break;
            }

            case EnvState::Sustain:
                if (!sustain) {
                    m_env_state_car[ch] = EnvState::Release;
                }
                break;

            case EnvState::Release:
                if (car_rr > 0 && (m_env_counter_car[ch] & ((1 << (14 - car_rr)) - 1)) == 0) {
                    m_env_level_car[ch] += 4;
                    if (m_env_level_car[ch] >= 0x3FF) {
                        m_env_level_car[ch] = 0x3FF;
                        m_env_state_car[ch] = EnvState::Off;
                    }
                }
                break;

            case EnvState::Off:
                m_env_level_car[ch] = 0x3FF;
                break;
        }

        // Update phase accumulators
        uint16_t fnum = m_fnum_low[ch] | ((m_channel_ctrl[ch] & 0x01) << 8);
        uint8_t block = (m_channel_ctrl[ch] >> 1) & 0x07;

        // Get multipliers from instrument
        uint8_t mod_mult_idx = inst[0] & 0x0F;
        uint8_t car_mult_idx = inst[1] & 0x0F;
        uint8_t mod_mult = s_multiplier_table[mod_mult_idx];
        uint8_t car_mult = s_multiplier_table[car_mult_idx];

        // Phase increment = fnum * 2^block * multiplier / 2
        uint32_t phase_inc = (fnum << block) >> 1;

        // Apply multipliers
        uint32_t phase_inc_mod = (phase_inc * mod_mult) >> 1;
        uint32_t phase_inc_car = (phase_inc * car_mult) >> 1;

        m_phase_mod[ch] = (m_phase_mod[ch] + phase_inc_mod) & 0x3FFFF;
        m_phase_car[ch] = (m_phase_car[ch] + phase_inc_car) & 0x3FFFF;
    }
}

float Mapper085::calculate_channel_output(int ch) {
    if (m_audio_silence) return 0.0f;
    if (m_env_state_car[ch] == EnvState::Off) return 0.0f;

    const uint8_t* inst = get_instrument(m_channel_vol[ch] >> 4);

    // Get modulator output first
    // Phase is 18-bit, we use top 10 bits for lookup (256 * 4 for quarter sine symmetry)
    uint32_t mod_phase = m_phase_mod[ch] >> 8;  // 10-bit phase

    // Apply feedback to modulator
    uint8_t fb = (inst[3] >> 0) & 0x07;  // Feedback level (0-7)
    if (fb > 0) {
        int16_t fb_value = (m_feedback_mod[ch][0] + m_feedback_mod[ch][1]) >> (8 - fb);
        mod_phase = (mod_phase + fb_value) & 0x3FF;
    }

    // Look up sine value with waveform selection
    bool mod_wf = (inst[3] & 0x08) != 0;  // Modulator waveform (0=sine, 1=half-sine)

    // Convert phase to quarter-sine index (0-255)
    uint16_t quarter = (mod_phase >> 8) & 0x03;
    uint8_t index = mod_phase & 0xFF;

    // Mirror for second half of each quarter
    if (quarter & 0x01) {
        index = 255 - index;
    }

    uint16_t log_sin = s_log_sin_table[index];

    // Negate for second half of full sine
    bool negate_mod = (quarter >= 2);

    // For half-sine waveform, output 0 for negative half
    if (mod_wf && negate_mod) {
        log_sin = 0x0FFF;  // Maximum attenuation
    }

    // Apply modulator envelope and total level
    uint8_t tl = inst[2] & 0x3F;  // Total level (0-63)
    uint16_t mod_atten = (m_env_level_mod[ch] >> 2) + (tl << 2);
    if (mod_atten > 255) mod_atten = 255;

    uint16_t total_atten = log_sin + (mod_atten << 3);
    if (total_atten > 0x0FFF) total_atten = 0x0FFF;

    // Convert back to linear
    int16_t mod_output = s_exp_table[total_atten & 0xFF] >> (total_atten >> 8);
    if (negate_mod && !mod_wf) mod_output = -mod_output;

    // Update feedback history
    m_feedback_mod[ch][1] = m_feedback_mod[ch][0];
    m_feedback_mod[ch][0] = mod_output;

    // Calculate carrier phase with modulation
    uint32_t car_phase = (m_phase_car[ch] >> 8) + (mod_output >> 1);
    car_phase &= 0x3FF;

    // Look up carrier sine
    bool car_wf = (inst[3] & 0x10) != 0;  // Carrier waveform

    quarter = (car_phase >> 8) & 0x03;
    index = car_phase & 0xFF;

    if (quarter & 0x01) {
        index = 255 - index;
    }

    log_sin = s_log_sin_table[index];

    bool negate_car = (quarter >= 2);

    if (car_wf && negate_car) {
        log_sin = 0x0FFF;
    }

    // Apply carrier envelope and channel volume
    uint8_t vol = m_channel_vol[ch] & 0x0F;  // Volume (0-15, inverted)
    uint16_t car_atten = (m_env_level_car[ch] >> 2) + (vol << 3);
    if (car_atten > 255) car_atten = 255;

    total_atten = log_sin + (car_atten << 3);
    if (total_atten > 0x0FFF) total_atten = 0x0FFF;

    // Convert to linear output
    int16_t car_output = s_exp_table[total_atten & 0xFF] >> (total_atten >> 8);
    if (negate_car && !car_wf) car_output = -car_output;

    // Normalize to float (-1.0 to 1.0 range)
    return car_output / 1024.0f;
}

float Mapper085::get_fm_sample() {
    // Clock FM synthesis
    m_fm_clock_counter++;
    if (m_fm_clock_counter >= FM_CLOCK_DIVIDER) {
        m_fm_clock_counter = 0;
        clock_fm();
    }

    if (m_audio_silence) {
        return 0.0f;
    }

    // Mix all 6 FM channels
    float output = 0.0f;
    for (int ch = 0; ch < FM_CHANNELS; ch++) {
        output += calculate_channel_output(ch);
    }

    // Scale down to prevent clipping (6 channels)
    output *= 0.15f;

    return output;
}

uint8_t Mapper085::ppu_read(uint16_t address, [[maybe_unused]] uint32_t frame_cycle) {
    if (address < 0x2000) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            return (*m_chr_rom)[offset];
        }
    }
    return 0;
}

void Mapper085::ppu_write(uint16_t address, uint8_t value) {
    if (address < 0x2000 && m_has_chr_ram) {
        int bank = address / 0x400;
        uint32_t offset = m_chr_bank_offset[bank] + (address & 0x3FF);
        if (offset < m_chr_rom->size()) {
            (*m_chr_rom)[offset] = value;
        }
    }
}

void Mapper085::scanline() {
    // Scanline mode IRQ (when not in cycle mode)
    if (!m_irq_mode_cycle && m_irq_enabled) {
        if (m_irq_counter == 0xFF) {
            m_irq_counter = m_irq_latch;
            m_irq_pending = true;
        } else {
            m_irq_counter++;
        }
    }
}

void Mapper085::cpu_cycles(int count) {
    // PERFORMANCE: Process all cycles at once instead of one at a time

    // Cycle mode IRQ - batch process prescaler increment
    if (m_irq_mode_cycle && m_irq_enabled) {
        int remaining = count;
        while (remaining > 0) {
            int cycles_to_tick = 341 - m_irq_prescaler;
            if (remaining < cycles_to_tick) {
                // Won't tick this batch
                m_irq_prescaler += remaining;
                remaining = 0;
            } else {
                // Prescaler will overflow - clock IRQ counter
                remaining -= cycles_to_tick;
                m_irq_prescaler = 0;

                if (m_irq_counter == 0xFF) {
                    m_irq_counter = m_irq_latch;
                    m_irq_pending = true;
                } else {
                    m_irq_counter++;
                }
            }
        }
    }
}

void Mapper085::cpu_cycle() {
    // Single-cycle version for compatibility - delegates to batched version
    cpu_cycles(1);
}

void Mapper085::save_state(std::vector<uint8_t>& data) {
    // PRG banks
    for (int i = 0; i < 3; i++) {
        data.push_back(m_prg_bank[i]);
    }

    // CHR banks
    for (int i = 0; i < 8; i++) {
        data.push_back(m_chr_bank[i]);
    }

    // IRQ state
    data.push_back(m_irq_latch);
    data.push_back(m_irq_counter);
    data.push_back(m_irq_enabled ? 1 : 0);
    data.push_back(m_irq_enabled_after_ack ? 1 : 0);
    data.push_back(m_irq_pending ? 1 : 0);
    data.push_back(m_irq_mode_cycle ? 1 : 0);
    data.push_back(m_irq_prescaler & 0xFF);
    data.push_back((m_irq_prescaler >> 8) & 0xFF);

    // Audio state
    data.push_back(m_audio_silence ? 1 : 0);
    data.push_back(m_fm_address);

    // Custom instrument
    for (int i = 0; i < 8; i++) {
        data.push_back(m_custom_instrument[i]);
    }

    // Channel registers
    for (int i = 0; i < 6; i++) {
        data.push_back(m_fnum_low[i]);
        data.push_back(m_channel_ctrl[i]);
        data.push_back(m_channel_vol[i]);
    }

    // Mirror mode
    data.push_back(static_cast<uint8_t>(m_mirror_mode));

    // FM synthesis state would be here for full save state support
    // For brevity, we'll let the FM state rebuild naturally
}

void Mapper085::load_state(const uint8_t*& data, size_t& remaining) {
    if (remaining < 40) return;  // Minimum state size

    // PRG banks
    for (int i = 0; i < 3; i++) {
        m_prg_bank[i] = *data++; remaining--;
    }

    // CHR banks
    for (int i = 0; i < 8; i++) {
        m_chr_bank[i] = *data++; remaining--;
    }

    // IRQ state
    m_irq_latch = *data++; remaining--;
    m_irq_counter = *data++; remaining--;
    m_irq_enabled = (*data++ != 0); remaining--;
    m_irq_enabled_after_ack = (*data++ != 0); remaining--;
    m_irq_pending = (*data++ != 0); remaining--;
    m_irq_mode_cycle = (*data++ != 0); remaining--;
    m_irq_prescaler = *data++; remaining--;
    m_irq_prescaler |= (*data++ << 8); remaining--;

    // Audio state
    m_audio_silence = (*data++ != 0); remaining--;
    m_fm_address = *data++; remaining--;

    // Custom instrument
    for (int i = 0; i < 8; i++) {
        m_custom_instrument[i] = *data++; remaining--;
    }

    // Channel registers
    for (int i = 0; i < 6; i++) {
        m_fnum_low[i] = *data++; remaining--;
        m_channel_ctrl[i] = *data++; remaining--;
        m_channel_vol[i] = *data++; remaining--;
    }

    // Mirror mode
    m_mirror_mode = static_cast<MirrorMode>(*data++); remaining--;

    // Reset FM synthesis state
    for (int i = 0; i < FM_CHANNELS; i++) {
        m_phase_mod[i] = 0;
        m_phase_car[i] = 0;
        m_env_state_mod[i] = EnvState::Off;
        m_env_state_car[i] = EnvState::Off;
        m_env_level_mod[i] = 0x3FF;
        m_env_level_car[i] = 0x3FF;
        m_env_counter_mod[i] = 0;
        m_env_counter_car[i] = 0;
        m_prev_key_on[i] = false;
        m_feedback_mod[i][0] = 0;
        m_feedback_mod[i][1] = 0;
    }

    update_prg_banks();
    update_chr_banks();
}

} // namespace nes
