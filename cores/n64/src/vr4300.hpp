#pragma once
#include <array>
#include <cstdint>
#include "memory.hpp"
#include "cache.hpp"
#include "tlb.hpp"

namespace n64 {
struct CpuState {
    std::array<uint64_t, 32> gpr{};
    std::array<uint64_t, 32> cp0{};
    uint32_t fcr31 = 0;
    std::array<uint64_t,32> fpr{};
    uint64_t hi = 0, lo = 0, pc = 0, next_pc = 0, instructions = 0;
    bool delay_slot = false;
    bool llbit = false;
};
class VR4300 {
public:
    explicit VR4300(Memory& memory) : memory(memory) {}
    void reset(uint64_t entry);
    void step();
    // For the machine's idle loop only: advance each bus clock before completion,
    // and stop at the first external interrupt or frontend scheduling boundary.
    unsigned cached_idle_budget(unsigned maximum) const;
    void complete_cached_idle(unsigned attempts,uint8_t pending);
    CpuState state;
    PrimaryCaches caches;
    Tlb tlb;
    // Transient diagnostics for the most recent attempt; no guest-visible state.
    bool exception_taken=false,instruction_valid=false;
    uint32_t instruction_word=0;
    uint64_t access_address=0;
    unsigned access_bytes=0;
    bool access_store=false;
    // Host-only execution diagnostics; excluded from save and rollback state.
    bool cached_idle_enabled=true;
    uint64_t cached_idle_steps=0;
private:
    Memory& memory;
    void exception(unsigned code, uint64_t fault_pc, bool delay, uint64_t bad_address = 0,bool refill = false,bool extended = false);
    bool translate(uint64_t address,bool store,uint64_t pc,bool delay,Tlb::Translation& result);
    bool watch(uint32_t physical,bool store,uint64_t pc,bool delay);
    bool read(uint64_t address,unsigned bytes,uint64_t& value,bool fetch,uint64_t pc,bool delay,uint64_t fault_address);
    bool write(uint64_t address,unsigned bytes,uint64_t value,uint8_t mask,uint64_t pc,bool delay,uint64_t fault_address);
};
uint64_t sign32(uint32_t value);
uint64_t sign16(uint16_t value);
uint64_t arithmetic_right(uint64_t value, unsigned amount);
}
