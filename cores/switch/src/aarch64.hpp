#pragma once
#include <array>
#include <cstdint>
namespace nx {
struct Memory {
    virtual ~Memory()=default;
    virtual bool read(uint64_t address,unsigned size,uint64_t& value,bool instruction=false)=0;
    virtual bool write(uint64_t address,unsigned size,uint64_t value)=0;
};
struct CpuState {
    std::array<uint64_t,31> x{};
    uint64_t sp=0,pc=0,steps=0,fault_address=0;
    uint32_t fault_word=0;
    unsigned nzcv=0,fault=0; // N/Z/C/V in bits 3/2/1/0; terminal profile diagnostic.
};
class Aarch64 {
public:
    explicit Aarch64(Memory& bus):bus(bus){}
    CpuState state;
    bool step();
private:
    Memory& bus;
    uint64_t reg(unsigned n,bool stack=false) const{return n==31?(stack?state.sp:0):state.x[n];}
    void put(unsigned n,uint64_t value,bool stack=false){if(n<31)state.x[n]=value;else if(stack)state.sp=value;}
    bool fail(unsigned reason,uint64_t address,uint32_t word){state.fault=reason;state.fault_address=address;state.fault_word=word;return false;}
    bool condition(unsigned cond) const;
    uint64_t add(uint64_t a,uint64_t b,unsigned carry,unsigned width,bool flags);
};
}
