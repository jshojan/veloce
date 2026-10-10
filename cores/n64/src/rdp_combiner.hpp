#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// SGI RDP Command Summary, tables 19, 21 and 22. This is command state,
// not a color-combiner evaluator or a textured/triangle rasterizer.
struct RdpCombiner {
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
    unsigned field(unsigned shift,unsigned mask) const{return unsigned(combine>>shift)&mask;}
};
}
