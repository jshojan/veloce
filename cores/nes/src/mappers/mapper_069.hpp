#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 069: Sunsoft FME-7/5A/5B
// Used by: Batman (Sunsoft), Gimmick!, Hebereke, Barcode World
//
// Features:
// - 8KB switchable PRG ROM banks
// - 1KB switchable CHR ROM/RAM banks
// - IRQ counter with CPU cycle counting
// - Optional expansion audio (5B variant)
// - PRG RAM with optional battery backup
class Mapper069 : public Mapper {
public:
    Mapper069(std::vector<uint8_t>& prg_rom,
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

    void reset() override;

    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

    // CPU cycle notification for IRQ counter and audio
    // PERFORMANCE: Batched version - receives cycle count for efficient processing
    void cpu_cycles(int count) override;
    void cpu_cycle() override;

    // Get expansion audio output (-1.0 to 1.0)
    float get_audio_output() const override { return m_audio_output; }

private:
    // Audio synthesis
    void clock_audio();
    void update_prg_banks();
    void update_chr_banks();
    void write_register(uint8_t value);

    // Command register
    uint8_t m_command = 0;

    // PRG banking
    uint8_t m_prg_bank[4] = {0};           // 4 x 8KB PRG banks
    uint32_t m_prg_bank_offset[4] = {0};

    // PRG RAM control
    bool m_prg_ram_enabled = false;
    bool m_prg_ram_select = false;         // false = ROM, true = RAM at $6000-$7FFF
    uint8_t m_prg_ram_bank = 0;

    // CHR banking
    uint8_t m_chr_bank[8] = {0};           // 8 x 1KB CHR banks
    uint32_t m_chr_bank_offset[8] = {0};

    // IRQ
    bool m_irq_enabled = false;
    bool m_irq_counter_enabled = false;
    bool m_irq_pending = false;
    uint16_t m_irq_counter = 0;

    // Audio expansion (5B variant - AY-3-8910 compatible)
    uint8_t m_audio_command = 0;
    uint8_t m_audio_registers[16] = {0};

    // Audio channel state (3 square wave channels)
    struct SunsoftChannel {
        uint16_t period = 0;       // 12-bit period from registers
        uint16_t timer = 0;        // Current timer countdown
        uint8_t volume = 0;        // 4-bit volume (0-15)
        bool tone_enabled = true;  // Tone output enabled
        bool noise_enabled = false;// Noise output enabled
        bool output_high = false;  // Current output state
    };
    SunsoftChannel m_ss_channels[3];

    // Noise generator
    uint16_t m_noise_period = 0;
    uint16_t m_noise_timer = 0;
    uint32_t m_noise_shift = 1;  // 17-bit LFSR
    bool m_noise_output = false;

    // Envelope generator
    uint16_t m_env_period = 0;
    uint16_t m_env_timer = 0;
    uint8_t m_env_shape = 0;
    uint8_t m_env_volume = 0;
    bool m_env_holding = false;
    bool m_env_attack = false;
    bool m_env_alternate = false;
    bool m_env_hold = false;

    // Audio timing (divides CPU clock by 16)
    uint8_t m_audio_divider = 0;

    // Audio output
    float m_audio_output = 0.0f;
};

} // namespace nes
