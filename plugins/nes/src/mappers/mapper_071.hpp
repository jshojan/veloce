#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 071: Camerica/Codemasters
// Used by: Micro Machines, Fire Hawk, Quattro games, Bee 52
// Features:
// - 16KB switchable PRG bank at $8000-$BFFF
// - 16KB fixed PRG at $C000-$FFFF (last bank)
// - 8KB CHR RAM at $0000-$1FFF
// - Optional single-screen mirroring control (some boards)
class Mapper071 : public Mapper {
public:
    Mapper071(std::vector<uint8_t>& prg_rom,
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
    uint32_t m_prg_bank_offset = 0;
    uint32_t m_prg_fixed_offset = 0;
    bool m_has_mirroring_control = false;  // Some boards have this
};

} // namespace nes
