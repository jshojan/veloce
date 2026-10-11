#pragma once
#include "rdp_tmem.hpp"
namespace n64 {
struct RdpTexelQuad;
// Canonical big-endian TMEM reads. YUV returns signed U/V and unconverted Y/A;
// filtering, conversion coefficients, division/LOD and pipeline timing are not
// performed here. Palette lane identifies the physical replicated TLUT bank.
struct RdpTexelFetch {
    using Color=std::array<int32_t,4>;
    static Color read(const RdpTmem& memory,const RdpTexture::Tile& tile,int32_t s,int32_t t){
        return read_row(memory,tile,s,t,tile.address()+tile.line()*(uint32_t(t)&255));
    }
private:
    friend struct RdpTexelQuad;
    static Color read_row(const RdpTmem& memory,const RdpTexture::Tile& tile,int32_t s,int32_t t,uint32_t base){
        const unsigned format=tile.format()<5?tile.format():4,size=tile.size();
        const uint32_t byte=(base*8+uint32_t(s))^((t&1)?4u:0u);
        const uint32_t short_at=(base*4+uint32_t(s))^((t&1)?2u:0u);
        if(format==1){
            if(size<2){unsigned v=memory.bytes[byte&2047];if(!size)v=(v&240)|(v>>4);return {int32_t(v)-128,int32_t(v)-128,int32_t(v),int32_t(v)};}
            const unsigned uv=half(memory,(((base*8+uint32_t(s))>>1)^((t&1)?2u:0u))&1023);
            unsigned y=memory.bytes[(byte&2047)|2048],a=y;
            if(size==3 && !(s&1)){const unsigned pair=half(memory,((((base*8+uint32_t(s))>>1)^((t&1)?2u:0u))&1023)|1024);y=pair>>8;a=(y&15)|(pair&240);}
            return {int32_t(uv>>8)-128,int32_t(uv&255)-128,int32_t(y),int32_t(a)};
        }
        if(!size){
            const unsigned address=(((base*16+uint32_t(s))>>1)^((t&1)?4u:0u))&4095;
            const unsigned packed=memory.bytes[address],v=(s&1)?packed&15:packed>>4;
            if(format==2)return mono((tile.palette()<<4)|v);
            if(format==3){const unsigned i=v>>1;return {int32_t((i<<5)|(i<<2)|(i>>1)),int32_t((i<<5)|(i<<2)|(i>>1)),int32_t((i<<5)|(i<<2)|(i>>1)),(v&1)?255:0};}
            return mono(v*17);
        }
        if(size==1){const unsigned v=memory.bytes[byte&4095];if(format==3)return {int32_t((v>>4)*17),int32_t((v>>4)*17),int32_t((v>>4)*17),int32_t((v&15)*17)};return mono(v);}
        const unsigned v=half(memory,short_at&2047);
        if(format==0){if(size==2)return rgba16(v);const unsigned rg=half(memory,short_at&1023),ba=half(memory,(short_at&1023)|1024);return {int32_t(rg>>8),int32_t(rg&255),int32_t(ba>>8),int32_t(ba&255)};}
        if(format==3 && size==2)return ia16(v);
        return {int32_t(v>>8),int32_t(v&255),int32_t(v>>8),int32_t(v&255)};
    }
public:
    static unsigned palette_index(const RdpTmem& memory,const RdpTexture::Tile& tile,int32_t s,int32_t t){
        return index_row(memory,tile,s,t,tile.address()+tile.line()*(uint32_t(t)&255));
    }
private:
    static unsigned index_row(const RdpTmem& memory,const RdpTexture::Tile& tile,int32_t s,int32_t t,uint32_t base){
        const unsigned kind=tile.format()<5?(tile.format()+2)&3:2;
        const unsigned byte=(base*8+uint32_t(s))^((t&1)?4u:0u);
        if(!tile.size()){
            unsigned nibble;
            if(kind==3)nibble=memory.bytes[byte&2047]>>4;
            else{const unsigned address=(((base*16+uint32_t(s))>>1)^((t&1)?4u:0u))&2047;const unsigned v=memory.bytes[address];nibble=(s&1)?v&15:v>>4;}
            return (tile.palette()<<4)|nibble;
        }
        if(tile.size()==1 || kind==3)return memory.bytes[byte&2047];
        return half(memory,((base*4+uint32_t(s))^((t&1)?2u:0u))&1023)>>8;
    }
public:
    static Color palette(const RdpTmem& memory,unsigned index,unsigned lane,bool intensity_alpha){
        const unsigned v=half(memory,1024+((index&255)*4)+(lane&3));return intensity_alpha?ia16(v):rgba16(v);
    }
private:
    static unsigned half(const RdpTmem& memory,unsigned index){return unsigned(memory.bytes[index*2])*256+memory.bytes[index*2+1];}
    static Color mono(unsigned v){return {int32_t(v),int32_t(v),int32_t(v),int32_t(v)};}
    static Color ia16(unsigned v){return {int32_t(v>>8),int32_t(v>>8),int32_t(v>>8),int32_t(v&255)};}
    static Color rgba16(unsigned v){const unsigned r=v>>11,g=(v>>6)&31,b=(v>>1)&31;return {int32_t(r*8+r/4),int32_t(g*8+g/4),int32_t(b*8+b/4),(v&1)?255:0};}
};
}
