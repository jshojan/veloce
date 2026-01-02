#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 024: Konami VRC6a
// Mapper 026: Konami VRC6b (address lines swapped)
// Used by: Akumajou Densetsu (Castlevania 3 Japan), Madara, Esper Dream 2
//
// Features:
// - 16KB/8KB switchable PRG ROM banks
// - 1KB switchable CHR ROM banks
// - Scanline IRQ counter
// - VRC6 expansion audio (2 pulse + 1 sawtooth)
class Mapper024 : public Mapper {
public:
    Mapper024(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram,
              bool is_vrc6b = false);  // true for mapper 026

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

    // CPU cycle notification for IRQ counter (cycle mode) and audio clocking
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
    uint16_t translate_address(uint16_t address);

    bool m_is_vrc6b;  // Address line configuration

    // PRG banking
    uint8_t m_prg_bank_16k = 0;    // $8000-$BFFF (16KB)
    uint8_t m_prg_bank_8k = 0;     // $C000-$DFFF (8KB)
    uint32_t m_prg_bank_16k_offset = 0;
    uint32_t m_prg_bank_8k_offset = 0;
    uint32_t m_prg_fixed_offset = 0;  // $E000-$FFFF (fixed to last 8KB)

    // CHR banking
    uint8_t m_chr_bank[8] = {0};       // 8 x 1KB CHR banks
    uint32_t m_chr_bank_offset[8] = {0};

    // IRQ
    uint8_t m_irq_latch = 0;
    uint8_t m_irq_counter = 0;
    bool m_irq_enabled = false;
    bool m_irq_enabled_after_ack = false;
    bool m_irq_pending = false;
    bool m_irq_mode_cycle = false;  // false = scanline mode, true = cycle mode
    uint16_t m_irq_prescaler = 0;
    static constexpr uint16_t IRQ_PRESCALER_RELOAD = 341;  // ~1 scanline in CPU cycles

    // VRC6 Audio registers
    uint8_t m_pulse1_regs[4] = {0};
    uint8_t m_pulse2_regs[4] = {0};
    uint8_t m_saw_regs[3] = {0};
    uint8_t m_audio_halt = 0;

    // VRC6 Pulse channel state
    struct VRC6Pulse {
        uint8_t duty = 0;         // 3-bit duty cycle (0-7, 8 = constant high)
        uint8_t volume = 0;       // 4-bit volume
        uint16_t period = 0;      // 12-bit period
        uint16_t timer = 0;       // Current timer countdown
        uint8_t sequence_pos = 0; // Current position in duty cycle (0-15)
        bool enabled = true;
    };
    VRC6Pulse m_vrc6_pulse[2];

    // VRC6 Sawtooth channel state
    struct VRC6Saw {
        uint8_t rate = 0;         // Accumulator rate (0-63)
        uint16_t period = 0;      // 12-bit period
        uint16_t timer = 0;       // Current timer countdown
        uint8_t accumulator = 0;  // 8-bit accumulator
        uint8_t step = 0;         // Current step (0-13, resets at 14)
        bool enabled = true;
    };
    VRC6Saw m_vrc6_saw;

    // Audio output
    float m_audio_output = 0.0f;

    // Audio divider - VRC6 audio runs at CPU/1 but we only need to
    // compute output periodically for performance
    uint8_t m_audio_divider = 0;
    static constexpr uint8_t AUDIO_DIVIDER_PERIOD = 16;  // Update audio every 16 CPU cycles
};

// Mapper026 is just Mapper024 with address lines swapped
class Mapper026 : public Mapper024 {
public:
    Mapper026(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram)
        : Mapper024(prg_rom, chr_rom, prg_ram, mirror, has_chr_ram, true) {}
};

} // namespace nes
