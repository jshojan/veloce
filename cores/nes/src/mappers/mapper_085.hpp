#pragma once

#include "mapper.hpp"
#include <array>
#include <cmath>

namespace nes {

// Mapper 085: Konami VRC7
// Used by: Lagrange Point (incredible FM soundtrack), Tiny Toon Adventures 2
//
// Features:
// - Three 8KB switchable PRG ROM banks + fixed 8KB bank
// - Eight 1KB switchable CHR ROM banks
// - VRC-style IRQ counter with prescaler
// - VRC7 FM synthesis audio expansion (based on Yamaha YM2413/OPLL)
//
// The VRC7 contains a cost-reduced OPLL with:
// - 6 FM channels (vs 9 in full OPLL)
// - 15 built-in instrument presets (read-only)
// - 1 custom instrument (user-programmable)
// - No rhythm mode
class Mapper085 : public Mapper {
public:
    Mapper085(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

    bool irq_pending(uint32_t frame_cycle = 0) override { return m_irq_pending; }
    void irq_clear() override { m_irq_pending = false; }

    void scanline() override;

    void reset() override;

    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

    // CPU cycle notification for IRQ counter (cycle mode)
    // PERFORMANCE: Batched version - receives cycle count for efficient processing
    void cpu_cycles(int count) override;
    void cpu_cycle() override;

    // Get FM audio samples - returns mixed FM output for blending with APU
    // This should be called by the emulator's audio system
    float get_fm_sample();

    // Clock FM synthesis at VRC7 rate (~49716 Hz = 3.58MHz / 72)
    void clock_fm();

private:
    void update_prg_banks();
    void update_chr_banks();

    // FM synthesis internals
    void fm_write_register(uint8_t reg, uint8_t value);
    void update_channel_instrument(int ch);
    float calculate_channel_output(int ch);

    // PRG banking - three 8KB switchable + one fixed
    uint8_t m_prg_bank[3] = {0};  // Banks at $8000, $A000, $C000
    uint32_t m_prg_bank_offset[4] = {0};  // Includes fixed bank at $E000

    // CHR banking - eight 1KB banks
    uint8_t m_chr_bank[8] = {0};
    uint32_t m_chr_bank_offset[8] = {0};

    // IRQ
    uint8_t m_irq_latch = 0;
    uint8_t m_irq_counter = 0;
    bool m_irq_enabled = false;
    bool m_irq_enabled_after_ack = false;
    bool m_irq_pending = false;
    bool m_irq_mode_cycle = false;  // false = scanline mode, true = cycle mode
    uint16_t m_irq_prescaler = 0;
    static constexpr int IRQ_PRESCALER_RELOAD = 341;  // ~1 scanline in CPU cycles

    // Audio silence control
    bool m_audio_silence = false;

    // ========================================
    // VRC7 FM Synthesis (YM2413/OPLL subset)
    // ========================================

    // FM register address latch
    uint8_t m_fm_address = 0;

    // Custom instrument registers ($00-$07)
    uint8_t m_custom_instrument[8] = {0};

    // Channel F-Number low registers ($10-$15)
    uint8_t m_fnum_low[6] = {0};

    // Channel trigger/sustain/block/F-Number high ($20-$25)
    // Bits: [x][sustain][key on][block 2:0][fnum 8]
    uint8_t m_channel_ctrl[6] = {0};

    // Channel instrument/volume ($30-$35)
    // Bits: [instrument 3:0][volume 3:0]
    uint8_t m_channel_vol[6] = {0};

    // ========================================
    // FM Synthesis State (per channel)
    // ========================================
    static constexpr int FM_CHANNELS = 6;

    // Phase accumulators for modulator and carrier (18-bit)
    uint32_t m_phase_mod[FM_CHANNELS] = {0};
    uint32_t m_phase_car[FM_CHANNELS] = {0};

    // Envelope state
    enum class EnvState { Attack, Decay, Sustain, Release, Off };
    EnvState m_env_state_mod[FM_CHANNELS] = {EnvState::Off};
    EnvState m_env_state_car[FM_CHANNELS] = {EnvState::Off};

    // Envelope levels (0-127, where 0 = max volume, 127 = silence)
    // Using 10-bit internal representation for smoother envelopes
    uint16_t m_env_level_mod[FM_CHANNELS] = {0x3FF};  // Start at max attenuation
    uint16_t m_env_level_car[FM_CHANNELS] = {0x3FF};

    // Envelope rate counters
    uint32_t m_env_counter_mod[FM_CHANNELS] = {0};
    uint32_t m_env_counter_car[FM_CHANNELS] = {0};

    // Previous key on state (for edge detection)
    bool m_prev_key_on[FM_CHANNELS] = {false};

    // Feedback state for modulator self-feedback
    int16_t m_feedback_mod[FM_CHANNELS][2] = {{0}};  // 2-sample history

    // FM clock divider (VRC7 runs at 3.58MHz / 72 = ~49716 Hz)
    uint32_t m_fm_clock_counter = 0;
    static constexpr uint32_t FM_CLOCK_DIVIDER = 36;  // CPU cycles per FM sample

    // ========================================
    // Lookup Tables
    // ========================================

    // Sine table (half sine for OPLL - 10 bit, 256 entries)
    // Stored as log-sin for easier multiplication
    static const uint16_t s_log_sin_table[256];

    // Exponential table for dB to linear conversion
    static const uint16_t s_exp_table[256];

    // Multiplier table (0-15 -> actual multiplier)
    static const uint8_t s_multiplier_table[16];

    // Key scale level table (octave/note -> attenuation)
    static const uint8_t s_ksl_table[8][16];

    // Attack rate table
    static const uint8_t s_attack_table[64];

    // Decay rate table
    static const uint8_t s_decay_table[64];

    // ========================================
    // Built-in VRC7 Instrument Patches
    // ========================================
    // Each patch is 8 bytes:
    // [0] Modulator: AM/VIB/EG/KSR/MULT
    // [1] Carrier: AM/VIB/EG/KSR/MULT
    // [2] Modulator: KSL/TL (Total Level)
    // [3] Carrier: KSL/WF + Mod: WF/FB
    // [4] Modulator: AR/DR
    // [5] Carrier: AR/DR
    // [6] Modulator: SL/RR
    // [7] Carrier: SL/RR
    static const uint8_t s_instrument_patches[15][8];

    // Get instrument data (0 = custom, 1-15 = preset)
    const uint8_t* get_instrument(int patch) const;
};

} // namespace nes
