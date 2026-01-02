#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 1: MMC1 (Nintendo MMC1)
// - PRG ROM: Up to 256KB (16 x 16KB banks)
// - PRG RAM: Up to 32KB (battery backed)
// - CHR ROM/RAM: Up to 128KB (can be RAM)
// - Switchable mirroring
// Games: Zelda, Metroid, Final Fantasy, many more (~25% of NES library)
class Mapper001 : public Mapper {
public:
    Mapper001(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

    void reset() override;
    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

private:
    void write_register(uint16_t address, uint8_t value);
    void update_banks();

    // Shift register for serial write
    uint8_t m_shift_register = 0x10;
    int m_shift_count = 0;

    // Control register ($8000-$9FFF)
    uint8_t m_control = 0x0C;  // Default: PRG fixed $C000, CHR 8KB mode

    // CHR bank registers ($A000-$BFFF, $C000-$DFFF)
    uint8_t m_chr_bank_0 = 0;
    uint8_t m_chr_bank_1 = 0;

    // PRG bank register ($E000-$FFFF)
    uint8_t m_prg_bank = 0;

    // Computed bank offsets
    uint32_t m_prg_bank_0_offset = 0;
    uint32_t m_prg_bank_1_offset = 0;
    uint32_t m_chr_bank_0_offset = 0;
    uint32_t m_chr_bank_1_offset = 0;
};

} // namespace nes
