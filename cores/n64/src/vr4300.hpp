#pragma once
#include <array>
#include <cstdint>

namespace n64 {
struct Memory {
    virtual ~Memory() = default;
    virtual bool read(uint64_t address, unsigned bytes, uint64_t& value) = 0;
    virtual bool write(uint64_t address, unsigned bytes, uint64_t value) = 0;
    // Aligned word/doubleword transaction; bit i enables ascending memory byte i.
    // Reject before modifying storage. No read-modify-write bus read is required.
    virtual bool write_masked(uint64_t address, unsigned bytes, uint64_t value, uint8_t mask) = 0;
};
struct CpuState {
    std::array<uint64_t, 32> gpr{};
    std::array<uint64_t, 32> cp0{};
    uint64_t hi = 0, lo = 0, pc = 0, next_pc = 0, instructions = 0;
    bool delay_slot = false;
};
class VR4300 {
public:
    explicit VR4300(Memory& memory) : memory(memory) {}
    void reset(uint64_t entry);
    void step();
    CpuState state;
private:
    Memory& memory;
    void exception(unsigned code, uint64_t fault_pc, bool delay, uint64_t bad_address = 0);
};
uint64_t sign32(uint32_t value);
uint64_t sign16(uint16_t value);
uint64_t arithmetic_right(uint64_t value, unsigned amount);
}
