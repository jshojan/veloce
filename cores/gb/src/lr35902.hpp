#pragma once

#include "types.hpp"
#include <cstdint>
#include <array>
#include <vector>

namespace gb {

class Bus;

// Sharp LR35902 CPU emulator (Game Boy CPU)
// Hybrid of Z80 and 8080 with custom extensions
class LR35902 {
public:
    explicit LR35902(Bus& bus);
    ~LR35902();

    // Reset the CPU
    void reset();

    // Execute one instruction, return M-cycles consumed
    int step();

    // Handle pending interrupts
    void handle_interrupts(uint8_t pending);

    // Debug access
    uint16_t get_pc() const { return m_pc; }
    uint16_t get_sp() const { return m_sp; }
    uint8_t get_a() const { return m_a; }
    uint8_t get_f() const { return m_f; }
    uint8_t get_b() const { return m_b; }
    uint8_t get_c() const { return m_c; }
    uint8_t get_d() const { return m_d; }
    uint8_t get_e() const { return m_e; }
    uint8_t get_h() const { return m_h; }
    uint8_t get_l() const { return m_l; }
    bool is_halted() const { return m_halted; }

    // Save/load state
    void save_state(std::vector<uint8_t>& data);
    void load_state(const uint8_t*& data, size_t& remaining);

private:
    // Memory access
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    uint8_t fetch();
    uint16_t fetch16();

    // Register pair access
    uint16_t get_af() const { return (static_cast<uint16_t>(m_a) << 8) | m_f; }
    uint16_t get_bc() const { return (static_cast<uint16_t>(m_b) << 8) | m_c; }
    uint16_t get_de() const { return (static_cast<uint16_t>(m_d) << 8) | m_e; }
    uint16_t get_hl() const { return (static_cast<uint16_t>(m_h) << 8) | m_l; }

    void set_af(uint16_t value) { m_a = value >> 8; m_f = value & 0xF0; }  // Lower 4 bits always 0
    void set_bc(uint16_t value) { m_b = value >> 8; m_c = value & 0xFF; }
    void set_de(uint16_t value) { m_d = value >> 8; m_e = value & 0xFF; }
    void set_hl(uint16_t value) { m_h = value >> 8; m_l = value & 0xFF; }

    // Flag operations
    bool get_flag_z() const { return m_f & FLAG_Z; }
    bool get_flag_n() const { return m_f & FLAG_N; }
    bool get_flag_h() const { return m_f & FLAG_H; }
    bool get_flag_c() const { return m_f & FLAG_C; }

    void set_flag_z(bool v) { if (v) m_f |= FLAG_Z; else m_f &= ~FLAG_Z; }
    void set_flag_n(bool v) { if (v) m_f |= FLAG_N; else m_f &= ~FLAG_N; }
    void set_flag_h(bool v) { if (v) m_f |= FLAG_H; else m_f &= ~FLAG_H; }
    void set_flag_c(bool v) { if (v) m_f |= FLAG_C; else m_f &= ~FLAG_C; }

    // Stack operations
    void push(uint16_t value);
    uint16_t pop();

    // ALU operations
    void alu_add(uint8_t value, bool with_carry);
    void alu_sub(uint8_t value, bool with_carry);
    void alu_and(uint8_t value);
    void alu_or(uint8_t value);
    void alu_xor(uint8_t value);
    void alu_cp(uint8_t value);
    uint8_t alu_inc(uint8_t value);
    uint8_t alu_dec(uint8_t value);

    // Rotate/shift operations
    uint8_t rlc(uint8_t value);
    uint8_t rrc(uint8_t value);
    uint8_t rl(uint8_t value);
    uint8_t rr(uint8_t value);
    uint8_t sla(uint8_t value);
    uint8_t sra(uint8_t value);
    uint8_t swap(uint8_t value);
    uint8_t srl(uint8_t value);

    // Bit operations
    void bit(int n, uint8_t value);
    uint8_t res(int n, uint8_t value);
    uint8_t set(int n, uint8_t value);

    // Execute CB-prefixed instruction
    int execute_cb();

    // Bus reference
    Bus& m_bus;

    // Registers
    uint8_t m_a = 0;    // Accumulator
    uint8_t m_f = 0;    // Flags
    uint8_t m_b = 0;
    uint8_t m_c = 0;
    uint8_t m_d = 0;
    uint8_t m_e = 0;
    uint8_t m_h = 0;
    uint8_t m_l = 0;
    uint16_t m_sp = 0;  // Stack pointer
    uint16_t m_pc = 0;  // Program counter

    // Interrupt master enable
    bool m_ime = false;
    bool m_ime_pending = false;  // EI enables after next instruction

    // CPU state
    bool m_halted = false;
    bool m_halt_bug = false;  // HALT bug: PC not incremented after HALT when IME=0

    // Flag bit positions
    static constexpr uint8_t FLAG_Z = 0x80;  // Zero
    static constexpr uint8_t FLAG_N = 0x40;  // Subtract
    static constexpr uint8_t FLAG_H = 0x20;  // Half-carry
    static constexpr uint8_t FLAG_C = 0x10;  // Carry

    // Cycle table for main opcodes
    static const uint8_t s_cycle_table[256];

    // Cycle table for CB-prefixed opcodes
    static const uint8_t s_cb_cycle_table[256];
};

} // namespace gb
