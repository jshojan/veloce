#pragma once
#include <array>
#include <cstdint>

namespace vb {
struct Memory {
    virtual ~Memory() = default;
    virtual bool read(uint32_t address, unsigned bytes, uint32_t& value) = 0;
    virtual bool write(uint32_t address, unsigned bytes, uint32_t value) = 0;
    virtual bool read_port(uint32_t, unsigned, uint32_t&) { return false; }
    virtual bool write_port(uint32_t, unsigned, uint32_t) { return false; }
    // One indivisible transaction for the CPU's bus owner. A failed compare
    // still writes the old word back, as required by CAXI.
    // Return bus-access success, including a comparison mismatch; false rejects
    // the transaction rather than reporting that the comparison was unequal.
    virtual bool compare_exchange(uint32_t, uint32_t, uint32_t, uint32_t&) { return false; }
};
struct CpuState {
    std::array<uint32_t,32> gpr{}, sr{};
    uint32_t pc = 0xfffffff0;
    uint64_t steps = 0;
    bool halted = false, unsupported = false;
};
class V810 {
public:
    explicit V810(Memory& bus) : bus(bus) {}
    void reset();
    void step();
    static bool condition(unsigned code, uint32_t psw);
    CpuState state;
private:
    void exception(uint32_t pc, uint32_t restore_pc, uint16_t code, uint32_t vector);
    Memory& bus;
};
uint32_t extend(uint32_t value, unsigned bits);
}
