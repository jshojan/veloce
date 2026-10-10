#pragma once
#include <array>
#include <cstdint>
namespace ppc {
struct State {
    std::array<uint32_t,32> r{};
    uint32_t pc=0,cr=0,xer=0,lr=0,ctr=0;
    uint64_t steps=0;
    bool unsupported=false;
};
// Big-endian instruction/transfer values; failed bus operations do not mutate.
struct Memory {
    virtual ~Memory()=default;
    virtual bool read(uint32_t,unsigned,uint32_t&) const=0;
    virtual bool write(uint32_t,unsigned,uint32_t)=0;
};
void execute(State&,Memory&);
}
