#include "rdp_texture_filter.hpp"
#include <cstdlib>
#include <iostream>
using Filter=n64::RdpTextureFilter;
static void check(bool v){if(!v){std::cerr<<"texture filter mismatch\n";std::exit(1);}}
static int64_t floor_div(int64_t v,int64_t d){return v/d-(v%d<0);}
static int64_t signed9(int32_t v){const unsigned r=unsigned(v)%512;return r<256?r:int64_t(r)-512;}
static Filter::Color oracle(const Filter::Quad& q,const Filter::Options& o){
    Filter::Color result;const unsigned s=o.s%32,t=o.t%32,uv=o.yuv?(s/2+unsigned(o.odd_s)*16):s;
    for(unsigned lane=0;lane<4;++lane){
        const unsigned x=lane<2?uv:s;const bool upper=x+t>=32,center=o.mid && x==16 && t==16;
        std::array<int64_t,4> weights{};int64_t divisor=32,rounding=16,bias=0;
        if(o.convert_one){const auto a=signed9(o.previous[0]),b=signed9(o.previous[1]);divisor=256;rounding=128;bias=signed9(o.previous[2])*256;
            if(center)weights={64,b,a,-64-a-b};else if(upper)weights={0,b,a,-a-b};else weights={-a-b,a,b,0};
        }else if(center){weights={1,1,1,1};divisor=4;rounding=2;}
        else if(upper)weights={0,32-int64_t(t),32-int64_t(x),int64_t(x+t)-32};
        else weights={32-int64_t(x+t),int64_t(x),int64_t(t),0};
        int64_t sum=bias+rounding;for(unsigned corner=0;corner<4;++corner)sum+=weights[corner]*q[corner][lane];
        result[lane]=unsigned(floor_div(sum,divisor))%512;
    }
    return result;
}
static Filter::Color converted(const Filter::Quad& q,const Filter::Options& o,const std::array<unsigned,4>& c){
    const unsigned s=o.s%32,t=o.t%32,uv=o.yuv?s/2+unsigned(o.odd_s)*16:s;
    const auto y=o.convert_one?signed9(o.previous[2]):q[s+t<32?0:3][2];
    const auto u=o.convert_one?signed9(o.previous[0]):q[uv+t<32?0:3][0];
    const auto v=o.convert_one?signed9(o.previous[1]):q[uv+t<32?0:3][1];
    const int64_t k0=2*signed9(int32_t(c[0]))+1,k1=2*signed9(int32_t(c[1]))+1,k2=2*signed9(int32_t(c[2]))+1,k3=2*signed9(int32_t(c[3]))+1;
    return {unsigned(y+floor_div(k0*v+128,256))%512,unsigned(y+floor_div(k1*u+k2*v+128,256))%512,unsigned(y+floor_div(k3*u+128,256))%512,unsigned(y)%512};
}
static Filter::Quad corners(unsigned a,unsigned b){Filter::Quad q;for(unsigned lane=0;lane<4;++lane){q[0][lane]=int32_t((a+lane*17)%256);q[1][lane]=int32_t((b+lane*31)%256);q[2][lane]=255-q[0][lane];q[3][lane]=255-q[1][lane];}return q;}
int main(){
    uint64_t normal=0,uv_cases=0,previous=0,conversion=0;
    Filter::Options o;o.mid=true;
    for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){const auto q=corners(a,b);
        for(unsigned s=0;s<32;++s)for(unsigned t=0;t<32;++t){o.s=s;o.t=t;check(Filter::filter(q,o)==oracle(q,o));++normal;}
        o.s=o.t=16;o.mid=false;check(Filter::filter(q,o)==oracle(q,o));++normal;o.mid=true;
    }
    for(unsigned pattern=0;pattern<512;++pattern){auto q=corners(pattern%256,pattern/2);for(auto& point:q){point[0]-=128;point[1]-=128;}
        for(unsigned flags=0;flags<4;++flags)for(unsigned s=0;s<32;++s)for(unsigned t=0;t<32;++t){o={};o.s=s;o.t=t;o.yuv=true;o.odd_s=flags&1;o.mid=flags&2;check(Filter::filter(q,o)==oracle(q,o));++uv_cases;}
    }
    for(unsigned a=0;a<512;++a)for(unsigned b=0;b<512;++b){auto q=corners(a%256,b%256);
        for(unsigned test=0;test<5;++test){o={};const unsigned ss[]={0,7,16,23,31},tt[]={0,31,16,17,31};o.s=ss[test];o.t=tt[test];o.mid=true;o.yuv=(a>>1)&1;o.odd_s=b&1;o.convert_one=true;o.previous={int32_t(a),int32_t(b),int32_t((a*13+b*7)%512),0};check(Filter::filter(q,o)==oracle(q,o));++previous;}
    }
    for(unsigned k=0;k<512;++k)for(unsigned raw=0;raw<512;++raw){auto q=corners(raw%256,raw/2);q[0][0]=int32_t(raw)-256;q[3][0]=255-int32_t(raw);q[0][1]=255-int32_t(raw);q[3][1]=int32_t(raw)-256;
        for(unsigned test=0;test<8;++test){o={};o.s=(test&1)?25:9;o.t=(test&2)?27:7;o.yuv=true;o.odd_s=test&1;o.convert_one=test&4;o.previous={int32_t(raw),int32_t(raw^511),int32_t((raw*19)%512),0};const std::array<unsigned,4> c={k,k^511,(k*17)%512,(k*23)%512};check(Filter::convert(q,o,c)==converted(q,o,c));++conversion;}
    }
    for(unsigned a=0;a<512;++a)for(unsigned b=0;b<512;++b){const auto q=corners(a%256,b%256);o={};o.previous={int32_t(a),int32_t(b),int32_t((a+b)%512),0};o.convert_one=true;const std::array<unsigned,4> c={0,a,b,511};check(Filter::convert(q,o,c)==converted(q,o,c));++conversion;}
    // Mid-texel averaging uses all four corners; ordinary filtering uses three.
    Filter::Quad q{};q[0].fill(0);q[1].fill(0);q[2].fill(0);q[3].fill(255);o={};o.s=o.t=16;o.mid=true;
    check(Filter::filter(q,o)==Filter::Color{64,64,64,64});o.mid=false;check(Filter::filter(q,o)==Filter::Color{0,0,0,0});
    std::cout<<normal<<" RGB lattice captures; "<<uv_cases<<" chroma lattice captures; "<<previous<<" previous-texel captures; "<<conversion<<" conversion captures; center goldens PASS\n";
}
