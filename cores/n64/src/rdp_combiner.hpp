#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// SGI RDP Command Summary, tables 19, 21 and 22. Command state and
// fixed-point combiner arithmetic; source routing and draw evaluation remain
// separate work. No textured/triangle rasterizer is implied by this kernel.
struct RdpCombiner {
    // Operands are encoded nine-bit values. A/B/D use the RDP special
    // extension (only 0x180..0x1ff are negative); C uses ordinary signed nine
    // bits. RGB retains all seventeen bits for the later key/rounding stages.
    static uint32_t rgb_equation(unsigned a,unsigned b,unsigned c,unsigned d){
        const int32_t factor=int32_t(c&511)-((c&256)?512:0);
        const int32_t fixed=(extend(a)-extend(b))*factor+extend(d)*256+128;
        return uint32_t(fixed)&131071;
    }
    static unsigned alpha_equation(unsigned a,unsigned b,unsigned c,unsigned d){
        return rgb_equation(a,b,c,d)/256;
    }
    static unsigned clamp(unsigned encoded){
        encoded&=511;
        return encoded>=384?0:encoded>=256?255:encoded;
    }
    uint64_t combine=0,primitive=0;
    uint32_t environment=0;
    struct Cycle {
        unsigned rgb_a,rgb_b,rgb_c,rgb_d,alpha_a,alpha_b,alpha_c,alpha_d;
    };
    Cycle cycle(unsigned n) const {
        if(n==0)return {field(52,15),field(28,15),field(47,31),field(15,7),field(44,7),field(12,7),field(41,7),field(9,7)};
        return {field(37,15),field(24,15),field(32,31),field(6,7),field(21,7),field(3,7),field(18,7),field(0,7)};
    }
    unsigned minimum_lod() const{return unsigned(primitive>>40)&31;}
    unsigned lod_fraction() const{return unsigned(primitive>>32)&255;}
    uint32_t primitive_color() const{return uint32_t(primitive);}
    bool command(unsigned op,uint64_t word){
        switch(op){
        case 58:primitive=word&0x00001fffffffffffull;return true;
        case 59:environment=uint32_t(word);return true;
        case 60:combine=word&0x00ffffffffffffffull;return true;
        default:return false;
        }
    }
    using State=std::array<uint64_t,3>;
    State snapshot() const{return {combine,primitive,environment};}
    bool restore(const State& s){
        if(s[0]>>56 || s[1]>>45 || s[2]>>32)return false;
        combine=s[0];primitive=s[1];environment=uint32_t(s[2]);return true;
    }
private:
    static int32_t extend(unsigned value){value&=511;return int32_t(value)-(value>=384?512:0);}
    unsigned field(unsigned shift,unsigned mask) const{return unsigned(combine>>shift)&mask;}
};
}
