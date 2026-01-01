#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 206: DxROM / Namcot 118 / MIMIC-1
// Used by: Babel no Tou, Dragon Buster, Famista series (Japan)
// Features:
// - Subset of MMC3 functionality (no IRQ, no PRG RAM protection)
// - 8KB switchable PRG banks
// - 2KB/1KB CHR banks (like MMC3)
// - Fixed mirroring (no mirroring control)
class Mapper206 : public Mapper {
public:
    Mapper206(std::vector<uint8_t>& prg_rom,
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
    void update_prg_banks();
    void update_chr_banks();

    // Bank select register
    uint8_t m_bank_select = 0;

    // Bank registers (R0-R7)
    uint8_t m_registers[8] = {0};

    // PRG bank offsets
    uint32_t m_prg_bank_offsets[4] = {0};

    // CHR bank offsets
    uint32_t m_chr_bank_offsets[8] = {0};
};

} // namespace nes
