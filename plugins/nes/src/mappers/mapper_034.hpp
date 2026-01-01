#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 034: BNROM / NINA-001
// Used by: Deadly Towers, Impossible Mission II, and some AVE games
// Two variants:
// - BNROM: 32KB PRG switching, no CHR banking
// - NINA-001: 32KB PRG switching, 4KB CHR banking
// We implement both based on ROM characteristics
class Mapper034 : public Mapper {
public:
    Mapper034(std::vector<uint8_t>& prg_rom,
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
    bool m_is_nina001 = false;  // True for NINA-001, false for BNROM

    uint8_t m_prg_bank = 0;
    uint8_t m_chr_bank_0 = 0;
    uint8_t m_chr_bank_1 = 0;
    uint32_t m_prg_bank_offset = 0;
    uint32_t m_chr_bank_0_offset = 0;
    uint32_t m_chr_bank_1_offset = 0;
};

} // namespace nes
