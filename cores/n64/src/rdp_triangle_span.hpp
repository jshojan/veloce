#pragma once
#include "rdp_triangle_edges.hpp"
#include <algorithm>
namespace n64 {
// Shared clipped quarter-row geometry. No pixel/color or coverage-memory policy.
struct RdpTriangleSpan {
    struct Clipped {unsigned x=0;bool under=false,over=false;};
    struct Row {
        std::array<unsigned,4> left{},right{};
        std::array<bool,4> enabled{};
        unsigned first=0,last=0;
        bool valid=false;
    };
    struct Bounds {
        unsigned left=0,right=0,first=0,last=0;
        int32_t top=0,bottom=0;
        bool field=false,odd=false,valid=false;
    };
    static Bounds bounds(const RdpTriangle& t,uint64_t scissor){
        Bounds b;b.left=unsigned(scissor>>44)&4095;b.right=unsigned(scissor>>12)&4095;
        b.top=std::max(t.y[2],int32_t((scissor>>32)&4095));b.bottom=std::min(t.y[0],int32_t(scissor&4095));
        b.field=bool(scissor&(1ull<<25));b.odd=bool(scissor&(1ull<<24));
        b.valid=b.left<b.right && b.top<b.bottom;
        if(b.valid){b.first=unsigned(b.top)/4;b.last=unsigned(b.bottom-1)/4;}
        return b;
    }
    static Clipped clip(uint32_t x,unsigned left,unsigned right){
        const unsigned sticky=bool((x>>1)&8191);
        unsigned value=((x>>13)&8190)|sticky;
        const bool under=(x&0x8000000) || (value<left*2 && !(x&0x4000000));
        value=under?left*2:((x>>13)&16382)|sticky;
        const bool over=(value&8192) || (value&8191)>=right*2;
        return {(over?right*2:value)&8191,under,over};
    }
    static Row row(const RdpTriangle& t,uint64_t scissor,unsigned scanline){
        return row(t,bounds(t,scissor),scanline);
    }
    static Row row(const RdpTriangle& t,const Bounds& b,unsigned scanline){
        Row result;if(scanline>=1024)return result;
        if(!b.valid || scanline<b.first || scanline>b.last)return result;
        if(b.field && bool(scanline&1)!=b.odd)return result;
        unsigned low=4095,high=0;bool any=false,allover=true,allunder=true;
        for(unsigned sub=0;sub<4;++sub){
            const int32_t y=int32_t(scanline*4+sub);RdpTriangleEdges::Sample sample;
            if(!RdpTriangleEdges::sample(t,y,sample))continue;
            const auto major=clip(sample.major,b.left,b.right),minor=clip(sample.minor,b.left,b.right);
            allover=allover && major.over && minor.over;
            allunder=allunder && major.under && minor.under;
            result.left[sub]=t.flip?major.x:minor.x;
            result.right[sub]=t.flip?minor.x:major.x;
            const uint32_t major_order=(sample.major^0x8000000)&0xfffc000;
            const uint32_t minor_order=(sample.minor^0x8000000)&0xfffc000;
            const bool crossed=t.flip?minor_order<major_order:major_order<minor_order;
            if(y<b.top || y>=b.bottom || crossed)continue;
            any=true;result.enabled[sub]=true;
            low=std::min(low,result.left[sub]/8);high=std::max(high,result.right[sub]/8);
        }
        if(any && !allover && !allunder && low<=high){result.first=low;result.last=high;result.valid=true;}
        return result;
    }
};
}
