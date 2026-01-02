#pragma once

#include "types.hpp"
#include <cstdint>
#include <array>
#include <vector>

namespace gba {

class ARM7TDMI;
class PPU;
class APU;
class Cartridge;

// GBA Memory Bus with proper timing
class Bus {
public:
    Bus();
    ~Bus();

    // Connect components
    void connect_cpu(ARM7TDMI* cpu) { m_cpu = cpu; }
    void connect_ppu(PPU* ppu) { m_ppu = ppu; }
    void connect_apu(APU* apu) { m_apu = apu; }
    void connect_cartridge(Cartridge* cart) { m_cartridge = cart; }

    // Memory access
    uint8_t read8(uint32_t address);
    uint16_t read16(uint32_t address);
    uint32_t read32(uint32_t address);
    void write8(uint32_t address, uint8_t value);
    void write16(uint32_t address, uint16_t value);
    void write32(uint32_t address, uint32_t value);

    // Unaligned write support (needed for correct SRAM byte selection)
    void write16_unaligned(uint32_t address, uint16_t value);
    void write32_unaligned(uint32_t address, uint32_t value);

    // I/O register access
    uint16_t read_io(uint32_t address);
    void write_io(uint32_t address, uint16_t value);

    // Input handling
    void set_input_state(uint32_t buttons);

    // BIOS protection simulation - update the "last BIOS read" value
    // Used by HLE BIOS functions to simulate proper BIOS behavior
    void set_last_bios_read(uint32_t value) { m_last_bios_read = value; }

    // Interrupt handling
    bool check_interrupts();
    void request_interrupt(GBAInterrupt irq);

    // DMA access
    void trigger_dma(int channel);
    int run_dma();
    void trigger_vblank_dma();
    void trigger_hblank_dma();

    // Timer access
    void step_timers(int cycles);
    void write_timer_control(int timer, uint16_t value);

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // PPU register access
    uint16_t get_bgcnt(int layer) const { return m_bgcnt[layer]; }
    uint16_t get_bghofs(int layer) const { return m_bghofs[layer]; }
    uint16_t get_bgvofs(int layer) const { return m_bgvofs[layer]; }
    uint16_t get_dispcnt() const { return m_dispcnt; }

    // Affine background parameters (layer 0=BG2, 1=BG3)
    int16_t get_bgpa(int layer) const { return static_cast<int16_t>(m_bgpa[layer]); }
    int16_t get_bgpb(int layer) const { return static_cast<int16_t>(m_bgpb[layer]); }
    int16_t get_bgpc(int layer) const { return static_cast<int16_t>(m_bgpc[layer]); }
    int16_t get_bgpd(int layer) const { return static_cast<int16_t>(m_bgpd[layer]); }
    int32_t get_bgx(int layer) const { return m_bgx[layer]; }
    int32_t get_bgy(int layer) const { return m_bgy[layer]; }

    // Window registers
    uint16_t get_win0h() const { return m_win0h; }
    uint16_t get_win1h() const { return m_win1h; }
    uint16_t get_win0v() const { return m_win0v; }
    uint16_t get_win1v() const { return m_win1v; }
    uint16_t get_winin() const { return m_winin; }
    uint16_t get_winout() const { return m_winout; }

    // Blending registers
    uint16_t get_bldcnt() const { return m_bldcnt; }
    uint16_t get_bldalpha() const { return m_bldalpha; }
    uint16_t get_bldy() const { return m_bldy; }

    // Interrupt registers (for debugging)
    uint16_t get_ie() const { return m_ie; }
    uint16_t get_if() const { return m_if; }
    uint16_t get_ime() const { return m_ime; }

private:
    // Get memory region and calculate wait states
    MemoryRegion get_region(uint32_t address);
    int get_wait_states(uint32_t address, bool is_sequential, int access_size);

    // Open bus behavior
    uint32_t get_open_bus_value(uint32_t address);

