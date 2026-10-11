#pragma once
#include <cstdint>
namespace n64 {
// SetPrimDepth ignores the high Z bit. Delta is the raw unsigned command field,
// unlike normalized triangle gradients; one-cycle comparison uses both directly.
struct RdpPrimitiveDepth {
    uint32_t value=0;
    void command(uint64_t word){value=uint32_t(word)&0x7fffffffu;}
    unsigned z()const{return (value>>16)*8;}
    uint16_t delta()const{return uint16_t(value);}
    uint64_t snapshot()const{return value;}
    bool restore(uint64_t state){if(state>0x7fffffffu)return false;value=uint32_t(state);return true;}
};
}
