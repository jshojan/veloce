#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "rdp_texture.hpp"
namespace n64 {
// Functional RDP texture transfers. Storage is canonical big-endian TMEM bytes.
// Full eight-byte transfer groups, bank placement and odd-row interleave are
// explicit; command counters do not model the physical memory pipeline.
struct RdpTmem {
    std::array<uint8_t,4096> bytes{};
    unsigned load(unsigned op,uint64_t word,RdpTexture& texture,const std::vector<uint8_t>& ram,uint32_t& groups){
        groups=0;
        if(op!=48 && op!=51 && op!=52)return 1;
        const unsigned source_size=texture.size(),sl=unsigned(word>>44)&4095,tl=unsigned(word>>32)&4095;
        const unsigned sh=unsigned(word>>12)&4095,last=unsigned(word)&4095;
        const bool palette=op==48,block=op==51;
        // Four-bit source images and multi-row palette loads crash
        // the physical load pipeline. Block these profiles before writes here;
        // partial crash effects and timing are not yet implemented.
        if(!source_size || (palette && (last>>2)>(tl>>2)))return 3;
        auto& tile=texture.tiles[(word>>24)&7];
        const unsigned left=block?sl:sl>>2,top=block?tl&1023:tl>>2;
        const unsigned right=block?sh:sh>>2,bottom=block?top:last>>2;
        const unsigned count=(right-left+1)&4095;
        tile.bounds=word&RdpTexture::bounds_mask;
        if(bottom<top || !count)return 0;
        const bool replicate=palette && source_size==2;
        const unsigned advance=replicate?1:16u>>source_size;
        for(unsigned row=top;row<=bottom;++row){
            const int32_t source_left=block && (left&2048)?int32_t(left)-4096:int32_t(left);
            uint32_t source=texture.address()+uint32_t(int32_t(row*texture.width())+source_left)*(1u<<(source_size-1));
            for(unsigned x=0,n=0;x<count;x+=advance,++n){
                const unsigned s=n*(palette?64u>>source_size:16u>>source_size);
                const unsigned t=block?(n*last)>>11:row-top;
                std::array<uint8_t,8> data{};
                if(replicate){const uint8_t a=read(ram,source),b=read(ram,source+1);for(unsigned i=0;i<4;++i){data[2*i]=a;data[2*i+1]=b;}source+=2;}
                else{for(unsigned i=0;i<8;++i)data[i]=read(ram,source+i);source+=8;}
                transfer(tile,s,t,data);++groups;
            }
        }
        return 0;
    }
private:
    static uint8_t read(const std::vector<uint8_t>& ram,uint32_t at){at&=0xffffff;return at<ram.size()?ram[at]:0;}
    void half(unsigned at,uint16_t v){at=(at&2047)*2;bytes[at]=uint8_t(v>>8);bytes[at+1]=uint8_t(v);}
    void transfer(const RdpTexture::Tile& tile,unsigned s,unsigned t,const std::array<uint8_t,8>& data){
        const unsigned short_s=(tile.format()==1 || tile.size()==1)?s/2:tile.size()>=2?s:s/4;
        const unsigned base=((tile.address()+((tile.line()*t)&511))*4+(short_s&2047))&2045;
        std::array<unsigned,4> bank{};
        for(unsigned i=0;i<4;++i){const unsigned at=((base+i)&2047)^((t&1)?2:0);bank[at&3]=at&1023;}
        const bool split=tile.format()==1 || (tile.format()==0 && tile.size()==3);
        if(split){
            const unsigned pair=(((short_s&2)!=0)^(t&1))?2:0;
            for(unsigned i=0;i<2;++i){uint16_t low,high;
                if(tile.format()==1){low=uint16_t(data[4*i])*256+data[4*i+2];high=uint16_t(data[4*i+1])*256+data[4*i+3];}
                else{low=uint16_t(data[4*i])*256+data[4*i+1];high=uint16_t(data[4*i+2])*256+data[4*i+3];}
                half(bank[pair+i],low);half(bank[pair+i]|1024,high);
            }
        }else for(unsigned i=0;i<4;++i){const unsigned source=(i^((t&1)?2:0))*2;half(bank[i]|(base&1024),uint16_t(data[source])*256+data[source+1]);}
    }
};
}