    // Components
    ARM7TDMI* m_cpu = nullptr;
    PPU* m_ppu = nullptr;
    APU* m_apu = nullptr;
    Cartridge* m_cartridge = nullptr;

    // Memory regions
    std::array<uint8_t, 0x4000> m_bios;      // 16KB BIOS
    std::array<uint8_t, 0x40000> m_ewram;    // 256KB External WRAM
    std::array<uint8_t, 0x8000> m_iwram;     // 32KB Internal WRAM

    // I/O Registers (directly mapped for fast access)
    // Display
    uint16_t m_dispcnt = 0;      // 0x4000000 - LCD Control
    uint16_t m_dispstat = 0;     // 0x4000004 - General LCD Status
    uint16_t m_vcount = 0;       // 0x4000006 - Vertical Counter

    // Background control
    std::array<uint16_t, 4> m_bgcnt;     // BG0-3 Control
    std::array<uint16_t, 4> m_bghofs;    // BG0-3 Horizontal Offset
    std::array<uint16_t, 4> m_bgvofs;    // BG0-3 Vertical Offset

    // Affine background parameters
    std::array<int32_t, 2> m_bgpa;       // BG2/3 Rotation/Scaling Parameter A
    std::array<int32_t, 2> m_bgpb;       // BG2/3 Rotation/Scaling Parameter B
    std::array<int32_t, 2> m_bgpc;       // BG2/3 Rotation/Scaling Parameter C
    std::array<int32_t, 2> m_bgpd;       // BG2/3 Rotation/Scaling Parameter D
    std::array<int32_t, 2> m_bgx;        // BG2/3 Reference Point X
    std::array<int32_t, 2> m_bgy;        // BG2/3 Reference Point Y

    // Window
    uint16_t m_win0h = 0, m_win1h = 0;
    uint16_t m_win0v = 0, m_win1v = 0;
    uint16_t m_winin = 0, m_winout = 0;

    // Special effects
    uint16_t m_mosaic = 0;
    uint16_t m_bldcnt = 0;
    uint16_t m_bldalpha = 0;
    uint16_t m_bldy = 0;

    // Sound registers
    std::array<uint16_t, 0x30> m_sound_regs;

    // DMA registers
    struct DMAChannel {
        uint32_t src = 0;
        uint32_t dst = 0;
        uint16_t count = 0;
        uint16_t control = 0;
        uint32_t internal_src = 0;
        uint32_t internal_dst = 0;
        uint32_t internal_count = 0;
        bool active = false;  // Set when DMA is first enabled, cleared when disabled
    };
    std::array<DMAChannel, 4> m_dma;

    // DMA helper
    void run_dma_channel(int channel);

    // Timer registers
    struct Timer {
        uint16_t counter = 0;
        uint16_t reload = 0;
        uint16_t control = 0;
        int prescaler_counter = 0;
    };
    std::array<Timer, 4> m_timers;

    // Interrupt registers
    uint16_t m_ie = 0;       // Interrupt Enable
    uint16_t m_if = 0;       // Interrupt Request Flags
    uint16_t m_ime = 0;      // Interrupt Master Enable

    // Key input
    uint16_t m_keyinput = 0x3FF;  // All buttons released
    uint16_t m_keycnt = 0;

    // Wait state control
    uint16_t m_waitcnt = 0;

    // Halt control
    uint8_t m_haltcnt = 0;

    // Post-boot flag
    uint8_t m_postflg = 0;

    // Prefetch buffer state (for accurate open bus)
    uint32_t m_last_read_value = 0;

    // BIOS protection - last value read from BIOS during BIOS execution
    // After startup (without BIOS), this simulates the value at 0xDC+8=0xE4: 0xE129F000
    uint32_t m_last_bios_read = 0xE129F000;

    // Wait state lookup tables (populated based on WAITCNT)
    std::array<int, 16> m_ws_n;  // Non-sequential
    std::array<int, 16> m_ws_s;  // Sequential
};

} // namespace gba
