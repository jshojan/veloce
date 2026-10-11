#pragma once
#include "rdp_texel_fetch.hpp"
namespace n64 {
// Arithmetic over prepared TMEM corner colors, ordered 00,10,01,11. Results
// retain nine bits for the combiner. Fetch-bank selection, mode registers,
// previous-texel storage, division/LOD and physical timing belong to callers.
struct RdpTextureFilter {
    using Input=RdpTexelFetch::Color;
    using Quad=std::array<Input,4>;
    using Color=std::array<unsigned,4>;
    struct Options {
        unsigned s=0,t=0;
        bool mid=false,yuv=false,odd_s=false,convert_one=false;
        Input previous{};
    };
    static Color filter(const Quad& q,const Options& options){
        Color result;
        const unsigned t=options.t&31,s=options.s&31;
        const unsigned uv=options.yuv?(s>>1)|(options.odd_s?16u:0u):s;
        for(unsigned lane=0;lane<4;++lane){
            const unsigned fraction=lane<2?uv:s;
            const bool upper=fraction+t>=32,center=options.mid && fraction==16 && t==16;
            int64_t value;
            if(options.convert_one){
                const int64_t a=signed9(options.previous[0]),b=signed9(options.previous[1]),base=signed9(options.previous[2]);
                const unsigned anchor=upper?3:0;
                int64_t sum=a*(int64_t(q[upper?2:1][lane])-q[anchor][lane])+b*(int64_t(q[upper?1:2][lane])-q[anchor][lane]);
                if(center)sum+=64*(int64_t(q[0][lane])-q[3][lane]);
                value=base+floor(sum+128,256);
            }else if(center){
                value=floor(int64_t(q[0][lane])+q[1][lane]+q[2][lane]+q[3][lane]+2,4);
            }else if(upper){
                value=q[3][lane]+floor((32-fraction)*(int64_t(q[2][lane])-q[3][lane])+(32-t)*(int64_t(q[1][lane])-q[3][lane])+16,32);
            }else{
                value=q[0][lane]+floor(fraction*(int64_t(q[1][lane])-q[0][lane])+t*(int64_t(q[2][lane])-q[0][lane])+16,32);
            }
            result[lane]=unsigned(value)&511;
        }
        return result;
    }
    static Color convert(const Quad& q,const Options& options,const std::array<unsigned,4>& coefficients){
        const unsigned s=options.s&31,t=options.t&31,uv=options.yuv?(s>>1)|(options.odd_s?16u:0u):s;
        const auto& chroma=q[uv+t>=32?3:0];const auto& luma=q[s+t>=32?3:0];
        const int64_t u=options.convert_one?signed9(options.previous[0]):chroma[0];
        const int64_t v=options.convert_one?signed9(options.previous[1]):chroma[1];
        const int64_t y=options.convert_one?signed9(options.previous[2]):luma[2];
        std::array<int64_t,4> k{};for(unsigned i=0;i<4;++i)k[i]=2*signed9(int32_t(coefficients[i]&511))+1;
        return {unsigned(y+floor(k[0]*v+128,256))&511,unsigned(y+floor(k[1]*u+k[2]*v+128,256))&511,unsigned(y+floor(k[3]*u+128,256))&511,unsigned(y)&511};
    }
private:
    static int64_t signed9(int32_t value){const unsigned raw=unsigned(value)&511;return raw<256?int64_t(raw):int64_t(raw)-512;}
    static int64_t floor(int64_t value,int64_t divisor){return value>=0?value/divisor:-((-value+divisor-1)/divisor);}
};
}
