#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 2: UxROM (UNROM/UOROM)
// - PRG ROM: 128KB-256KB, switchable 16KB banks at $8000
// - CHR RAM: 8KB
// - Fixed last bank at $C000
// Games: Mega Man, Castlevania, Contra, Duck Tales, Metal Gear
class Mapper002 : public Mapper {
public:
    Mapper002(std::vector<uint8_t>& prg_rom,
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
    uint8_t m_prg_bank = 0;
    uint32_t m_prg_bank_offset = 0;
    uint32_t m_last_bank_offset = 0;
};

} // namespace nes
