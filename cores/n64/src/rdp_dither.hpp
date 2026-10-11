#pragma once
#include <array>
#include <algorithm>
namespace n64 {
// Settled ordered-dither seeds and final RGB rounding. Random selectors require
// retained RNG/noise state and remain unsupported by this functional profile.
struct RdpDither {
    struct Seeds {unsigned rgb=7,alpha=0;};
    static bool supported(unsigned rgb,unsigned alpha){return rgb<4 && alpha<4 && rgb!=2 && alpha!=2;}
    static Seeds sample(unsigned rgb,unsigned alpha,unsigned x,unsigned y,bool field=false){
        constexpr std::array<unsigned,16> magic={0,6,1,7,4,2,5,3,3,5,2,4,7,1,6,0};
        constexpr std::array<unsigned,16> bayer={0,4,1,5,4,0,5,1,3,7,2,6,7,3,6,2};
        const unsigned index=((y>>(field?1:0))&3)*4+(x&3);
        const unsigned base=rgb==0?magic[index]:bayer[index];
        return {rgb==3?7:base,alpha==3?0:alpha==0?base:(base^7)};
    }
    static unsigned channel(unsigned value,unsigned seed){return (value&7)>seed?std::min(255u,(value&248)+8):value;}
    static void apply(std::array<unsigned,3>& color,unsigned seed){for(auto& value:color)value=channel(value,seed);}
};
}
