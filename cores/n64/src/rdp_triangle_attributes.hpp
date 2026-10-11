#pragma once
#include "rdp_coverage.hpp"
#include "rdp_combiner.hpp"
namespace n64 {
// Functional interpolants for one/two-cycle rasterization. The row's start is
// the clipped major-side pixel (first for flip, last otherwise). No draw,
// perspective division, texture fetch, blending or depth-memory policy here.
struct RdpTriangleAttributes {
    struct Row {
        // RGBA, STW, Z, with 32-bit modular accumulators.
        std::array<uint32_t,8> initial{},dx{};
        // Coverage-correction gradients: RGBA and Z only; STW slots stay zero.
        std::array<int32_t,8> derivative_x{},derivative_y{};
        unsigned start=0,delta_z=0;
    };
    struct Pixel {
        std::array<uint32_t,8> raw{};
        std::array<uint8_t,4> rgba{};
        std::array<int32_t,3> stw{};
        unsigned z=0;
    };
    static bool row(const RdpTriangle& t,unsigned scan,unsigned start,Row& out){
        if(t.y[2]<-8192 || t.y[2]>8191)return false;
        const int32_t top=t.y[2]>=0?t.y[2]/4:-((-t.y[2]+3)/4);
        if(scan>=1024 || start>=1024 || int32_t(scan)<top)return false;
        const bool offset=bool(t.edge[1][1]&0x80000000)==t.flip;
        RdpTriangleEdges::Sample edge;
        if(!RdpTriangleEdges::sample(t,int32_t(scan*4)+(offset?3:0),edge))return false;
        const int32_t anchor=int32_t(RdpTriangleEdges::signed_field(edge.major>>16,12));
        const unsigned fraction=(edge.major>>8)&255;
        const int32_t distance=t.flip?int32_t((int32_t(start)-anchor)&4095):-int32_t((anchor-int32_t(start))&4095);
        Row value;value.start=start;
        for(unsigned lane=0;lane<8;++lane){
            const auto c=coefficients(t,lane);
            value.dx[lane]=lane==7?c[1]:c[1]&~31u;
            const unsigned shift=lane==7?10:14,bits=lane==7?22:13;
            if(lane<4 || lane==7){
                value.derivative_x[lane]=int32_t(RdpTriangleEdges::signed_field(value.dx[lane]>>shift,bits));
                value.derivative_y[lane]=int32_t(RdpTriangleEdges::signed_field(c[3]>>shift,bits));
            }
            const uint32_t stepped=c[0]+uint32_t(uint64_t(c[2])*unsigned(int32_t(scan)-top));
            uint32_t difference=0;
            if(offset){
                const uint32_t de=c[2]&~511u,dy=c[3]&~511u;
                difference=de-uint32_t(shift_signed(de,2))-dy+uint32_t(shift_signed(dy,2));
            }
            const uint32_t fractional_dx=uint32_t(shift_signed(c[1],8))&~1u;
            const uint32_t adjusted=((stepped&~511u)+difference-fraction*fractional_dx)&~1023u;
            value.initial[lane]=adjusted+uint32_t(uint64_t(value.dx[lane])*uint32_t(distance));
        }
        value.delta_z=delta_z(t.z[1],t.z[3]);
        out=value;return true;
    }
    static bool pixel(const Row& row,unsigned x,uint8_t coverage,Pixel& out){
        if(x>=1024)return false;
        Pixel value;const auto cv=RdpCoverage::derivatives(coverage);
        for(unsigned lane=0;lane<8;++lane){
            value.raw[lane]=row.initial[lane]+uint32_t(uint64_t(row.dx[lane])*uint32_t(int32_t(x)-int32_t(row.start)));
            if(lane<4){
                const int64_t quarter=shift_signed(value.raw[lane],14);
                const int64_t corrected=cv.count==8?floor_shift(quarter,2):floor_shift(quarter*4+int64_t(cv.x)*row.derivative_x[lane]+int64_t(cv.y)*row.derivative_y[lane],4);
                value.rgba[lane]=uint8_t(RdpCombiner::clamp(unsigned(corrected)));
            }else if(lane<7)value.stw[lane-4]=int32_t(RdpTriangleEdges::signed_field(value.raw[lane]>>16,16));
            else{
                const int64_t depth=(value.raw[lane]>>10)&0x3fffff;
                const int64_t corrected=cv.count==8?depth/8:floor_shift(depth*4+int64_t(cv.x)*row.derivative_x[lane]+int64_t(cv.y)*row.derivative_y[lane],5);
                const unsigned encoded=unsigned(corrected);
                const unsigned range=(encoded>>17)&3;
                value.z=range==2?0x3ffff:range==3?0:encoded&0x3ffff;
            }
        }
        out=value;return true;
    }
    static unsigned delta_z(uint32_t dx,uint32_t dy){
        const unsigned x=dx>>16,y=dy>>16;
        const unsigned sum=((x&32768)?(~x&32767):x)+((y&32768)?(~y&32767):y);
        if(sum&0xc000)return 32768;
        if(!sum)return 1;
        if(sum==1)return 3;
        unsigned bit=8192;while(!(sum&bit))bit>>=1;
        return bit*2;
    }
private:
    static int64_t floor_shift(int64_t value,unsigned bits){
        const int64_t divisor=int64_t(1)<<bits;
        return value>=0?value/divisor:-((-value+divisor-1)/divisor);
    }
    static int64_t shift_signed(uint32_t raw,unsigned bits){return floor_shift(int64_t(raw)-((raw&0x80000000)?0x100000000ll:0),bits);}
    static std::array<uint32_t,4> coefficients(const RdpTriangle& t,unsigned lane){
        if(lane==7)return t.z;
        std::array<uint32_t,4> result{};
        for(unsigned group=0;group<4;++group)result[group]=lane<4?t.color[group][lane]:t.tex[group][lane-4];
        return result;
    }
};
}
