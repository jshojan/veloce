#pragma once

#include "mapper.hpp"
#include <array>

namespace nes {

// Mapper 5: MMC5 (Nintendo MMC5)
// Used by: Castlevania III, Just Breed, Metal Slader Glory, and others
// One of the most complex NES mappers with many unique features:
// - Complex PRG/CHR banking with multiple modes
// - 1KB ExRAM for extended attributes or as extra nametable
// - Scanline counter/IRQ
// - 8x8 -> 16-bit hardware multiplier
// - Fill mode for nametables
// - Flexible nametable mapping
// - Split screen capability
// - Audio expansion (pulse channels - stubbed for now)
class Mapper005 : public Mapper {
public:
    Mapper005(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override;

    bool irq_pending(uint32_t frame_cycle = 0) override;
    void irq_clear() override;
    void scanline() override;
    void notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr, uint32_t frame_cycle) override;
    void notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle) override;
    void notify_frame_start() override;

    void reset() override;
    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

    // CPU cycle notification for audio
    // PERFORMANCE: Batched version - receives cycle count for efficient processing
    void cpu_cycles(int count) override;
    void cpu_cycle() override;

    // Get expansion audio output (-1.0 to 1.0)
    float get_audio_output() const override { return m_audio_output; }

private:
    // Audio processing helper
    void process_audio_batch();

    // PRG banking helpers
    uint32_t get_prg_bank_offset(int bank, bool is_ram) const;
    uint8_t read_prg(uint16_t address);
    void write_prg(uint16_t address, uint8_t value);

    // CHR banking helpers
    uint32_t get_chr_bank_offset(int bank, bool for_sprites) const;

    // Nametable helpers
    uint8_t read_nametable(uint16_t address);
    void write_nametable(uint16_t address, uint8_t value);
    uint8_t get_exram_attribute(uint16_t address);

    // Scanline detection
    void detect_scanline(uint16_t address, uint32_t frame_cycle);

    // ========== Registers ==========

    // $5100: PRG mode (0-3)
    // Mode 0: One 32KB bank at $8000
    // Mode 1: Two 16KB banks at $8000 and $C000
    // Mode 2: One 16KB bank at $8000, two 8KB at $C000 and $E000
    // Mode 3: Four 8KB banks
    uint8_t m_prg_mode = 3;

    // $5101: CHR mode (0-3)
    // Mode 0: 8KB banks
    // Mode 1: 4KB banks
    // Mode 2: 2KB banks
    // Mode 3: 1KB banks
    uint8_t m_chr_mode = 3;

    // $5102-$5103: PRG RAM protect
    // Both must have specific values for RAM to be writable
    uint8_t m_prg_ram_protect1 = 0;  // Must be 0x02 for writes
    uint8_t m_prg_ram_protect2 = 0;  // Must be 0x01 for writes

    // $5104: ExRAM mode
    // 0: As extra nametable
    // 1: Extended attribute mode
    // 2: CPU read/write mode
    // 3: CPU read-only mode
    uint8_t m_exram_mode = 0;

    // $5105: Nametable mapping
    // Each 2-bit field selects source for each nametable:
    // 0: CIRAM page 0 (internal VRAM $2000)
    // 1: CIRAM page 1 (internal VRAM $2400)
    // 2: ExRAM as nametable
    // 3: Fill-mode (use fill tile/attribute)
    uint8_t m_nametable_mapping = 0;

    // $5106: Fill tile
    uint8_t m_fill_tile = 0;

    // $5107: Fill attribute (bits 0-1)
    uint8_t m_fill_attribute = 0;

    // $5113: PRG RAM bank (for $6000-$7FFF)
    uint8_t m_prg_ram_bank = 0;

    // $5114-$5117: PRG bank registers
    // Each register selects a PRG bank, with bit 7 indicating RAM vs ROM
    std::array<uint8_t, 4> m_prg_banks = {0, 0, 0, 0xFF};

    // $5120-$5127: CHR bank registers (sprite mode, or both when $5130 last written)
    std::array<uint16_t, 8> m_chr_banks_sprite = {0};

    // $5128-$512B: CHR bank registers (background mode)
    std::array<uint16_t, 4> m_chr_banks_bg = {0};

    // $5130: Upper CHR bank bits (used as high bits for all CHR banks)
    uint8_t m_chr_upper_bits = 0;

    // Track which CHR bank set was last written to
    // false = sprite banks ($5120-$5127), true = BG banks ($5128-$512B)
    bool m_last_chr_write_was_bg = false;

    // $5200: Split mode control
    // Bit 7: Enable split
    // Bit 6: Right side split (0 = left, 1 = right)
    // Bits 4-0: Split tile position (0-31)
    uint8_t m_split_mode = 0;

    // $5201: Split Y scroll
    uint8_t m_split_scroll = 0;

    // $5202: Split CHR bank
    uint8_t m_split_bank = 0;

    // $5203: IRQ scanline compare value
    uint8_t m_irq_scanline = 0;

    // $5204: IRQ enable/status
    // Bit 7: Enable IRQ
    // Read: Bit 7 = in-frame flag, Bit 6 = IRQ pending
    bool m_irq_enabled = false;
    bool m_irq_pending = false;
    bool m_in_frame = false;

    // $5205-$5206: Multiplier
    uint8_t m_multiplicand = 0;  // $5205 (write)
    uint8_t m_multiplier = 0;    // $5206 (write)
    // Result is read back from $5205 (lo) and $5206 (hi)

    // ========== Internal State ==========

    // ExRAM (1KB)
    std::array<uint8_t, 1024> m_exram = {0};

    // Current scanline counter (0-255, wraps)
    uint8_t m_scanline_counter = 0;

    // For in-frame detection
    uint16_t m_last_ppu_addr = 0;
    int m_consecutive_nametable_reads = 0;
    uint32_t m_last_frame_cycle = 0;

    // Track rendering state for sprite/BG CHR bank selection
    // The PPU fetches BG tiles first (cycles 1-256, 321-336), then sprites (257-320)
    // We detect this by watching the PPU address bus
    bool m_fetching_sprites = false;
    uint16_t m_current_tile_addr = 0;

    // For extended attribute mode - track current tile fetch
    uint8_t m_exram_attr_latch = 0;

    // Split screen state
    bool m_in_split_region = false;
    uint8_t m_split_tile_count = 0;

    // ========== MMC5 Audio ==========
    // MMC5 has two pulse channels similar to the NES APU pulse channels
    // but with additional features

    struct MMC5Pulse {
        bool enabled = false;
        uint8_t duty = 0;        // 2-bit duty cycle
        bool length_halt = false; // Also envelope loop
        bool constant_volume = false;
        uint8_t volume = 0;      // 4-bit volume/envelope
        uint16_t timer_period = 0;
        uint16_t timer = 0;
        uint8_t sequence_pos = 0;
        uint8_t length_counter = 0;
        uint8_t envelope_counter = 0;
        uint8_t envelope_divider = 0;
        bool envelope_start = false;
    };
    MMC5Pulse m_mmc5_pulse[2];

    // PCM channel (raw 8-bit sample output)
    uint8_t m_pcm_output = 0;
    bool m_pcm_irq_enabled = false;
    bool m_pcm_read_mode = false;

    // Audio output
    float m_audio_output = 0.0f;

    // Audio clocking - use divider for performance
    uint32_t m_audio_cycles = 0;
    uint8_t m_audio_divider = 0;
    static constexpr uint8_t AUDIO_DIVIDER_PERIOD = 16;  // Update audio every 16 CPU cycles
};

} // namespace nes
