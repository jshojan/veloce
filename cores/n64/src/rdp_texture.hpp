#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// SGI RDP Command Summary tables 3, 6 and 9. Descriptor/image state only;
// TMEM loads, coordinate processing and texel sampling are separate operations.
struct RdpTexture {
    static constexpr uint64_t descriptor_mask=0x00fbffff00ffffffull;
    static constexpr uint64_t bounds_mask=0x00ffffff00ffffffull;
    static constexpr uint64_t image_mask=0x00f803ff03ffffffull;
    struct Tile {
        uint64_t descriptor=0,bounds=0;
        unsigned format() const{return unsigned(descriptor>>53)&7;}
        unsigned size() const{return unsigned(descriptor>>51)&3;}
        unsigned line() const{return unsigned(descriptor>>41)&511;}
        unsigned address() const{return unsigned(descriptor>>32)&511;}
        unsigned palette() const{return unsigned(descriptor>>20)&15;}
        unsigned clamp(bool t) const{return unsigned(descriptor>>(t?19:9))&1;}
        unsigned mirror(bool t) const{return unsigned(descriptor>>(t?18:8))&1;}
        unsigned mask(bool t) const{return unsigned(descriptor>>(t?14:4))&15;}
        unsigned shift(bool t) const{return unsigned(descriptor>>(t?10:0))&15;}
        unsigned low(bool t) const{return unsigned(bounds>>(t?32:44))&4095;}
        unsigned high(bool t) const{return unsigned(bounds>>(t?0:12))&4095;}
    };
    std::array<Tile,8> tiles{};
    uint64_t image=0;
    unsigned format() const{return unsigned(image>>53)&7;}
    unsigned size() const{return unsigned(image>>51)&3;}
    unsigned width() const{return (unsigned(image>>32)&1023)+1;}
    uint32_t address() const{return uint32_t(image)&0x3ffffff;}
    bool command(unsigned op,uint64_t word){
        switch(op){
        case 53:tiles[(word>>24)&7].descriptor=word&descriptor_mask;return true;
        case 50:tiles[(word>>24)&7].bounds=word&bounds_mask;return true;
        case 61:image=word&image_mask;return true;
        default:return false;
        }
    }
    using State=std::array<uint64_t,17>;
    State snapshot() const{State s{};for(unsigned i=0;i<8;++i){s[2*i]=tiles[i].descriptor;s[2*i+1]=tiles[i].bounds;}s[16]=image;return s;}
    bool restore(const State& s){
        for(unsigned i=0;i<16;++i)if(s[i]&~(i&1?bounds_mask:descriptor_mask))return false;
        if(s[16]&~image_mask)return false;
        for(unsigned i=0;i<8;++i){tiles[i].descriptor=s[2*i];tiles[i].bounds=s[2*i+1];}image=s[16];return true;
    }
};
}
