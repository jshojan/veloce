#pragma once
#include <array>
#include <cstdint>
#include "memory.hpp"
#include "cache.hpp"

namespace n64 {
struct CpuState {
    std::array<uint64_t, 32> gpr{};
    std::array<uint64_t, 32> cp0{};
    uint32_t fcr31 = 0;
    uint64_t hi = 0, lo = 0, pc = 0, next_pc = 0, instructions = 0;
    bool delay_slot = false;
};
class VR4300 {
public:
    explicit VR4300(Memory& memory) : memory(memory) {}
    void reset(uint64_t entry);
    void step();
    CpuState state;
    PrimaryCaches caches;
private:
    Memory& memory;
    void exception(unsigned code, uint64_t fault_pc, bool delay, uint64_t bad_address = 0);
};
uint64_t sign32(uint32_t value);
uint64_t sign16(uint16_t value);
uint64_t arithmetic_right(uint64_t value, unsigned amount);
}
