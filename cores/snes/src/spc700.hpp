#pragma once

#include <cstdint>
#include <array>
#include <vector>

namespace snes {

class DSP;

// Sony SPC700 Sound Processor
// 8-bit CPU running at ~1.024 MHz
// Reference: fullsnes, anomie's SPC700 doc
class SPC700 {
public:
    SPC700();
    ~SPC700();

    void reset();

    // Connect DSP for register access
    void connect_dsp(DSP* dsp) { m_dsp = dsp; }

    // Execute one instruction, return cycles consumed
    int step();

    // Communication ports (main CPU <-> SPC700)
    uint8_t read_port(int port);
    void write_port(int port, uint8_t value);

    // Main CPU side port access
    uint8_t cpu_read_port(int port);
    void cpu_write_port(int port, uint8_t value);

    // IPL ROM enable/disable
    void set_ipl_rom_enabled(bool enabled) { m_ipl_rom_enabled = enabled; }

    // Get audio RAM for DMA
    uint8_t* get_ram() { return m_ram.data(); }
    const uint8_t* get_ram() const { return m_ram.data(); }

    // Save state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

    // Debug
    uint16_t get_pc() const { return m_pc; }

private:
    // Memory access
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    uint8_t read_dp(uint8_t address);
    void write_dp(uint8_t address, uint8_t value);

    // Stack operations
    void push(uint8_t value);
    uint8_t pop();
    void push16(uint16_t value);
    uint16_t pop16();

    // Flag operations
    void set_flag(uint8_t flag, bool value);
    bool get_flag(uint8_t flag) const;
    void update_nz(uint8_t value);

    // ALU operations
    uint8_t op_adc(uint8_t a, uint8_t b);
    uint8_t op_sbc(uint8_t a, uint8_t b);
    uint8_t op_and(uint8_t a, uint8_t b);
    uint8_t op_or(uint8_t a, uint8_t b);
    uint8_t op_eor(uint8_t a, uint8_t b);
    void op_cmp(uint8_t a, uint8_t b);
    uint8_t op_asl(uint8_t value);
    uint8_t op_lsr(uint8_t value);
    uint8_t op_rol(uint8_t value);
    uint8_t op_ror(uint8_t value);
    uint8_t op_inc(uint8_t value);
    uint8_t op_dec(uint8_t value);

    // Addressing mode helpers
    uint16_t addr_dp();
    uint16_t addr_dp_x();
    uint16_t addr_dp_y();
    uint16_t addr_abs();
    uint16_t addr_abs_x();
    uint16_t addr_abs_y();
    uint16_t addr_dp_x_ind();  // (dp+X)
    uint16_t addr_dp_ind_y();  // (dp)+Y

    // Execute single instruction
    void execute();

    // DSP reference
    DSP* m_dsp = nullptr;

    // Registers
    uint8_t m_a = 0;        // Accumulator
    uint8_t m_x = 0;        // X index
    uint8_t m_y = 0;        // Y index
    uint8_t m_sp = 0xEF;    // Stack pointer (in page 1)
    uint16_t m_pc = 0xFFC0; // Program counter (starts at IPL ROM)
    uint8_t m_psw = 0;      // Processor status word

    // Memory (64KB)
    std::array<uint8_t, 0x10000> m_ram;

    // IPL ROM (64 bytes) - boot ROM
    static const uint8_t IPL_ROM[64];
    bool m_ipl_rom_enabled = true;

    // I/O ports ($F4-$F7)
    std::array<uint8_t, 4> m_port_out;  // SPC -> CPU
    std::array<uint8_t, 4> m_port_in;   // CPU -> SPC

    // Timers ($FA-$FC targets, $FD-$FF outputs)
    std::array<uint8_t, 3> m_timer_target;
    std::array<uint8_t, 3> m_timer_counter;
    std::array<uint8_t, 3> m_timer_output;
    std::array<bool, 3> m_timer_enabled;
    std::array<int, 3> m_timer_divider;

    // Control register ($F1)
    uint8_t m_control = 0x80;

    // Cycle counter
    int m_cycles = 0;

    // PSW flags
    static constexpr uint8_t FLAG_C = 0x01;  // Carry
    static constexpr uint8_t FLAG_Z = 0x02;  // Zero
    static constexpr uint8_t FLAG_I = 0x04;  // Interrupt enable
    static constexpr uint8_t FLAG_H = 0x08;  // Half-carry
    static constexpr uint8_t FLAG_B = 0x10;  // Break
    static constexpr uint8_t FLAG_P = 0x20;  // Direct page (0=00xx, 1=01xx)
    static constexpr uint8_t FLAG_V = 0x40;  // Overflow
    static constexpr uint8_t FLAG_N = 0x80;  // Negative
};

} // namespace snes
