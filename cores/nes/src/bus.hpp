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
// Implements cycle-accurate CPU/PPU/APU synchronization
class Bus {
public:
    Bus();
    ~Bus();

    // Connect components
    void connect_cpu(CPU* cpu) { m_cpu = cpu; }
    void connect_ppu(PPU* ppu) { m_ppu = ppu; }
    void connect_apu(APU* apu) { m_apu = apu; }
    void connect_cartridge(Cartridge* cart) { m_cartridge = cart; }

    // CPU memory access - these tick PPU/APU for cycle accuracy
    // Each memory access takes 1 CPU cycle = 3 PPU cycles
    uint8_t cpu_read(uint16_t address);
    void cpu_write(uint16_t address, uint8_t value);

    // Non-ticking memory access (for save states, debugging, etc.)
    uint8_t cpu_peek(uint16_t address) const;

    // PPU memory access (for CHR ROM/RAM)
    uint8_t ppu_read(uint16_t address, uint32_t frame_cycle = 0);
    void ppu_write(uint16_t address, uint8_t value);

    // Controller input
    void set_controller_state(int controller, uint32_t buttons);
    uint8_t read_controller(int controller);

    // DMA - OAM DMA is handled cycle-by-cycle now
    void start_oam_dma(uint8_t page);
    bool is_dma_active() const { return m_dma_active; }
    void run_dma_cycle();  // Run one DMA cycle, ticking PPU/APU
    int get_pending_dma_cycles();  // Legacy - returns 0 now since DMA is inline

    // Mapper scanline counter (for MMC3, etc.)
    void mapper_scanline();

    // Notify mapper of PPU address changes (for MMC3 A12 clocking)
    void notify_ppu_addr_change(uint16_t old_addr, uint16_t new_addr, uint32_t frame_cycle);

    // Notify mapper of PPU address bus activity during rendering (for A12 tracking)
    void notify_ppu_address_bus(uint16_t address, uint32_t frame_cycle);

    // Notify mapper of frame start (for resetting timing state)
    void notify_frame_start();

    // Check for mapper IRQ
    bool mapper_irq_pending(uint32_t frame_cycle = 0);
    void mapper_irq_clear();

    // CPU cycle notification for mappers (IRQ counters, expansion audio)
    void mapper_cpu_cycles(int count);
    void mapper_cpu_cycle();

    // Get expansion audio output from mapper (-1.0 to 1.0)
    float get_mapper_audio() const;

    // Get current mirror mode (0=H, 1=V, 2=SingleScreen0, 3=SingleScreen1, 4=FourScreen)
    int get_mirror_mode() const;

    // Tick PPU and APU for one CPU cycle (3 PPU cycles)
    // This is the core synchronization mechanism
    // Returns true if an NMI edge was detected during this cycle
    bool tick();

    // Tick only PPU (for internal use)
    void tick_ppu_only(int ppu_cycles);

    // Enable/disable cycle-accurate mode
    // When disabled, PPU/APU are not ticked during memory accesses (legacy mode)
    void set_cycle_accurate(bool enabled) { m_cycle_accurate = enabled; }
    bool is_cycle_accurate() const { return m_cycle_accurate; }

    // Check and handle NMI/IRQ after ticking
    void check_interrupts();

    // Poll IRQ status (for cycle-accurate interrupt detection)
    // Returns true if any IRQ source is active
    bool poll_irq_status();

    // Get the current CPU cycle count (for APU jitter timing)
    uint64_t get_current_cpu_cycle() const { return m_cpu_cycles; }

    // Get total CPU cycles elapsed (for debugging)
    uint64_t get_cpu_cycles() const { return m_cpu_cycles; }

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

    // OAM DMA state - cycle-accurate handling
    bool m_dma_active = false;
    uint8_t m_dma_page = 0;
    int m_dma_cycle = 0;      // Current DMA cycle (0-513)
    uint8_t m_dma_data = 0;   // Data being transferred
    bool m_dma_dummy_cycle = false;  // Whether we need an extra alignment cycle

    // Legacy DMA cycles (kept for compatibility, always 0 now)
    int m_pending_dma_cycles = 0;

    // Cycle-accurate mode flag
    bool m_cycle_accurate = true;

    // CPU cycle counter
    uint64_t m_cpu_cycles = 0;
};

} // namespace nes
