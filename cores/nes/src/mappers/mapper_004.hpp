#pragma once

#include "mapper.hpp"

namespace nes {

// Mapper 4: MMC3 (Nintendo MMC3)
// Used by: Super Mario Bros. 3, Mega Man 3-6, many others
// - PRG ROM: Up to 512KB (8KB switchable banks)
// - CHR ROM/RAM: Up to 256KB (1KB/2KB switchable banks)
// - Scanline counter for IRQ
// - Switchable mirroring
class Mapper004 : public Mapper {
public:
    Mapper004(std::vector<uint8_t>& prg_rom,
              std::vector<uint8_t>& chr_rom,
              std::vector<uint8_t>& prg_ram,
              MirrorMode mirror,
              bool has_chr_ram);

    uint8_t cpu_read(uint16_t address) override;
    void cpu_write(uint16_t address, uint8_t value) override;

    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) override;
    void ppu_write(uint16_t address, uint8_t value) override;

    MirrorMode get_mirror_mode() const override { return m_mirror_mode; }

    bool irq_pending(uint32_t frame_cycle = 0) override;
    void irq_clear() override { m_irq_pending = false; m_irq_pending_at_cycle = 0; }
    void scanline() override;
    void notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr, uint32_t frame_cycle) override;
    void notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle) override;
    void notify_frame_start() override;

    void reset() override;
    void save_state(std::vector<uint8_t>& data) override;
    void load_state(const uint8_t*& data, size_t& remaining) override;

private:
    void update_banks();
    void clock_counter_on_a12_fast(bool a12, uint32_t frame_cycle);

    // Bank select register
    uint8_t m_bank_select = 0;
    bool m_prg_mode = false;    // PRG ROM bank mode
    bool m_chr_mode = false;    // CHR ROM bank mode

    // Bank registers R0-R7
    uint8_t m_registers[8] = {0};

    // PRG bank offsets (in bytes)
    uint32_t m_prg_bank[4] = {0};

    // CHR bank offsets (in bytes)
    uint32_t m_chr_bank[8] = {0};

    // IRQ counter
    uint8_t m_irq_counter = 0;
    uint8_t m_irq_latch = 0;
    bool m_irq_enabled = false;
    bool m_irq_pending = false;
    bool m_irq_reload = false;

    // IRQ delay: accounts for CPU/PPU synchronization timing
    // MMC3 IRQ signal propagation is nearly immediate on real hardware
    // but there's a small delay before the CPU sees it.
    // This delay is in PPU cycles.
    static constexpr uint32_t IRQ_DELAY_CYCLES = 0;
    uint32_t m_irq_pending_at_cycle = 0;  // Frame cycle when IRQ was triggered

    // A12 tracking for scanline detection
    bool m_last_a12 = false;
    uint32_t m_last_a12_cycle = 0;  // PPU cycle when A12 last changed
    uint32_t m_current_frame_cycle = 0;  // Current PPU frame cycle for IRQ delay tracking

    // Debug counter for A12 clocks
    int m_debug_clock_count = 0;
};

} // namespace nes
