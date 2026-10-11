#pragma once
#include "rdp_tmem.hpp"
#include <algorithm>
namespace n64 {
// A copy fetch is a bank-coupled 64-bit transfer, not four scalar color samples.
// Tile upper/clamp bounds and filtering do not participate in this path.
struct RdpTextureCopy {
    static uint64_t fetch(const RdpTmem& memory,const RdpTexture::Tile& tile,uint16_t s,uint16_t t,bool palette){
        const int32_t origin=coordinate(tile,false,s),row=masked(tile,true,coordinate(tile,true,t));
        std::array<uint32_t,4> offset{};
        const unsigned stride=tile.size()==1 || tile.format()==1?2:tile.size()>=2?4:1;
        for(unsigned lane=0;lane<4;++lane)offset[lane]=(uint32_t(masked(tile,false,origin+int32_t(lane)))*stride)&8191;
        const uint32_t base=(tile.address()+((tile.line()*uint32_t(row))&511))*16;
        std::array<uint32_t,6> address={base+offset[0],base+offset[1],base+offset[1],base+offset[2],base+offset[3],base+offset[3]};
        if(tile.format()==1){address[1]=address[0]+2*(offset[1]-offset[0]);address[4]=address[1]+offset[3]-offset[0];}
        for(auto& at:address)at=(at&8191)^((row&1)?8u:0u);
        constexpr std::array<unsigned,4> lower_order={0,1,3,4},upper_order={0,2,3,5};
        const auto lower=banks(memory,address,lower_order,false);
        auto upper=banks(memory,address,upper_order,true);
        if(palette)for(unsigned lane=0;lane<4;++lane){const unsigned at=address[lower_order[lane]],word=lower[(at>>2)&3];
            const unsigned index=tile.size()==0?(tile.palette()*16+((word>>(4*(3-(at&3))))&15)):(word>>(at&2?0:8))&255;
            upper[lane]=half(memory,1024+index*4+lane);
        }
        std::array<unsigned,4> words{};
        const bool large=tile.format()==1 || (tile.format()==0 && tile.size()==3);
        for(unsigned lane=0;lane<4;++lane){const unsigned at=address[lower_order[lane]];
            words[lane]=palette?upper[lane]:(large || !(at&4096))?lower[(at>>2)&3]:upper[(address[upper_order[lane]]>>2)&3];
        }
        const uint32_t low=words[2]*65536u+words[3];
        uint32_t high=words[0]*65536u+words[1];
        if(!palette && tile.size()!=2){high=0;for(unsigned lane=0;lane<4;++lane)high=(high<<8)|replicate(words[lane],address[lower_order[lane]]&3,tile);}
        return (uint64_t(high)<<32)|low;
    }
private:
    static int32_t signed16(uint16_t raw){return raw<32768?int32_t(raw):int32_t(raw)-65536;}
    static int32_t floor_div(int32_t value,unsigned divisor){return value/int32_t(divisor)-int32_t(value%int32_t(divisor)<0);}
    static int32_t coordinate(const RdpTexture::Tile& tile,bool t,uint16_t raw){
        const unsigned shift=tile.shift(t);
        const int32_t shifted=shift<=10?floor_div(signed16(raw),1u<<shift):signed16(uint16_t(uint32_t(raw)<<(16-shift)));
        return floor_div(shifted-int32_t(tile.low(t)*8),32);
    }
    static int32_t masked(const RdpTexture::Tile& tile,bool t,int32_t value){
        if(!tile.mask(t))return value;
        const unsigned bits=std::min(tile.mask(t),10u);
        if(tile.mirror(t) && ((uint32_t(value)>>bits)&1))value=~value;
        return int32_t(uint32_t(value)&((1u<<bits)-1));
    }
    static unsigned half(const RdpTmem& memory,unsigned index){return unsigned(memory.bytes[index*2])*256+memory.bytes[index*2+1];}
    static std::array<unsigned,4> banks(const RdpTmem& memory,const std::array<uint32_t,6>& address,const std::array<unsigned,4>& order,bool upper){
        std::array<unsigned,4> words{};
        for(unsigned bank=0;bank<4;++bank){unsigned index=0;for(auto lane:order)if(((address[lane]>>2)&3)==bank){index=(address[lane]>>2)&1023;break;}words[bank]=half(memory,index+(upper?1024:0));}
        return words;
    }
    static unsigned replicate(unsigned word,unsigned offset,const RdpTexture::Tile& tile){
        if(tile.size()==0){const unsigned nibble=(word>>(4*(3-offset)))&15;
            if(tile.format()==2)return tile.palette()*16+nibble;
            if(tile.format()==3){const unsigned intensity=nibble>>1;return intensity*32+intensity*4+intensity/2;}
            return nibble*17;
        }
        if(tile.size()==1){const unsigned byte=(word>>(offset&2?0:8))&255;return tile.format()==3?(byte>>4)*17:byte;}
        return word>>8;
    }
};
}
