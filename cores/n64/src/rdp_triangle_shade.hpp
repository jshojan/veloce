#pragma once
#include "rdp_triangle_attributes.hpp"
#include <vector>
#include "rdp_texel_quad.hpp"
#include "rdp_convert.hpp"
#include "rdp_texture_divider.hpp"
#include "rdp_blender.hpp"
#include "rdp_framebuffer.hpp"
namespace n64 {
// Functional one-cycle RGBA16/RGBA32: optional blending/AA/alpha-coverage decisions.
// No key, dither, Z accesses or previous-pixel combiner feedback. Affine/perspective
// texture sampling supports cycle-zero filtering/passthrough, color conversion and
// optional TLUT. SetConvert also supplies K4/K5 combiner inputs.
// Unsupported profiles reject before any framebuffer write. Physical pipeline/crash timing remains separate work.
struct RdpTriangleShade {
    static unsigned draw(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,const RdpCombiner& combiner,std::vector<uint8_t>& ram,const RdpTexture* texture=nullptr,const RdpTmem* tmem=nullptr,const RdpConvert* conversion=nullptr,RdpBlender* blender=nullptr,HiddenRdram* hidden=nullptr){
        RdpTriangle triangle;if(!RdpTriangle::decode(packet,triangle))return 1;
        const bool textured=triangle.texture && texture && tmem;
        const uint64_t sampling=textured?((63ull<<42)|(1ull<<51)):0;
        const uint64_t dither=(3ull<<38)|(3ull<<36);
        const uint64_t allowed=dither|0xffff0000ull|0x340ull|sampling|(blender?0x7089ull:0);
        if((mode&~allowed) || (mode&dither)!=dither || (!blender && ((mode>>30)&3)) || !combiner.independent_one_cycle(textured,conversion!=nullptr) || (textured && !(mode&(1ull<<43)) && !conversion))return 1;
        const unsigned size=unsigned(image>>51)&3,bytes=size==2?2:4;
        if(((image>>53)&7)!=0 || (size!=3 && !(size==2 && hidden && blender)))return 1;
        if(hidden && hidden->size()!=ram.size())return 5;
        const uint32_t origin=uint32_t(image)&0x3ffffff;
        if(origin&7)return 4;
        const unsigned width=(unsigned(image>>32)&1023)+1;
        const auto bounds=RdpTriangleSpan::bounds(triangle,scissor);
        if(!bounds.valid)return 0;
        // Preflight every complete row so functional bounds rejection is atomic.
        for(unsigned y=bounds.first;y<=bounds.last;++y){const auto row=RdpTriangleSpan::row(triangle,bounds,y);if(row.valid && uint64_t(origin)+(uint64_t(y)*width+row.last+1)*bytes>ram.size())return 5;}
        for(unsigned y=bounds.first;y<=bounds.last;++y){
            const auto row=RdpTriangleSpan::row(triangle,bounds,y);if(!row.valid)continue;
            RdpTriangleAttributes::Row attributes;
            if(!RdpTriangleAttributes::row(triangle,y,triangle.flip?row.first:row.last,attributes))return 1;
            // Traversal follows the hardware major-edge direction, including
            // framebuffer row overlap when width is smaller than the span.
            for(unsigned step=0;step<=row.last-row.first;++step){
                const unsigned x=triangle.flip?row.first+step:row.last-step;
                const uint8_t mask=RdpCoverage::mask(row,x);
                // The blender applies AA/alpha/coverage gates after memory latch reads.
                // Legacy direct calls without it retain the non-AA store gate.
                if(!blender && !(mask&128))continue;
                RdpTriangleAttributes::Pixel pixel;RdpTriangleAttributes::pixel(attributes,x,mask,pixel);
                RdpCombiner::Sources sources;
                if(conversion){sources.k4=conversion->coefficient(4);sources.k5=conversion->coefficient(5);}
                for(unsigned lane=0;lane<4;++lane)sources.shade[lane]=pixel.rgba[lane];
                if(textured){
                    const auto& tile=texture->tiles[triangle.tile];
                    const auto divided=RdpTextureDivider::divide(uint16_t(pixel.stw[0]),uint16_t(pixel.stw[1]),uint16_t(pixel.stw[2]),mode&(1ull<<51));
                    const auto sample=RdpTextureCoordinates::sample(tile,RdpTextureDivider::saturate(divided.s),RdpTextureDivider::saturate(divided.t));
                    if((mode&(1ull<<45)) || (mode&(1ull<<47))){
                        const auto palette=(mode&(1ull<<47))?((mode&(1ull<<46))?RdpTexelQuad::Palette::ia:RdpTexelQuad::Palette::rgba):RdpTexelQuad::Palette::none;
                        const auto quad=RdpTexelQuad::read(*tmem,tile,sample,palette,!(mode&(1ull<<45)));
                        RdpTextureFilter::Options options;options.s=sample.s.fraction;options.t=sample.t.fraction;
                        options.mid=mode&(1ull<<44);options.yuv=tile.format()==1;options.odd_s=sample.s.texel&1;
                        sources.texel0=(mode&(1ull<<43))?RdpTextureFilter::filter(quad,options):RdpTextureFilter::convert_weights(quad,options,conversion->weights());
                    }else{
                        const auto texel=RdpTexelFetch::read(*tmem,tile,sample.s.texel,sample.t.texel);
                        if(mode&(1ull<<43)){for(unsigned lane=0;lane<4;++lane)sources.texel0[lane]=unsigned(texel[lane])&511;}
                        else{RdpTextureFilter::Quad quad{};quad.fill(texel);sources.texel0=RdpTextureFilter::convert_weights(quad,{},conversion->weights());}
                    }
                }
                auto color=combiner.evaluate(1,sources).clamped;
                const size_t at=origin+(size_t(y)*width+x)*bytes;
                if(blender){const uint32_t memory=bytes==2?RdpFramebuffer::read16(ram.data()+at,hidden->halfword(at)):(uint32_t(ram[at])<<24)|(uint32_t(ram[at+1])<<16)|(uint32_t(ram[at+2])<<8)|ram[at+3];
                    const auto result=blender->pixel(mode,color,pixel.rgba[3],memory,RdpCoverage::derivatives(mask).count,mask>>7,attributes.delta_z);
                    if(!result.write){RdpFramebuffer::rejected(at,bytes,result.rgb,result.coverage,hidden);continue;}
                    RdpFramebuffer::write(at,bytes,result.rgb,result.coverage,ram,hidden);continue;
                }
                const unsigned memory=(mode&64)?ram[at+3]>>5:7;
                const unsigned cvg=RdpCoverage::stored(unsigned(mode>>8)&3,false,RdpCoverage::derivatives(mask).count,memory);
                RdpFramebuffer::write(at,bytes,{color[0],color[1],color[2]},cvg,ram,hidden);
            }
        }
        return 0;
    }
};
}
