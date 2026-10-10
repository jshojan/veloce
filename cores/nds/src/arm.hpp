#pragma once
#include <array>
#include <cstdint>
namespace nds {
struct Memory {
    virtual ~Memory()=default;
    virtual bool read(uint32_t address,unsigned bytes,uint32_t& value)=0;
    virtual bool write(uint32_t address,unsigned bytes,uint32_t value)=0;
};
struct ArmState {
    std::array<uint32_t,16> r{};
    uint32_t cpsr=0xdf;
    uint64_t steps=0;
    bool unsupported=false;
};
class Arm {
public:
    Arm(Memory& memory,bool arm9,bool arm11=false):bus(memory),v5(arm9 || arm11),v6(arm11) {}
    void reset(uint32_t entry) { state={}; state.r[15]=entry; }
    void step();
    static bool condition(unsigned code,uint32_t cpsr);
    static uint32_t shift(uint32_t value,unsigned type,unsigned amount,bool by_register,bool& carry);
    ArmState state;
private:
    void thumb_step();
    bool transfer(uint32_t address,unsigned width,bool load,bool sign,uint32_t& value);
    Memory& bus;
    bool v5,v6;
};
}
