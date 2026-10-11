#pragma once
#include "rdp_triangle_attributes.hpp"
#include <vector>
namespace n64 {
// First one-cycle pixel path: RGBA32, no AA/blending, key, alpha compare,
// dither, Z accesses, texture fetch or previous-pixel combiner feedback.
// Unsupported profiles reject before any framebuffer write. Hidden RDRAM
// bits and physical pipeline/crash timing remain separate work.
struct RdpTriangleShade {
    static unsigned draw(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,const RdpCombiner& combiner,std::vector<uint8_t>& ram){
        const uint64_t dither=(3ull<<38)|(3ull<<36);
        const uint64_t allowed=dither|0xffff0000ull|0x340ull;
        if((mode&~allowed) || (mode&dither)!=dither || ((mode>>30)&3) || !combiner.independent_one_cycle())return 1;
        if(((image>>53)&7)!=0 || ((image>>51)&3)!=3)return 1;
        const uint32_t origin=uint32_t(image)&0x3ffffff;
        if(origin&7)return 4;
        RdpTriangle triangle;if(!RdpTriangle::decode(packet,triangle))return 1;
        const unsigned width=(unsigned(image>>32)&1023)+1;
        const auto bounds=RdpTriangleSpan::bounds(triangle,scissor);
        if(!bounds.valid)return 0;
        // Preflight every complete row so functional bounds rejection is atomic.
        for(unsigned y=bounds.first;y<=bounds.last;++y){const auto row=RdpTriangleSpan::row(triangle,bounds,y);if(row.valid && uint64_t(origin)+(uint64_t(y)*width+row.last+1)*4>ram.size())return 5;}
        for(unsigned y=bounds.first;y<=bounds.last;++y){
            const auto row=RdpTriangleSpan::row(triangle,bounds,y);if(!row.valid)continue;
            RdpTriangleAttributes::Row attributes;
            if(!RdpTriangleAttributes::row(triangle,y,triangle.flip?row.first:row.last,attributes))return 1;
            // Traversal follows the hardware major-edge direction, including
            // framebuffer row overlap when width is smaller than the span.
            for(unsigned step=0;step<=row.last-row.first;++step){
                const unsigned x=triangle.flip?row.first+step:row.last-step;
                const uint8_t mask=RdpCoverage::mask(row,x);
                // With AA disabled the upper-left coverage bit gates stores.
                if(!(mask&128))continue;
                RdpTriangleAttributes::Pixel pixel;RdpTriangleAttributes::pixel(attributes,x,mask,pixel);
                RdpCombiner::Sources sources;
                for(unsigned lane=0;lane<4;++lane)sources.shade[lane]=pixel.rgba[lane];
                const auto color=combiner.evaluate(1,sources).clamped;
                const size_t at=origin+(size_t(y)*width+x)*4;
                const unsigned memory=(mode&64)?ram[at+3]>>5:7;
                const unsigned cvg=RdpCoverage::stored(unsigned(mode>>8)&3,false,RdpCoverage::derivatives(mask).count,memory);
                for(unsigned lane=0;lane<3;++lane)ram[at+lane]=uint8_t(color[lane]);
                ram[at+3]=uint8_t(cvg<<5);
            }
        }
        return 0;
    }
};
}
