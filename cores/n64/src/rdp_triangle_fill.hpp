#pragma once
#include "rdp_triangle_edges.hpp"
#include "rdp_fill.hpp"
namespace n64 {
// Fill-cycle span bounds. Shade/texture/Z attributes do not select fill bytes.
// Hidden coverage bits, crash writes and physical raster timing remain pending.
struct RdpTriangleFill {
    struct Span {unsigned left=0,right=0;bool valid=false;};
    struct Clipped {unsigned x=0;bool under=false,over=false;};
    static Clipped clip(uint32_t x,unsigned left,unsigned right){
        const unsigned sticky=bool((x>>1)&8191);
        unsigned value=((x>>13)&8190)|sticky;
        const bool under=(x&0x8000000) || (value<left*2 && !(x&0x4000000));
        value=under?left*2:((x>>13)&16382)|sticky;
        const bool over=(value&8192) || (value&8191)>=right*2;
        return {(over?right*2:value)&8191,under,over};
    }
    static std::array<Span,1024> spans(const RdpTriangle& t,uint64_t scissor){
        std::array<Span,1024> result{};
        const unsigned sl=unsigned(scissor>>44)&4095,sr=unsigned(scissor>>12)&4095;
        const int32_t top=std::max(t.y[2],int32_t((scissor>>32)&4095));
        const int32_t bottom=std::min(t.y[0],int32_t(scissor&4095));
        if(sl>=sr || top>=bottom)return result;
        const bool field=bool(scissor&(1ull<<25));const unsigned odd=unsigned(scissor>>24)&1;
        for(unsigned row=unsigned(top)/4;row<=unsigned(bottom-1)/4;++row){
            if(field && (row&1)!=odd)continue;
            unsigned low=4095,high=0;bool any=false,allover=true,allunder=true;
            for(unsigned sub=0;sub<4;++sub){
                const int32_t y=int32_t(row*4+sub);RdpTriangleEdges::Sample sample;
                if(!RdpTriangleEdges::sample(t,y,sample))continue;
                const auto major=clip(sample.major,sl,sr),minor=clip(sample.minor,sl,sr);
                allover=allover && major.over && minor.over;
                allunder=allunder && major.under && minor.under;
                const uint32_t major_order=(sample.major^0x8000000)&0xfffc000;
                const uint32_t minor_order=(sample.minor^0x8000000)&0xfffc000;
                const bool crossed=t.flip?minor_order<major_order:major_order<minor_order;
                if(y<top || y>=bottom || crossed)continue;
                any=true;
                low=std::min(low,(t.flip?major.x:minor.x)/8);
                high=std::max(high,(t.flip?minor.x:major.x)/8);
            }
            if(any && !allover && !allunder && low<=high)result[row]={low,high,true};
        }
        return result;
    }
    static unsigned draw(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,uint32_t color,std::vector<uint8_t>& ram){
        RdpTriangle triangle;if(!RdpTriangle::decode(packet,triangle))return 1;
        const unsigned size=unsigned(image>>51)&3,bytes=size?1u<<(size-1):0;
        const unsigned width=(unsigned(image>>32)&1023)+1;
        const uint32_t origin=uint32_t(image)&0x3ffffff;
        if(!size || ((mode>>52)&3)!=3)return 3;
        if(origin&7)return 4;
        const auto rows=spans(triangle,scissor);bool any=false;
        for(unsigned y=0;y<rows.size();++y)if(rows[y].valid){
            any=true;
            if(uint64_t(origin)+(uint64_t(y)*width+rows[y].right+1)*bytes>ram.size())return 5;
        }
        if(!any)return 0;
        if((mode&0x50) || ((mode&0x20) && !(mode&4)))return 3;
        for(unsigned y=0;y<rows.size();++y)if(rows[y].valid)for(unsigned x=rows[y].left;x<=rows[y].right;++x){
            const size_t at=origin+(size_t(y)*width+x)*bytes;
            if(bytes==1)ram[at]=uint8_t(color>>(8*(3-(at&3))));
            else if(bytes==2){const uint32_t value=(at&2)?color&65535:color>>16;ram[at]=uint8_t(value>>8);ram[at+1]=uint8_t(value);}
            else for(unsigned n=0;n<4;++n)ram[at+n]=uint8_t(color>>(8*(3-n)));
        }
        return 0;
    }
};
}
