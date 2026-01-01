#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 009: MMC2 (PxROM)
// Used by: Punch-Out!!, Mike Tyson's Punch-Out!!
// Features:
// - 8KB switchable PRG bank at $8000-$9FFF
// - 24KB fixed PRG at $A000-$FFFF (last 3 banks)
// - Two CHR bank registers per 4KB area, latched by reading tiles $FD/$FE
// - Mirroring control
class Mapper009 : public Mapper {
public:
    Mapper009(std::vector<uint8_t>& prg_rom,
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

    // PRG banking
    uint8_t m_prg_bank = 0;
    uint32_t m_prg_bank_offset = 0;
    uint32_t m_prg_fixed_offset = 0;  // Fixed to last 3 banks

    // CHR banking - two registers per 4KB, selected by latch
    uint8_t m_chr_bank_0_fd = 0;  // $0000-$0FFF when latch0 = $FD
    uint8_t m_chr_bank_0_fe = 0;  // $0000-$0FFF when latch0 = $FE
    uint8_t m_chr_bank_1_fd = 0;  // $1000-$1FFF when latch1 = $FD
    uint8_t m_chr_bank_1_fe = 0;  // $1000-$1FFF when latch1 = $FE

    // Latches - set when reading specific tiles from pattern tables
    bool m_latch_0 = false;  // false=$FD, true=$FE
    bool m_latch_1 = false;

    // Cached bank offsets
    uint32_t m_chr_bank_0_offset = 0;
    uint32_t m_chr_bank_1_offset = 0;
};

} // namespace nes
