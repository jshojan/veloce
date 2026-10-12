#pragma once
#include "rdp_texture_coordinates.hpp"
#include "rdp_texture_filter.hpp"
#include "rdp_rgba16_simd.hpp"
namespace n64 {
// Coupled cycle fetch over canonical TMEM. Coordinates are already shifted,
// clamped and masked; fractions determine palette/chroma lane remapping.
// nearest selects four TLUT banks of one index (sample_type=0 + TLUT).
struct RdpTexelQuad {
    using Quad=RdpTextureFilter::Quad;
    enum class Palette {none,rgba,ia};
    static Quad read(const RdpTmem& memory,const RdpTexture::Tile& tile,
                     const RdpTextureCoordinates::Sample& sample,
                     Palette palette=Palette::none,bool nearest=false){
        if constexpr(RdpRgba16Simd::available){
            if(palette==Palette::none && tile.format()==0 && tile.size()==2){
                const uint32_t row0=uint32_t(sample.t.texel)&255;
                const int32_t row1=int32_t(row0)+(sample.t.neighbor-sample.t.texel);
                const uint32_t base0=tile.address()+tile.line()*row0;
                const uint32_t base1=tile.address()+tile.line()*uint32_t(row1);
                // Tile bases/strides are eight-byte units; the gather uses
                // halfword units, odd-row bank swapping and 4 KiB wrapping.
                const auto half=[&](uint32_t base,int32_t row,int32_t s){
                    return uint16_t(RdpTexelFetch::half(memory,((base*4+uint32_t(s))^((row&1)?2u:0u))&2047));
                };
                const std::array<uint16_t,4> words={half(base0,int32_t(row0),sample.s.texel),
                    half(base0,int32_t(row0),sample.s.neighbor),half(base1,row1,sample.s.texel),
                    half(base1,row1,sample.s.neighbor)};
                Quad colors;RdpRgba16Simd::decode(words,colors);return colors;
            }
        }
        return read_scalar(memory,tile,sample,palette,nearest);
    }
    static Quad read_scalar(const RdpTmem& memory,const RdpTexture::Tile& tile,
                     const RdpTextureCoordinates::Sample& sample,
                     Palette palette=Palette::none,bool nearest=false){
        const uint32_t row0=uint32_t(sample.t.texel)&255;
        const int32_t dt=sample.t.neighbor-sample.t.texel;
        const int32_t row1=int32_t(row0)+dt;
        const uint32_t base0=tile.address()+tile.line()*row0;
        const uint32_t base1=tile.address()+tile.line()*uint32_t(row1);
        const int32_t ds=sample.s.neighbor-sample.s.texel;
        const unsigned s=sample.s.fraction&31,t=sample.t.fraction&31;
        const unsigned uv=tile.format()==1?(s>>1)|((sample.s.texel&1)?16u:0u):s;
        const bool upper=s+t>=32,upper_uv=uv+t>=32;
        Quad colors{};
        for(unsigned corner=0;corner<4;++corner){
            const bool right=corner&1,bottom=corner&2;
            const uint32_t base=bottom?base1:base0;
            const int32_t row=bottom?row1:int32_t(row0);
            const int32_t x=right?sample.s.neighbor:sample.s.texel;
            if(palette!=Palette::none){
                const uint32_t origin=nearest?tile.address()+tile.line()*uint32_t(sample.t.texel):base;
                const int32_t index_s=nearest?sample.s.texel:(tile.format()==1 && right?x+ds:x);
                const int32_t index_t=nearest?sample.t.texel:row;
                const unsigned index=RdpTexelFetch::index_row(memory,tile,index_s,index_t,origin);
                colors[corner]=RdpTexelFetch::palette(memory,index,corner^(upper_uv?3u:0u),palette==Palette::ia);
            }else if(tile.format()!=1){
                colors[corner]=RdpTexelFetch::read_row(memory,tile,x,row,base);
            }else{
                const unsigned size=tile.size();
                const uint32_t chroma_s=uint32_t(x)+(right?uint32_t(ds):0u);
                if(size<2){
                    unsigned value=memory.bytes[((base*8+chroma_s)^((row&1)?4u:0u))&2047];
                    if(!size)value=(value>>4)*17;
                    colors[corner]={int32_t(value)-128,int32_t(value)-128,int32_t(value),int32_t(value)};
                }else{
                    const unsigned chroma_at=(((base*8+chroma_s)>>1)^((row&1)?2u:0u))&1023;
                    const unsigned pair=RdpTexelFetch::half(memory,chroma_at);
                    const unsigned luma_at=((base*8+uint32_t(x))^((row&1)?4u:0u))&2047;
                    unsigned y=memory.bytes[luma_at|2048],a=y;
                    if(size==3 && !(x&1)){
                        const unsigned at=right?luma_at>>1:chroma_at;
                        const unsigned packed=RdpTexelFetch::half(memory,at|1024);
                        y=packed>>8;a=(y&15)|(packed&240);
                    }
                    colors[corner]={int32_t(pair>>8)-128,int32_t(pair&255)-128,int32_t(y),int32_t(a)};
                }
            }
        }
        // In YUV4/8 and TLUT modes, B/A traverse the opposite corner order
        // when the chroma and luma triangles disagree; R/G keep their order.
        if(upper!=upper_uv && (palette!=Palette::none || (tile.format()==1 && tile.size()<2))){
            for(unsigned corner=0;corner<2;++corner)for(unsigned lane=2;lane<4;++lane)
                std::swap(colors[corner][lane],colors[3-corner][lane]);
        }
        return colors;
    }
};
}
