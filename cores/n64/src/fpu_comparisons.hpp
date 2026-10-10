#pragma once
#include <cstdint>
namespace n64 {
// Comparisons use raw integer encodings; denormals are ordered normally.
// VR4300 NaN convention: a set top fraction bit denotes a signaling NaN.
class FpuComparisons {
public:
    struct Result {bool condition=false;unsigned causes=0;};
    static Result calculate(unsigned fmt,unsigned predicate,uint64_t a,uint64_t b) {
        if((fmt!=16 && fmt!=17) || predicate>=16)return {false,32};
        const unsigned f=fmt==16?23:52,e=fmt==16?8:11;
        if(fmt==16){a=uint32_t(a);b=uint32_t(b);}
        const uint64_t sign=uint64_t(1)<<(f+e),fraction=(uint64_t(1)<<f)-1,exponent=((uint64_t(1)<<e)-1)<<f;
        const bool an=(a&exponent)==exponent && (a&fraction),bn=(b&exponent)==exponent && (b&fraction),unordered=an || bn;
        const unsigned causes=unordered && ((predicate&8) || (an && (a&(uint64_t(1)<<(f-1)))) || (bn && (b&(uint64_t(1)<<(f-1)))))?16:0;
        const uint64_t am=a&~sign,bm=b&~sign;
        const bool equal=!unordered && (a==b || (!am && !bm));
        const bool less=!unordered && !equal && ((a&sign)!=(b&sign)?bool(a&sign):(a&sign)?am>bm:am<bm);
        return {bool(((predicate&1) && unordered) || ((predicate&2) && equal) || ((predicate&4) && less)),causes};
    }
};
}
