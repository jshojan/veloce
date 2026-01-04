#pragma once

#include "types.hpp"
#include <cstdint>
#include <array>
#include <vector>
#include <string>

namespace gb {

class LR35902;
class PPU;
class APU;
class Cartridge;

// Game Boy Memory Bus
class Bus {
public:
    Bus();
    ~Bus();

    // Connect components
    void connect_cpu(LR35902* cpu) { m_cpu = cpu; }
    void connect_ppu(PPU* ppu) { m_ppu = ppu; }
    void connect_apu(APU* apu) { m_apu = apu; }
    void connect_cartridge(Cartridge* cart) { m_cartridge = cart; }

    // CGB mode
    void set_cgb_mode(bool cgb) { m_cgb_mode = cgb; }
    bool is_cgb_mode() const { return m_cgb_mode; }

    // Memory access
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    // Input handling
    void set_input_state(uint32_t buttons);

    // Interrupt handling
    uint8_t get_pending_interrupts();
    void request_interrupt(uint8_t irq);
    void clear_interrupt(uint8_t irq);

    // OAM DMA
    void start_oam_dma(uint8_t page);
    void step_oam_dma();
    bool is_oam_dma_active() const { return m_oam_dma_active; }

    // OAM bug trigger (DMG only)
    // Called by CPU when 16-bit register pair operations occur with OAM-range addresses
    void trigger_oam_bug(uint16_t address, bool is_read);

    // Timer
    void step_timer(int cycles);

    // Tick system by 1 M-cycle (4 T-cycles) - used for cycle-accurate timing
    // This ticks timer, serial, PPU, and APU
    void tick_m_cycle();

    // Serial
    void step_serial(int cycles);

    // Serial output capture (for test ROMs)
    const std::string& get_serial_output() const { return m_serial_output; }
    void clear_serial_output() { m_serial_output.clear(); }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    // Components
    LR35902* m_cpu = nullptr;
    PPU* m_ppu = nullptr;
    APU* m_apu = nullptr;
    Cartridge* m_cartridge = nullptr;

    // Memory
    std::array<uint8_t, 0x2000> m_wram;      // 8KB Work RAM (banked on CGB)
    std::array<uint8_t, 0x6000> m_wram_cgb;  // Extra 24KB for CGB banks 2-7
    std::array<uint8_t, 0x7F> m_hram;        // High RAM

    // I/O Registers
    uint8_t m_joyp = 0xCF;       // FF00 - Joypad
    uint8_t m_sb = 0;            // FF01 - Serial transfer data
    uint8_t m_sc = 0;            // FF02 - Serial control
    uint8_t m_div = 0;           // FF04 - Divider (high byte)
    uint8_t m_tima = 0;          // FF05 - Timer counter
    uint8_t m_tma = 0;           // FF06 - Timer modulo
    uint8_t m_tac = 0;           // FF07 - Timer control
    uint8_t m_if = 0;            // FF0F - Interrupt flags
    uint8_t m_ie = 0;            // FFFF - Interrupt enable

    // CGB-specific registers
    uint8_t m_key1 = 0;          // FF4D - CPU speed switch
    uint8_t m_vbk = 0;           // FF4F - VRAM bank
    uint8_t m_hdma1 = 0;         // FF51 - HDMA source high
    uint8_t m_hdma2 = 0;         // FF52 - HDMA source low
    uint8_t m_hdma3 = 0;         // FF53 - HDMA dest high
    uint8_t m_hdma4 = 0;         // FF54 - HDMA dest low
    uint8_t m_hdma5 = 0;         // FF55 - HDMA control
    uint8_t m_rp = 0;            // FF56 - Infrared
    uint8_t m_svbk = 0;          // FF70 - WRAM bank

    // Joypad state
    uint8_t m_joypad_buttons = 0xFF;   // Button states
    uint8_t m_joypad_directions = 0xFF; // Direction states

    // OAM DMA
    bool m_oam_dma_active = false;
    uint16_t m_oam_dma_src = 0;
    uint8_t m_oam_dma_offset = 0;

    // Timer internals - using falling edge detection like real hardware
    uint16_t m_div_counter = 0;  // Full 16-bit DIV counter (system counter)
    bool m_prev_timer_bit = false;  // Previous state of selected bit for falling edge detection
    uint8_t m_tima_overflow_cycle = 0;  // Countdown for delayed TMA reload (0 = no overflow pending)

    // Serial internals
    int m_serial_counter = 0;
    int m_serial_bits = 0;
    std::string m_serial_output;  // Captured serial output for test ROMs

    // CGB mode flag
    bool m_cgb_mode = false;
    bool m_double_speed = false;

    // Bit positions in div_counter for each TAC clock select
    // TAC bits 1-0: 00=bit 9 (4096Hz), 01=bit 3 (262144Hz), 10=bit 5 (65536Hz), 11=bit 7 (16384Hz)
    static constexpr int TIMER_DIV_BITS[] = {9, 3, 5, 7};

    // Helper to get the current timer tick bit from DIV counter
    bool get_timer_bit() const;

    // Check for falling edge and increment TIMA if needed
    void check_timer_falling_edge(bool new_bit);

    // I/O helpers
    uint8_t read_io(uint16_t address);
    void write_io(uint16_t address, uint8_t value);
};

} // namespace gb
