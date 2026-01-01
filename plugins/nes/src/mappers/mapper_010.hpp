#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 010: MMC4 (FxROM)
// Used by: Fire Emblem, Fire Emblem Gaiden (Japan)
// Features:
// - 16KB switchable PRG bank at $8000-$BFFF
// - 16KB fixed PRG at $C000-$FFFF (last bank)
// - Same CHR latching as MMC2 (tiles $FD/$FE)
// - Mirroring control
class Mapper010 : public Mapper {
public:
    Mapper010(std::vector<uint8_t>& prg_rom,
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
    void update_chr_banks();

    // PRG banking - 16KB switchable + 16KB fixed
    uint8_t m_prg_bank = 0;
    uint32_t m_prg_bank_offset = 0;
    uint32_t m_prg_fixed_offset = 0;

    // CHR banking - same as MMC2
    uint8_t m_chr_bank_0_fd = 0;
    uint8_t m_chr_bank_0_fe = 0;
    uint8_t m_chr_bank_1_fd = 0;
    uint8_t m_chr_bank_1_fe = 0;

    bool m_latch_0 = false;
    bool m_latch_1 = false;

    uint32_t m_chr_bank_0_offset = 0;
    uint32_t m_chr_bank_1_offset = 0;
};

} // namespace nes
