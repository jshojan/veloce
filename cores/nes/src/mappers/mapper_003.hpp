#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 3: CNROM
// - PRG ROM: 16KB or 32KB (fixed, no banking)
// - CHR ROM: 8KB-32KB, switchable 8KB banks
// Games: Arkanoid, Gradius, Solomon's Key, Galaxian
class Mapper003 : public Mapper {
public:
    Mapper003(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram);

    void reset() override;

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

private:
    uint8_t m_chr_bank = 0;
    uint32_t m_chr_bank_offset = 0;
};

} // namespace nes
