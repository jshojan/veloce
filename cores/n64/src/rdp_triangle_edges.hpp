#pragma once
#include "rdp_triangle.hpp"
namespace n64 {
// Raw edge accumulator at one quarter-scanline. No clipping/coverage/pixels.
struct RdpTriangleEdges {
    struct Sample {uint32_t major=0,minor=0;};
    static int64_t signed_field(uint32_t raw,unsigned bits){
        const uint32_t mask=(uint32_t(1)<<bits)-1;raw&=mask;
        return raw&(uint32_t(1)<<(bits-1))?int64_t(raw)-(int64_t(1)<<bits):int64_t(raw);
    }
    static int64_t quarter_step(uint32_t slope){
        const int64_t value=signed_field(slope,30);
        int64_t step=value>=0?value/4:-((-value+3)/4);
        if(step%2) --step;
        return step;
    }
    static bool sample(const RdpTriangle& t,int32_t y,Sample& out){
        const int32_t top=t.y[2]>=0?(t.y[2]/4)*4:-(((-t.y[2]+3)/4)*4);
        if(y<top || y < -8192 || y>8191)return false;
        const bool lower=t.y[1]>=top && y>=t.y[1];
        const unsigned edge=lower?0:2;const int32_t anchor=lower?t.y[1]:top;
        Sample value;
        value.major=advance(t.edge[1],y-top);
        value.minor=advance(t.edge[edge],y-anchor);
        out=value;return true;
    }
private:
    static uint32_t advance(const std::array<uint32_t,2>& edge,int32_t quarters){
        const uint32_t initial=uint32_t(signed_field(edge[0],28))&~1u;
        return uint32_t(uint64_t(initial)+uint64_t(quarter_step(edge[1])*quarters));
    }
};
}
