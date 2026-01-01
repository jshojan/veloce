#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace nes {

class CPU;
class PPU;
class APU;
class Cartridge;

// NES Memory Bus - connects all components
class Bus {
public:
    Bus();
    ~Bus();

    // Connect components
    void connect_cpu(CPU* cpu) { m_cpu = cpu; }
    void connect_ppu(PPU* ppu) { m_ppu = ppu; }
    void connect_apu(APU* apu) { m_apu = apu; }
    void connect_cartridge(Cartridge* cart) { m_cartridge = cart; }

    // CPU memory access
    uint8_t cpu_read(uint16_t address);
    void cpu_write(uint16_t address, uint8_t value);

    // PPU memory access (for CHR ROM/RAM)
    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0);
    void ppu_write(uint16_t address, uint8_t value);

    // Controller input
    void set_controller_state(int controller, uint32_t buttons);
    uint8_t read_controller(int controller);

    // DMA
    void oam_dma(uint8_t page);
    int get_pending_dma_cycles();  // Returns and clears pending DMA cycles

    // Mapper scanline counter (for MMC3, etc.)
    void mapper_scanline();

    // Notify mapper of PPU address changes (for MMC3 A12 clocking)
    void notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr);

    // Notify mapper of PPU address bus activity during rendering (for A12 tracking)
    void notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle);

    // Check for mapper IRQ
    bool mapper_irq_pending(uint32_t frame_cycle = 0);
    void mapper_irq_clear();

    // Get current mirror mode (0=H, 1=V, 2=SingleScreen0, 3=SingleScreen1, 4=FourScreen)
    int get_mirror_mode() const;

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // Test ROM support - check and print test output from $6000+
    void check_test_output();

private:
    // Components
    CPU* m_cpu = nullptr;
    PPU* m_ppu = nullptr;
    APU* m_apu = nullptr;
    Cartridge* m_cartridge = nullptr;

    // Internal RAM (2KB, mirrored 4 times in $0000-$1FFF)
    std::array<uint8_t, 2048> m_ram;

    // Controller state
    uint32_t m_controller_state[2] = {0, 0};
    uint8_t m_controller_shift[2] = {0, 0};
    bool m_controller_strobe = false;

    // DMA cycles pending (set by oam_dma, consumed by main loop)
    int m_pending_dma_cycles = 0;
};

} // namespace nes
