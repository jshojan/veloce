#pragma once
#include "rdp_triangle_span.hpp"
#include "rdp_fill.hpp"
namespace n64 {
// Fill-cycle span bounds. Shade/texture/Z attributes do not select fill bytes.
// Paired/byte metadata follows span traversal. Crash writes and timing are pending.
struct RdpTriangleFill {
    struct Span {unsigned left=0,right=0;bool valid=false;};
    using Clipped=RdpTriangleSpan::Clipped;
    static Clipped clip(uint32_t x,unsigned left,unsigned right){return RdpTriangleSpan::clip(x,left,right);}
    static std::array<Span,1024> spans(const RdpTriangle& t,uint64_t scissor){
        std::array<Span,1024> result{};
        const auto bounds=RdpTriangleSpan::bounds(t,scissor);if(!bounds.valid)return result;
        for(unsigned row=bounds.first;row<=bounds.last;++row){
            const auto geometry=RdpTriangleSpan::row(t,bounds,row);
            if(geometry.valid)result[row]={geometry.first,geometry.last,true};
        }
        return result;
    }
    static unsigned draw(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,uint32_t color,std::vector<uint8_t>& ram,HiddenRdram* hidden=nullptr){
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
        if(hidden && hidden->size()!=ram.size())return 5;
        if((mode&0x50) || ((mode&0x20) && !(mode&4)))return 3;
        for(unsigned y=0;y<rows.size();++y)if(rows[y].valid)for(unsigned step=0;step<=rows[y].right-rows[y].left;++step){
            const unsigned x=triangle.flip?rows[y].left+step:rows[y].right-step;
            const size_t at=origin+(size_t(y)*width+x)*bytes;
            RdpFill::store(at,bytes,color,ram,hidden,triangle.flip);
        }
        if(hidden && bytes==1)hidden->finish_rdp_bytes();
        return 0;
    }
};
}
