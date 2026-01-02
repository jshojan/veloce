#pragma once

#include <cstdint>
#include <vector>

namespace nes {

// Mirror modes for nametables
enum class MirrorMode {
    Horizontal,
    Vertical,
    SingleScreen0,
    SingleScreen1,
    FourScreen
};

// Base class for cartridge mappers
class Mapper {
public:
    virtual ~Mapper() = default;

    // CPU memory access ($4020-$FFFF)
    virtual uint8_t cpu_read(uint16_t address) = 0;
    virtual void cpu_write(uint16_t address, uint8_t value) = 0;

    // PPU memory access ($0000-$1FFF)
    // The frame_cycle parameter is (scanline * 341 + cycle) for A12 timing
    virtual uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0) = 0;
    virtual void ppu_write(uint16_t address, uint8_t value) = 0;

    // Get current mirror mode
    virtual MirrorMode get_mirror_mode() const = 0;

    // IRQ support (some mappers generate IRQs)
    // frame_cycle is (scanline * 341 + cycle) for delayed IRQ timing
    virtual bool irq_pending(uint32_t frame_cycle = 0) { return false; }
    virtual void irq_clear() {}

    // Scanline counter (for MMC3 and similar)
    virtual void scanline() {}

    // PPU address change notification (for MMC3 A12 clocking from PPUADDR writes)
    // frame_cycle is (scanline * 341 + cycle) for proper A12 filter timing
    virtual void notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr, uint32_t frame_cycle) {}

    // PPU address bus notification (for MMC3 A12 clocking during rendering)
    // Called for ALL PPU address bus activity including nametable/attribute fetches
    virtual void notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle) {}

    // Frame start notification (called when scanline resets to 0)
    // Used by mappers like MMC3 to reset frame-relative timing state
    virtual void notify_frame_start() {}

    // CPU cycle notification (for mappers with IRQ counters or expansion audio)
    // PERFORMANCE: Receives cycle COUNT to allow batched processing instead of
    // being called once per cycle. Mappers should process all cycles at once.
    virtual void cpu_cycles(int count) { (void)count; }

    // Legacy single-cycle interface (deprecated, kept for compatibility)
    virtual void cpu_cycle() {}

    // Get expansion audio output (-1.0 to 1.0) for mappers with audio chips
    virtual float get_audio_output() const { return 0.0f; }

    // Reset mapper state
    virtual void reset() {}

    // Save state
    virtual void save_state(std::vector<uint8_t>& data) {}
    virtual void load_state(const uint8_t*& data, size_t& remaining) {}

    // Battery-backed save data (for mappers with EEPROM or other save mechanisms)
    // These are separate from PRG RAM which is handled by Cartridge directly.
    // Returns true if this mapper has additional save data beyond PRG RAM.
    virtual bool has_mapper_save_data() const { return false; }

    // Get mapper-specific save data (e.g., EEPROM)
    virtual std::vector<uint8_t> get_mapper_save_data() const { return {}; }

    // Set mapper-specific save data
    virtual bool set_mapper_save_data(const std::vector<uint8_t>& data) { (void)data; return false; }

protected:
    std::vector<uint8_t>* m_prg_rom = nullptr;
    std::vector<uint8_t>* m_chr_rom = nullptr;
    std::vector<uint8_t>* m_prg_ram = nullptr;
    MirrorMode m_mirror_mode = MirrorMode::Horizontal;
    bool m_has_chr_ram = false;
};

// Factory function to create mapper by number
Mapper* create_mapper(int mapper_number,
                      std::vector<uint8_t>& prg_rom,
                      std::vector<uint8_t>& chr_rom,
                      std::vector<uint8_t>& prg_ram,
                      MirrorMode initial_mirror,
                      bool has_chr_ram);

} // namespace nes
