#pragma once
#include "rdp_packet.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace n64 {
// Packet fields only. Raw 32-bit fixed-point coefficients remain bit vectors;
// rasterizer truncation, interpolation, coverage and cycle timing are separate.
// SGI RDP Command Summary v2 tables 11-15, pp.12-20.
struct RdpTriangle {
    bool shade=false,texture=false,depth=false,flip=false;
    uint8_t level=0,tile=0;
    // Low, middle, high Y, signed quarter-pixel coordinates.
    std::array<int32_t,3> y{};
    // Low, high, middle edges: initial X and dX/dY, raw 16.16 words.
    std::array<std::array<uint32_t,2>,3> edge{};
    // Initial, d/dX, d/dEdge, d/dY groups, RGBA or STW channels.
    std::array<std::array<uint32_t,4>,4> color{};
    std::array<std::array<uint32_t,3>,4> tex{};
    std::array<uint32_t,4> z{};
    static bool decode(const RdpPacket& packet,RdpTriangle& out){
        const unsigned op=unsigned(packet.words[0]>>56)&63;
        if(op<8 || op>15 || packet.count!=RdpPacket::length(op) || packet.needed!=packet.count)return false;
        RdpTriangle value;
        value.shade=op&4;value.texture=op&2;value.depth=op&1;
        const uint64_t head=packet.words[0];
        value.flip=(head>>55)&1;value.level=(head>>51)&7;value.tile=(head>>48)&7;
        for(unsigned n=0;n<3;++n){const uint32_t raw=uint32_t(head>>(32-16*n))&0x3fff;value.y[n]=raw&0x2000?int32_t(raw)-0x4000:int32_t(raw);}
        for(unsigned n=0;n<3;++n){value.edge[n][0]=uint32_t(packet.words[n+1]>>32);value.edge[n][1]=uint32_t(packet.words[n+1]);}
        unsigned cursor=4;
        if(value.shade){gradients(packet,cursor,value.color);cursor+=8;}
        if(value.texture){gradients(packet,cursor,value.tex);cursor+=8;}
        if(value.depth){for(unsigned n=0;n<4;++n)value.z[n]=uint32_t(packet.words[cursor+n/2]>>(n%2?0:32));}
        out=value;return true;
    }
private:
    template<std::size_t channels>
    static void gradients(const RdpPacket& packet,unsigned offset,std::array<std::array<uint32_t,channels>,4>& out){
        for(unsigned group=0;group<4;++group){
            const unsigned integer=offset+(group/2)*4+group%2;
            for(unsigned lane=0;lane<channels;++lane){
                const unsigned shift=48-lane*16;
                out[group][lane]=(uint32_t((packet.words[integer]>>shift)&65535)<<16)|uint32_t((packet.words[integer+2]>>shift)&65535);
            }
        }
    }
};
}
