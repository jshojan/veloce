#pragma once
#include <array>
#include <cstdint>

namespace vb {
struct Memory {
    virtual ~Memory() = default;
    virtual bool read(uint32_t address, unsigned bytes, uint32_t& value) = 0;
    virtual bool write(uint32_t address, unsigned bytes, uint32_t value) = 0;
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
    void divide_zero(uint32_t pc);
    Memory& bus;
};
uint32_t extend(uint32_t value, unsigned bits);
}
