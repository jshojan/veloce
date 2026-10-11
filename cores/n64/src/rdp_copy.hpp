#pragma once
#include "rdp_rectangle.hpp"
#include "rdp_triangle_span.hpp"
#include "rdp_texture_copy.hpp"
#include "rdp_texture_divider.hpp"
#include "hidden_rdram.hpp"
#include <vector>
namespace n64 {
// Functional copy-cycle burst writes. No combiner, blender, color conversion,
// coverage/depth tests or RGB dither. Physical DMA/crash timing remains separate.
struct RdpCopy {
    static unsigned rectangle(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,const RdpTexture& texture,const RdpTmem& tmem,unsigned threshold,std::vector<uint8_t>& ram,HiddenRdram& hidden){
        RdpTriangle triangle;if(!RdpRectangle::decode(packet,true,triangle))return 1;
        return draw(triangle,image,mode,scissor,texture,tmem,threshold,ram,hidden);
    }
    static unsigned triangle(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,const RdpTexture& texture,const RdpTmem& tmem,unsigned threshold,std::vector<uint8_t>& ram,HiddenRdram& hidden){
        RdpTriangle triangle;if(!RdpTriangle::decode(packet,triangle))return 1;
        return draw(triangle,image,mode,scissor,texture,tmem,threshold,ram,hidden);
    }
    static unsigned draw(const RdpTriangle& triangle,uint64_t image,uint64_t mode,uint64_t scissor,const RdpTexture& texture,const RdpTmem& tmem,unsigned threshold,std::vector<uint8_t>& ram,HiddenRdram& hidden){
        const unsigned size=unsigned(image>>51)&3,bytes=size?1u<<(size-1):1;
        if(((mode>>52)&3)!=2 || size==3 || (size==1 && (mode&3)==3))return 1; // Random CI8 alpha threshold requires RNG state.
        const uint32_t origin=uint32_t(image)&0x3ffffff;if(origin&7)return 4;
        if(hidden.size()!=ram.size())return 5;
        const unsigned width=(unsigned(image>>32)&1023)+1,advance=size?16u>>size:8;
        const auto bounds=RdpTriangleSpan::bounds(triangle,scissor);if(!bounds.valid){if(size==1)hidden.reset_byte_raster_history();hidden.discard_rdp_bytes();return 0;}
        for(unsigned y=bounds.first;y<=bounds.last;++y){const auto span=RdpTriangleSpan::row(triangle,bounds,y);if(!span.valid)continue;
            const int64_t first=int64_t(origin)+(int64_t(y)*width+span.first)*bytes-(triangle.flip?0:bytes-1);
            const uint64_t end=uint64_t(origin)+(uint64_t(y)*width+span.last)*bytes+(triangle.flip?bytes:1);
            if(first<0 || end>ram.size())return 5;
        }
        if(size==1)hidden.reset_byte_raster_history();
        hidden.discard_rdp_bytes();
        for(unsigned y=bounds.first;y<=bounds.last;++y){const auto span=RdpTriangleSpan::row(triangle,bounds,y);if(!span.valid)continue;
            std::array<uint32_t,3> value{},step{};attributes(triangle,y,value,step);
            const unsigned first=triangle.flip?span.first:span.last;
            int64_t destination=int64_t(origin)+(int64_t(y)*width+first)*bytes;
            unsigned remaining=(span.last-span.first+1)*bytes;
            for(unsigned pixel=0;pixel<=span.last-span.first;pixel+=advance){
                const auto divided=divide(value,mode);unsigned tile=triangle.tile;
                if(mode&(1ull<<48))tile=lod(triangle,value,step,mode);
                const uint64_t fetched=RdpTextureCopy::fetch(tmem,texture.tiles[tile],RdpTextureDivider::saturate(divided.s),RdpTextureDivider::saturate(divided.t),mode&(1ull<<47));
                const uint64_t data=size?fetched:0;unsigned mask=255;
                if(mode&1){mask=0;for(unsigned lane=0;lane<4;++lane){const bool accepted=size==2?bool((data>>(48-lane*16))&1):size==1?((data>>(24-lane*8))&255)>=(threshold&255):false;if(accepted)mask|=3u<<(6-lane*2);}}
                const unsigned count=std::min(8u,remaining);
                for(unsigned lane=0;lane<count;++lane)if(mask&(128u>>lane)){
                    const auto at=size_t(destination+(triangle.flip?int64_t(lane):-int64_t(lane)));ram[at]=uint8_t(data>>(56-lane*8));hidden.rdp_byte(at,ram[at],triangle.flip);
                }
                remaining-=count;destination+=triangle.flip?8:-8;for(unsigned lane=0;lane<3;++lane)value[lane]+=step[lane];
            }
        }
        if(size==1)hidden.finish_rdp_bytes();else hidden.discard_rdp_bytes();
        return 0;
    }
private:
    static int64_t signed32(uint32_t raw){return raw<0x80000000u?int64_t(raw):int64_t(raw)-0x100000000ll;}
    static int64_t floor_div(int64_t value,unsigned divisor){return value/int64_t(divisor)-int64_t(value%int64_t(divisor)<0);}
    static void attributes(const RdpTriangle& triangle,unsigned y,std::array<uint32_t,3>& value,std::array<uint32_t,3>& step){
        const int32_t top=int32_t(floor_div(triangle.y[2],4));const bool offset=bool(triangle.edge[1][1]&0x80000000u)==triangle.flip;
        for(unsigned lane=0;lane<3;++lane){const uint32_t de=triangle.tex[2][lane]&~511u,dy=triangle.tex[3][lane]&~511u;
            const uint32_t difference=offset?de-uint32_t(floor_div(signed32(de),4))-dy+uint32_t(floor_div(signed32(dy),4)):0;
            value[lane]=((triangle.tex[0][lane]+uint32_t(uint64_t(triangle.tex[2][lane])*uint32_t(int32_t(y)-top)))&~511u)+difference;value[lane]&=~1023u;
            step[lane]=triangle.tex[1][lane]&~31u;if(!triangle.flip)step[lane]=0-step[lane];
        }
    }
    static RdpTextureDivider::Pair divide(const std::array<uint32_t,3>& value,uint64_t mode){return RdpTextureDivider::divide(uint16_t(value[0]>>16),uint16_t(value[1]>>16),uint16_t(value[2]>>16),mode&(1ull<<51));}
    static unsigned lod(const RdpTriangle& triangle,const std::array<uint32_t,3>& value,const std::array<uint32_t,3>& step,uint64_t mode){
        auto next=value,far=value;for(unsigned lane=0;lane<3;++lane){next[lane]+=step[lane];far[lane]+=step[lane]*2;}
        const auto a=divide(next,mode),b=divide(far,mode);unsigned level=triangle.level;bool magnify=false;
        if(!((a.s|a.t|b.s|b.t)&393216)){
            auto signed17=[](uint32_t raw){return int32_t(raw&131071)-((raw&65536)?131072:0);};
            const int32_t ds=signed17(b.s)-signed17(a.s),dt=signed17(b.t)-signed17(a.t);
            const unsigned delta=std::max(ds<0?unsigned(~ds)&131071:unsigned(ds),dt<0?unsigned(~dt)&131071:unsigned(dt));
            const unsigned encoded=(delta&32767)|((delta&114688)?16384:0);
            if(!(encoded&16384)){magnify=encoded<32;level=0;if(!magnify){unsigned scale=(encoded>>5)&255;while(scale>>=1)++level;if(!triangle.level || (encoded&24576) || level>=triangle.level)level=triangle.level;}}
        }
        return (triangle.tile+level+((mode&(1ull<<50)) && !magnify?1:0))&7;
    }
};
}
