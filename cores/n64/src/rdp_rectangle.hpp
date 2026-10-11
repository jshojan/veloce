#pragma once
#include "rdp_triangle.hpp"
namespace n64 {
// Native rectangle fields expressed as constant edges and ST/W interpolants.
// The flip command exchanges derivatives; raster traversal still runs left to right.
struct RdpRectangle {
    static bool decode(const RdpPacket& packet,bool inclusive,RdpTriangle& out){
        if(packet.count!=2 || packet.needed!=2)return false;
        const unsigned op=unsigned(packet.words[0]>>56)&63;if(op!=36 && op!=37)return false;
        const auto word=packet.words[0],coordinates=packet.words[1];
        const unsigned left=unsigned(word>>12)&4095,right=unsigned(word>>44)&4095;
        const unsigned top=unsigned(word)&4095,bottom=(unsigned(word>>32)&4095)|(inclusive?3u:0u);
        RdpTriangle value;value.texture=value.flip=true;value.tile=uint8_t(word>>24)&7;
        value.y={int32_t(bottom),int32_t(bottom),int32_t(top)};
        value.edge[0][0]=value.edge[2][0]=right*16384;value.edge[1][0]=left*16384;
        value.tex[0]={uint32_t(coordinates>>48)<<16,(uint32_t(coordinates>>32)&65535)<<16,0};
        auto signed16=[](uint16_t raw){return raw<32768?int32_t(raw):int32_t(raw)-65536;};
        const uint32_t ds=uint32_t(int64_t(signed16(uint16_t(coordinates>>16)))*2048),dt=uint32_t(int64_t(signed16(uint16_t(coordinates)))*2048);
        if(op==36){value.tex[1][0]=ds;value.tex[2][1]=value.tex[3][1]=dt;}
        else{value.tex[1][1]=dt;value.tex[2][0]=value.tex[3][0]=ds;}
        out=value;return true;
    }
};
}
