#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// SetConvert packs six nine-bit fields. Reset weights are zero; a programmed
// zero field produces weight one, so retain whether the command has executed.
struct RdpConvert {
    static constexpr uint64_t fields=(1ull<<54)-1,programmed=1ull<<54;
    uint64_t state=0;
    void command(uint64_t word){state=(word&fields)|programmed;}
    unsigned coefficient(unsigned n) const{return unsigned(state>>(45-9*n))&511;}
    std::array<int64_t,4> weights() const{
        std::array<int64_t,4> result{};
        if(state&programmed)for(unsigned n=0;n<4;++n){const unsigned raw=coefficient(n);result[n]=2*(int64_t(raw)-(raw>=256?512:0))+1;}
        return result;
    }
    uint64_t snapshot() const{return state;}
    bool restore(uint64_t value){if(value&~(fields|programmed) || (!(value&programmed) && value))return false;state=value;return true;}
};
}
