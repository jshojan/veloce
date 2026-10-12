#pragma once
#include "rdp_triangle_attributes.hpp"
#include "rdp_rectangle.hpp"
#include <vector>
#include "rdp_texel_quad.hpp"
#include "rdp_convert.hpp"
#include "rdp_texture_divider.hpp"
#include "rdp_blender.hpp"
#include "rdp_framebuffer.hpp"
#include "rdp_primitive_depth.hpp"
#include "rdp_dither.hpp"
namespace n64 {
// Functional one-cycle RGBA16/RGBA32 and bounded two-cycle fog/pass feedback:
// optional blending/AA/alpha-coverage decisions.
// Interpolated/primitive Z, ordered RGB/alpha dither; no key or prior-pixel feedback. Affine/perspective
// texture sampling supports cycle-zero/one tile filtering and passthrough, color conversion and
// optional TLUT. SetConvert also supplies K4/K5 combiner inputs.
// Unsupported profiles reject before any framebuffer write. Physical pipeline/crash timing remains separate work.
struct RdpTriangleShade {
    static unsigned draw(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,const RdpCombiner& combiner,std::vector<uint8_t>& ram,const RdpTexture* texture=nullptr,const RdpTmem* tmem=nullptr,const RdpConvert* conversion=nullptr,RdpBlender* blender=nullptr,HiddenRdram* hidden=nullptr,uint32_t depth_image=0,const RdpPrimitiveDepth* primitive=nullptr){
        RdpTriangle triangle;if(!RdpTriangle::decode(packet,triangle))return 1;
        return draw_decoded(triangle,image,mode,scissor,combiner,ram,texture,tmem,conversion,blender,hidden,depth_image,primitive);
    }
    static unsigned rectangle(const RdpPacket& packet,uint64_t image,uint64_t mode,uint64_t scissor,const RdpCombiner& combiner,std::vector<uint8_t>& ram,const RdpTexture* texture=nullptr,const RdpTmem* tmem=nullptr,const RdpConvert* conversion=nullptr,RdpBlender* blender=nullptr,HiddenRdram* hidden=nullptr,uint32_t depth_image=0,const RdpPrimitiveDepth* primitive=nullptr){
        RdpTriangle triangle;if(!RdpRectangle::decode(packet,false,triangle))return 1;
        return draw_decoded(triangle,image,mode,scissor,combiner,ram,texture,tmem,conversion,blender,hidden,depth_image,primitive);
    }
private:
    static unsigned draw_decoded(const RdpTriangle& triangle,uint64_t image,uint64_t mode,uint64_t scissor,const RdpCombiner& combiner,std::vector<uint8_t>& ram,const RdpTexture* texture=nullptr,const RdpTmem* tmem=nullptr,const RdpConvert* conversion=nullptr,RdpBlender* blender=nullptr,HiddenRdram* hidden=nullptr,uint32_t depth_image=0,const RdpPrimitiveDepth* primitive=nullptr){
        const bool textured=triangle.texture && texture && tmem;
        const bool two_cycle=((mode>>52)&3)==1;
        const bool lod_enabled=mode&(1ull<<48);
        // Bounded two-cycle LOD: exact 1/32-texel S/T gradients and constant W.
        // d/dEdge drives interpolation; LOD uses the independent d/dY.
        // Perspective uses post-division adjacent coordinates per pixel.
        unsigned first_tile=triangle.tile,second_tile=(triangle.tile+1)&7,lod_fraction=0;
        const auto select_lod=[&](unsigned units,unsigned& first,unsigned& second,unsigned& fraction){
            const bool magnify=units<32;
            unsigned level=0;
            for(unsigned value=units>>5;value>1;value>>=1)++level;
            const bool distant=!triangle.level || (!magnify && level>=triangle.level);
            fraction=distant?255u:magnify?0u:((units<<3)>>level)&255u;
            if(level>triangle.level)level=triangle.level;
            first=(triangle.tile+level)&7;
            second=(first+(!magnify&&!distant?1u:0u))&7;
        };
        if(lod_enabled){
            const bool simple_gradient=!triangle.tex[1][2] && !triangle.tex[2][2] &&
                !triangle.tex[3][2];
            constexpr uint64_t unit=1ull<<16;
            unsigned lod_units=0;
            for(unsigned group=1;group<=3;group+=2)for(unsigned lane=0;lane<2;++lane){
                const int64_t gradient=int32_t(triangle.tex[group][lane]);
                const uint64_t absolute=uint64_t(gradient<0?-gradient:gradient);
                if(absolute%unit || absolute/unit>255*32)return 1;
                // The RDP's signed-difference absolute value uses ones'
                // complement, so a negative delta loses one 1/32-texel unit.
                const unsigned units=unsigned(absolute/unit)-(gradient<0?1u:0u);
                if(units>lod_units)lod_units=units;
            }
            if(!textured || !two_cycle || !simple_gradient ||
               (combiner.uses_lod_fraction() && combiner.minimum_lod()))return 1;
            if(!(mode&(1ull<<51)))select_lod(lod_units,first_tile,second_tile,lod_fraction);
        }
        const uint64_t sampling=textured?((63ull<<42)|(1ull<<51)|(1ull<<48)):(blender?(2047ull<<41):0);
        const uint64_t dither=(3ull<<38)|(3ull<<36);
        const bool depth_mode=mode&48,primitive_mode=mode&4;
        if(primitive_mode && !primitive)return 1;
        if(depth_mode && (!blender || !hidden))return 1;
        const unsigned rgb_dither=unsigned(mode>>38)&3,alpha_dither=unsigned(mode>>36)&3;
        const uint64_t ignored=blender?((3ull<<54)|(15ull<<32)|(1ull<<15)):0;
        const uint64_t allowed=ignored|dither|0xffff0000ull|0x340ull|sampling|(blender?0x7cbdull:0)|(two_cycle?(1ull<<52):0);
        if((mode&~allowed) || (blender?!RdpDither::supported(rgb_dither,alpha_dither):(mode&dither)!=dither) || (!blender && ((mode>>30)&3)) ||
           (two_cycle?(!blender || !(RdpBlender::fog_two_cycle_supported(mode) || RdpBlender::pass_two_cycle_supported(mode)) || !combiner.independent_two_cycle(textured,conversion!=nullptr,textured,lod_enabled)):
                      !combiner.independent_one_cycle(textured,conversion!=nullptr)) ||
           (textured && !(mode&(1ull<<43)) && !conversion))return 1;
        const unsigned size=unsigned(image>>51)&3,bytes=size==2?2:4;
        if(((image>>53)&7)!=0 || (size!=3 && !(size==2 && hidden && blender)))return 1;
        if(hidden && hidden->size()!=ram.size())return 5;
        const uint32_t origin=uint32_t(image)&0x3ffffff;
        if(origin&7)return 4;
        const unsigned width=(unsigned(image>>32)&1023)+1;
        const auto bounds=RdpTriangleSpan::bounds(triangle,scissor);
        if(!bounds.valid)return 0;
        if(depth_mode && (depth_image&7))return 4;
        // Preflight every complete row so functional bounds rejection is atomic.
        for(unsigned y=bounds.first;y<=bounds.last;++y){const auto row=RdpTriangleSpan::row(triangle,bounds,y);if(row.valid){const uint64_t end=uint64_t(y)*width+row.last+1;
            if(uint64_t(origin)+end*bytes>ram.size() || (depth_mode && uint64_t(depth_image)+end*2>ram.size()))return 5;}}
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
                unsigned pixel_first_tile=first_tile,pixel_second_tile=second_tile;
                RdpCombiner::Sources sources;
                sources.lod=lod_fraction;
                if(conversion){sources.k4=conversion->coefficient(4);sources.k5=conversion->coefficient(5);}
                for(unsigned lane=0;lane<4;++lane)sources.shade[lane]=pixel.rgba[lane];
                if(textured){
                    const auto divided=RdpTextureDivider::divide(uint16_t(pixel.stw[0]),uint16_t(pixel.stw[1]),uint16_t(pixel.stw[2]),mode&(1ull<<51));
                    if(lod_enabled && (mode&(1ull<<51))){
                        const auto neighbor=[&](uint32_t ds,uint32_t dt){
                            return RdpTextureDivider::divide(uint16_t((pixel.raw[4]+ds)>>16),
                                uint16_t((pixel.raw[5]+dt)>>16),uint16_t(pixel.stw[2]),true);
                        };
                        const auto right=neighbor(attributes.dx[4],attributes.dx[5]);
                        const auto below=neighbor(triangle.tex[3][0],triangle.tex[3][1]);
                        const auto distance=[](uint32_t current,uint32_t next){
                            const int32_t a=int32_t(current&131071)-((current&65536)?131072:0);
                            const int32_t b=int32_t(next&131071)-((next&65536)?131072:0);
                            const int32_t delta=b-a;
                            return unsigned(delta<0?-delta-1:delta);
                        };
                        const bool overflow=(divided.s|divided.t|right.s|right.t|below.s|below.t)&0x60000;
                        unsigned units=overflow?16384u:0u;
                        if(!overflow){
                            for(unsigned axis=0;axis<2;++axis){
                                const auto& pair=axis?below:right;
                                const unsigned ds=distance(divided.s,pair.s),dt=distance(divided.t,pair.t);
                                if(ds>units)units=ds;
                                if(dt>units)units=dt;
                            }
                        }
                        select_lod(units,pixel_first_tile,pixel_second_tile,sources.lod);
                    }
                    const auto fetch=[&](const RdpTexture::Tile& tile)->RdpCombiner::Color{
                        const auto sample=RdpTextureCoordinates::sample(tile,RdpTextureDivider::saturate(divided.s),RdpTextureDivider::saturate(divided.t));
                        if((mode&(1ull<<45)) || (mode&(1ull<<47))){
                            const auto palette=(mode&(1ull<<47))?((mode&(1ull<<46))?RdpTexelQuad::Palette::ia:RdpTexelQuad::Palette::rgba):RdpTexelQuad::Palette::none;
                            const auto quad=RdpTexelQuad::read(*tmem,tile,sample,palette,!(mode&(1ull<<45)));
                            RdpTextureFilter::Options options;options.s=sample.s.fraction;options.t=sample.t.fraction;
                            options.mid=mode&(1ull<<44);options.yuv=tile.format()==1;options.odd_s=sample.s.texel&1;
                            return (mode&(1ull<<43))?RdpTextureFilter::filter(quad,options):RdpTextureFilter::convert_weights(quad,options,conversion->weights());
                        }
                        const auto texel=RdpTexelFetch::read(*tmem,tile,sample.s.texel,sample.t.texel);
                        if(mode&(1ull<<43)){
                            RdpCombiner::Color color{};for(unsigned lane=0;lane<4;++lane)color[lane]=unsigned(texel[lane])&511;return color;
                        }
                        RdpTextureFilter::Quad quad{};quad.fill(texel);return RdpTextureFilter::convert_weights(quad,{},conversion->weights());
                    };
                    sources.texel0=fetch(texture->tiles[pixel_first_tile]);
                    if(two_cycle && combiner.uses_texel1_cycle1())
                        sources.texel1=fetch(texture->tiles[pixel_second_tile]);
                }
                if(two_cycle)sources.combined=combiner.evaluate(0,sources).combined;
                auto color=combiner.evaluate(1,sources).clamped;
                const size_t at=origin+(size_t(y)*width+x)*bytes;
                if(blender){const uint32_t memory=bytes==2?RdpFramebuffer::read16(ram.data()+at,hidden->halfword(at)):(uint32_t(ram[at])<<24)|(uint32_t(ram[at+1])<<16)|(uint32_t(ram[at+2])<<8)|ram[at+3];
                    const size_t za=depth_image+(size_t(y)*width+x)*2;
                    const unsigned z=primitive_mode?primitive->z():pixel.z;
                    const unsigned delta=primitive_mode?primitive->delta():attributes.delta_z;
                    RdpBlender::DepthInput depth;
                    if(depth_mode){depth.z=z;if(mode&16){depth.word=uint16_t((unsigned(ram[za])<<8)|ram[za+1]);depth.hidden=hidden->halfword(za);}}
                    const auto seeds=RdpDither::sample(rgb_dither,alpha_dither,x,y,bounds.field);
                    auto result=two_cycle?
                        blender->pixel_two_cycle(mode,color,pixel.rgba[3],memory,RdpCoverage::derivatives(mask).count,mask>>7,delta,depth_mode?&depth:nullptr,seeds.alpha):
                        blender->pixel(mode,color,pixel.rgba[3],memory,RdpCoverage::derivatives(mask).count,mask>>7,delta,depth_mode?&depth:nullptr,seeds.alpha);
                    RdpDither::apply(result.rgb,seeds.rgb);
                    if(!result.write){RdpFramebuffer::rejected(at,bytes,result.rgb,result.coverage,hidden);continue;}
                    RdpFramebuffer::write(at,bytes,result.rgb,result.coverage,ram,hidden);
                    if(mode&32)RdpDepth::store(za,z,RdpDepth::delta_encoding(uint16_t(delta)),ram,*hidden);
                    continue;
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
