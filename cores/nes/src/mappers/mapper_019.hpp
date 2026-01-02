#pragma once

#include "mapper.hpp"
#include <array>

namespace nes {

// Mapper 019: Namco 163 (also known as Namco 129/163)
// Used by: Megami Tensei II, Rolling Thunder, Dragon Ninja, Splatterhouse,
//          Famista series, King of Kings, Sangokushi II, Battle Fleet,
//          Erika to Satoru no Yume Bouken, Final Lap, Mappy Kids, Youkai Douchuuki
//
// Features:
// - PRG ROM: Up to 512KB (8KB switchable banks)
// - PRG RAM: 8KB at $6000-$7FFF with optional battery backup
// - CHR ROM: Up to 256KB (1KB switchable banks)
// - Flexible nametable control (CHR banks $E0-$FF use CIRAM)
// - 15-bit IRQ counter (counts up, fires at $8000)
// - 128 bytes internal RAM shared between waveform data and audio registers
// - Expansion audio: Up to 8 wavetable synthesis channels
class Mapper019 : public Mapper {
public:
    Mapper019(std::vector<uint8_t>& prg_rom,
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

    // CPU cycle notification for IRQ counter and audio synthesis
    // PERFORMANCE: Batched version - receives cycle count for efficient processing
    void cpu_cycles(int count) override;
    void cpu_cycle() override;

    // Get audio output sample (-1.0 to 1.0)
    float get_audio_output() const override { return m_audio_output; }

private:
    void update_prg_banks();
    void update_chr_banks();
    void clock_audio();
    uint8_t read_internal_ram(uint8_t addr);
    void write_internal_ram(uint8_t addr, uint8_t value);

    // PRG banking
    uint8_t m_prg_bank[3] = {0};           // 3 x 8KB switchable banks ($8000-$DFFF)
    uint32_t m_prg_bank_offset[4] = {0};   // 4 x 8KB bank offsets (includes fixed $E000)
    bool m_prg_ram_write_protect = false;

    // CHR banking - 8 x 1KB banks
    // Banks $E0-$FF map to CIRAM (nametable RAM) instead of CHR ROM
    uint8_t m_chr_bank[8] = {0};
    uint32_t m_chr_bank_offset[8] = {0};

    // Nametable banking - 4 nametable banks (for $2000-$2FFF)
    // Can select CHR ROM or CIRAM
    uint8_t m_nt_bank[4] = {0};

    // IRQ counter
    // Namco 163 uses a 15-bit counter that counts UP
    // IRQ fires when bit 15 becomes set (counter >= $8000)
    uint16_t m_irq_counter = 0;
    bool m_irq_enabled = false;
    bool m_irq_pending = false;

    // Internal RAM (128 bytes)
    // $00-$3F: Waveform data (4-bit samples, 2 per byte)
    // $40-$7F: Channel registers (8 bytes per channel x 8 channels)
    std::array<uint8_t, 128> m_internal_ram = {};

    // RAM address register
    uint8_t m_ram_addr = 0;
    bool m_ram_auto_increment = false;

    // Sound enable flag
    bool m_sound_enabled = false;

    // Active channel count (1-8, derived from channel 7's frequency high byte)
    uint8_t m_active_channels = 1;

    // Audio synthesis state
    // Each channel has:
    // - 18-bit frequency
    // - 24-bit phase accumulator
    // - 4-bit waveform length (determines wave size: (256 - length*4) samples)
    // - Waveform offset in RAM
    // - 4-bit volume
    struct AudioChannel {
        uint32_t frequency = 0;    // 18-bit frequency
        uint32_t phase = 0;        // 24-bit phase accumulator
        uint8_t wave_length = 0;   // 4-bit (0-63, wave samples = 256 - length*4)
        uint8_t wave_offset = 0;   // Starting offset in RAM for waveform
        uint8_t volume = 0;        // 4-bit volume
    };
    std::array<AudioChannel, 8> m_channels = {};

    // Audio output
    float m_audio_output = 0.0f;

    // Audio timing
    // Channels are clocked sequentially at CPU_CLOCK / 15
    // We use an outer divider for performance
    uint8_t m_audio_cycle = 0;
    uint8_t m_current_channel = 0;
    uint8_t m_audio_divider = 0;
    static constexpr uint8_t AUDIO_DIVIDER_PERIOD = 15;  // Match internal clock divider

    // CIRAM (internal nametable RAM) access
    // The PPU provides 2KB, but Namco 163 can use CHR banks to address it
    // Pointers to the bus's CIRAM would be needed, but we handle this
    // by returning special values and letting the bus handle CIRAM access
};

} // namespace nes
