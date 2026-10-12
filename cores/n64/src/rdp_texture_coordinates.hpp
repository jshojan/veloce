#pragma once
#include "rdp_texture.hpp"
#include <algorithm>
namespace n64 {
// Tile coordinate processing for one/two-cycle sampling. Inputs are raw signed
// 16-bit S/T with five fractional bits, after division/LOD saturation. Bounds
// use quarter-texels. Perspective, LOD, TMEM fetching and filtering are separate.
struct RdpTextureCoordinates {
    struct Axis {int32_t texel=0,neighbor=0;unsigned fraction=0;};
    struct Sample {Axis s,t;};
    struct SimpleAxis {unsigned high=0,mask=0;bool clamp=false;};
    struct SimpleTile {SimpleAxis s,t;bool valid=false;};
    // Zero shift/low, nonmirrored tiles with <=8 mask bits cover the common
    // RGBA16 game path. Both axes wrap their neighbor to zero at this width.
    static SimpleTile prepare_simple(const RdpTexture::Tile& tile){
        const unsigned sm=tile.mask(false),tm=tile.mask(true);
        if(tile.shift(false) || tile.shift(true) || tile.low(false) || tile.low(true) ||
           tile.mirror(false) || tile.mirror(true) || !sm || !tm || sm>8 || tm>8)return {};
        return {{tile.high(false),(1u<<sm)-1,bool(tile.clamp(false))},
                {tile.high(true),(1u<<tm)-1,bool(tile.clamp(true))},true};
    }
    static Sample sample_simple(const SimpleTile& tile,uint16_t s,uint16_t t){
        return {simple_axis(tile.s,s),simple_axis(tile.t,t)};
    }
    static Axis axis(const RdpTexture::Tile& tile,bool t,uint16_t raw){
        const unsigned shift=tile.shift(t),low=tile.low(t),high=tile.high(t);
        int32_t coordinate;
        if(shift<=10)coordinate=divide(signed16(raw),1u<<shift);
        else coordinate=signed16(uint16_t(uint32_t(raw)<<(16-shift)));
        const bool maximum=divide(coordinate,8)>=int32_t(high);
        coordinate-=int32_t(low*8);
        Axis result;result.fraction=uint32_t(coordinate)&31;
        if(tile.clamp(t) || !tile.mask(t)){
            if(maximum){result.texel=int32_t(((high>>2)-(low>>2))&1023);result.fraction=0;}
            else if(coordinate<0){result.texel=0;result.fraction=0;}
            else result.texel=divide(coordinate,32);
        }else result.texel=divide(coordinate,32);
        int32_t difference=1;
        if(tile.mask(t)){
            const unsigned bits=std::min(tile.mask(t),10u),mask=(1u<<bits)-1;
            if(tile.mirror(t)){
                const unsigned wrap=(uint32_t(result.texel)>>bits)&1;
                if(wrap)result.texel=~result.texel;
                result.texel=int32_t(uint32_t(result.texel)&mask);
                difference=((uint32_t(result.texel)-wrap)&mask)==mask?0:1-int32_t(wrap*2);
            }else{
                result.texel=int32_t(uint32_t(result.texel)&mask);
                // The T neighbor subtractor retains upper address bits at a
                // mask boundary; S wraps the whole masked coordinate to zero.
                if(unsigned(result.texel)==mask)difference=-(t?(result.texel&255):result.texel);
            }
        }
        result.neighbor=result.texel+difference;return result;
    }
    static Sample sample(const RdpTexture::Tile& tile,uint16_t s,uint16_t t){return {axis(tile,false,s),axis(tile,true,t)};}
private:
    static Axis simple_axis(const SimpleAxis& setup,uint16_t raw){
        const int32_t coordinate=signed16(raw);
        Axis result;result.fraction=raw&31;
        int32_t texel;
        if(setup.clamp && coordinate>=int32_t(setup.high*8)){
            texel=int32_t((setup.high>>2)&1023);result.fraction=0;
        }else if(setup.clamp && coordinate<0){texel=0;result.fraction=0;}
        else texel=divide(coordinate,32);
        result.texel=int32_t(uint32_t(texel)&setup.mask);
        result.neighbor=int32_t((uint32_t(result.texel)+1)&setup.mask);
        return result;
    }
    static int32_t signed16(uint16_t raw){return raw>=32768?int32_t(raw)-65536:int32_t(raw);}
    static int32_t divide(int32_t value,unsigned divisor){return value>=0?value/int32_t(divisor):-int32_t((uint32_t(-value)+divisor-1)/divisor);}
};
}
