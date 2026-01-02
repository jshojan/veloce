#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 079: NINA-03/NINA-06
// Used by: Some AVE (American Video Entertainment) games
// Features:
// - 32KB switchable PRG bank at $8000-$FFFF
// - 8KB switchable CHR bank at $0000-$1FFF
// - Register at $4100-$5FFF (directly connected to address bus)
class Mapper079 : public Mapper {
public:
    Mapper079(std::vector<uint8_t>& prg_rom,
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
    uint8_t m_prg_bank = 0;
    uint8_t m_chr_bank = 0;
    uint32_t m_prg_bank_offset = 0;
    uint32_t m_chr_bank_offset = 0;
};

} // namespace nes
