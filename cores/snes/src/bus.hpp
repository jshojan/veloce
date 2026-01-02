#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include "debug.hpp"

namespace snes {

class CPU;
class PPU;
class APU;
class DMA;
class Cartridge;

// SNES Memory Bus - connects all components
// Handles the complex SNES memory mapping
class Bus {
public:
    Bus();
    ~Bus();

    // Connect components
    void connect_cpu(CPU* cpu) { m_cpu = cpu; }
    void connect_ppu(PPU* ppu) { m_ppu = ppu; }
    void connect_apu(APU* apu) { m_apu = apu; }
    void connect_dma(DMA* dma) { m_dma = dma; }
    void connect_cartridge(Cartridge* cart) { m_cartridge = cart; }

    // Memory access (24-bit address)
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);

    // Controller input
    void set_controller_state(int controller, uint32_t buttons);

    // Frame timing
    void start_frame();
    void start_hblank();
    void start_vblank();

    // NMI/IRQ management
    bool nmi_pending() const { return m_nmi_pending; }
    void clear_nmi() { m_nmi_pending = false; }
    bool irq_pending() const;
    void set_irq_line(bool active);

    // CPU I/O register access ($4200-$421F)
    uint8_t read_cpu_io(uint16_t address);
    void write_cpu_io(uint16_t address, uint8_t value);

    // Get/set open bus value
    uint8_t get_open_bus() const { return m_open_bus; }

    // Blargg test detection
    BlarggTestState& get_blargg_state() { return m_blargg_state; }
    const BlarggTestState& get_blargg_state() const { return m_blargg_state; }
    void report_blargg_result(uint64_t frame_count) { m_blargg_state.report(frame_count); }
    bool blargg_test_completed() const { return m_blargg_state.should_exit(); }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    // Components
    CPU* m_cpu = nullptr;
    PPU* m_ppu = nullptr;
    APU* m_apu = nullptr;
    DMA* m_dma = nullptr;
    Cartridge* m_cartridge = nullptr;

    // Work RAM (128KB)
    std::array<uint8_t, 0x20000> m_wram;

    // Open bus value
    uint8_t m_open_bus = 0;

    // Controller state
    std::array<uint32_t, 2> m_controller_state;
    std::array<uint16_t, 2> m_controller_latch;
    bool m_auto_joypad_read = false;
    int m_joypad_counter = 0;

    // CPU I/O registers ($4200-$421F)
    uint8_t m_nmitimen = 0;    // $4200 - NMI/IRQ enable
    uint8_t m_wrio = 0xFF;     // $4201 - Programmable I/O port (output)
    uint16_t m_wrmpya = 0;     // $4202-$4203 - Multiplication
    uint16_t m_wrmpyb = 0;
    uint16_t m_wrdiv = 0;      // $4204-$4205 - Division
    uint8_t m_wrdivb = 0;      // $4206
    uint16_t m_htime = 0;      // $4207-$4208 - H-IRQ time
    uint16_t m_vtime = 0;      // $4209-$420A - V-IRQ time
    uint8_t m_mdmaen = 0;      // $420B - DMA enable
    uint8_t m_hdmaen = 0;      // $420C - HDMA enable
    uint8_t m_memsel = 0;      // $420D - FastROM select

    // Math results
    uint16_t m_rddiv = 0;      // $4214-$4215 - Division result
    uint16_t m_rdmpy = 0;      // $4216-$4217 - Multiplication result

    // Status
    bool m_nmi_pending = false;
    bool m_nmi_flag = false;
    bool m_irq_flag = false;
    bool m_irq_line = false;
    uint8_t m_rdnmi = 0;       // $4210 - NMI flag
    uint8_t m_timeup = 0;      // $4211 - IRQ flag

    // H/V counters for IRQ timing
    int m_hcounter = 0;
    int m_vcounter = 0;

    // WRAM access port ($2180-$2183)
    uint32_t m_wram_addr = 0;  // 17-bit address for WRAM data port

    // Blargg test state (for automated testing)
    BlarggTestState m_blargg_state;
};

} // namespace snes
