#pragma once

#include "mapper.hpp"

namespace nes {

// VRC2/VRC4 Mapper variants
// Mapper 21: VRC4a, VRC4c
// Mapper 22: VRC2a
// Mapper 23: VRC2b, VRC4e, VRC4f
// Mapper 25: VRC2c, VRC4b, VRC4d
//
// Used by: Contra, Gradius II, Wai Wai World, Ganbare Goemon, etc.
//
// Features:
// - 8KB switchable PRG ROM banks
// - 1KB switchable CHR ROM banks
// - Scanline IRQ counter (VRC4 only)
// - Mirroring control
class MapperVRC : public Mapper {
public:
    enum class Variant {
        VRC2a,  // Mapper 22
        VRC2b,  // Mapper 23
        VRC2c,  // Mapper 25
        VRC4a,  // Mapper 21
        VRC4b,  // Mapper 25
        VRC4c,  // Mapper 21
        VRC4d,  // Mapper 25
        VRC4e,  // Mapper 23
        VRC4f   // Mapper 23
    };

    MapperVRC(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram,
              Variant variant);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

    bool irq_pending(uint32_t frame_cycle = 0) override { return m_irq_pending; }
    void irq_clear() override { m_irq_pending = false; }

    void scanline() override;

    void reset() override;

    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

    // CPU cycle notification for IRQ counter (cycle mode)
    // PERFORMANCE: Batched version - receives cycle count for efficient processing
    void cpu_cycles(int count) override;
    void cpu_cycle() override;

private:
    void update_prg_banks();
    void update_chr_banks();
    uint16_t translate_address(uint16_t address);
    void write_chr_bank(int bank, uint8_t value, bool high_nibble);

    Variant m_variant;
    bool m_is_vrc4;  // true for VRC4 (has IRQ), false for VRC2

    // PRG banking
    uint8_t m_prg_bank_0 = 0;  // $8000 or $C000 (swappable)
    uint8_t m_prg_bank_1 = 0;  // $A000
    bool m_prg_swap_mode = false;  // false = $8000 swappable, true = $C000 swappable
    uint32_t m_prg_bank_offset[4] = {0};

    // CHR banking (stored as low/high nibbles for VRC4)
    uint8_t m_chr_bank_lo[8] = {0};
    uint8_t m_chr_bank_hi[8] = {0};
    uint32_t m_chr_bank_offset[8] = {0};

    // IRQ (VRC4 only)
    uint8_t m_irq_latch = 0;
    uint8_t m_irq_counter = 0;
    bool m_irq_enabled = false;
    bool m_irq_enabled_after_ack = false;
    bool m_irq_pending = false;
    bool m_irq_mode_cycle = false;
    uint16_t m_irq_prescaler = 0;
    static constexpr int IRQ_PRESCALER_RELOAD = 341;
};

// Wrapper classes for specific mapper numbers
class Mapper021 : public MapperVRC {
public:
    Mapper021(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram)
        : MapperVRC(prg_rom, chr_rom, prg_ram, mirror, has_chr_ram, Variant::VRC4a) {}
};

class Mapper022 : public MapperVRC {
public:
    Mapper022(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram)
        : MapperVRC(prg_rom, chr_rom, prg_ram, mirror, has_chr_ram, Variant::VRC2a) {}
};

class Mapper023 : public MapperVRC {
public:
    Mapper023(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram)
        : MapperVRC(prg_rom, chr_rom, prg_ram, mirror, has_chr_ram, Variant::VRC4e) {}
};

class Mapper025 : public MapperVRC {
public:
    Mapper025(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram)
        : MapperVRC(prg_rom, chr_rom, prg_ram, mirror, has_chr_ram, Variant::VRC4b) {}
};

} // namespace nes
