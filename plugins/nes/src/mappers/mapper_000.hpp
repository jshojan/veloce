#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 0: NROM
// - PRG ROM: 16KB or 32KB (mirrored if 16KB)
// - CHR ROM: 8KB
// - No banking, simplest mapper
class Mapper000 : public Mapper {
public:
    Mapper000(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

private:
    bool m_prg_16k;  // True if only 16KB PRG ROM (needs mirroring)
};

} // namespace nes
