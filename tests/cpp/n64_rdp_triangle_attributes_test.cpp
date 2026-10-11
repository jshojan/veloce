#include "rdp_triangle_attributes.hpp"
#include <cstdlib>
#include <iostream>
using Coefficients=std::array<std::array<uint32_t,4>,8>;
static void check(bool value){if(!value){std::cerr<<"triangle attributes mismatch\n";std::exit(1);}}
static int64_t sx(uint32_t v,unsigned bits){const uint64_t size=1ull<<bits;const uint64_t low=uint64_t(v)%size;return low>=size/2?int64_t(low)-int64_t(size):int64_t(low);}
static int64_t floor_div(int64_t value,int64_t divisor){int64_t q=value/divisor;if(value%divisor<0)--q;return q;}
static uint32_t wrap(int64_t value){return uint32_t(uint64_t(value)%0x100000000ull);}
static unsigned clip9(int64_t value){const unsigned v=unsigned(value)&511;return v<256?v:v<384?255:0;}
static uint32_t rng(uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static unsigned depth_delta(uint32_t x,uint32_t y){
    const int64_t a=sx(x>>16,16),b=sx(y>>16,16);
    const unsigned sum=unsigned(a<0?-a-1:a)+unsigned(b<0?-b-1:b);
    if(sum>=16384)return 32768;
    if(sum==0)return 1;
    if(sum==1)return 3;
    unsigned high=1;while(high*2<=sum)high*=2;return high*2;
}
static n64::RdpTriangleAttributes::Row oracle_row(const n64::RdpTriangle& t,const Coefficients& c,unsigned scan,unsigned start){
    n64::RdpTriangleAttributes::Row r;r.start=start;r.delta_z=depth_delta(c[7][1],c[7][3]);
    const int64_t top=floor_div(t.y[2],4);
    const bool adjust=(t.edge[1][1]>>31)==unsigned(t.flip);
    uint32_t edge=wrap(sx(t.edge[1][0],28));edge-=edge%2;
    const int64_t step=floor_div(floor_div(sx(t.edge[1][1],30),4),2)*2;
    for(int64_t quarter=top*4;quarter<int64_t(scan*4)+(adjust?3:0);++quarter)edge=wrap(int64_t(edge)+step);
    const int64_t anchor=sx(edge/65536,12),fraction=(edge/256)%256;
    int64_t skip=t.flip?int64_t(start)-anchor:anchor-int64_t(start);
    skip=(skip%4096+4096)%4096;if(!t.flip)skip=-skip;
    for(unsigned lane=0;lane<8;++lane){
        uint32_t value=c[lane][0];
        for(int64_t row=top;row<int64_t(scan);++row)value=wrap(int64_t(value)+c[lane][2]);
        int64_t correction=0;
        if(adjust){const int64_t de=floor_div(sx(c[lane][2],32),512)*512,dy=floor_div(sx(c[lane][3],32),512)*512;correction=de-floor_div(de,4)-dy+floor_div(dy,4);}
        const int64_t dxfrac=floor_div(floor_div(sx(c[lane][1],32),256),2)*2;
        const uint32_t initial=wrap(int64_t(value-value%512)+correction-fraction*dxfrac);
        r.dx[lane]=lane==7?c[lane][1]:c[lane][1]-c[lane][1]%32;
        r.initial[lane]=wrap(int64_t(initial-initial%1024)+int64_t(r.dx[lane])*skip);
        if(lane<4 || lane==7){const unsigned shift=lane==7?1024:16384,bits=lane==7?22:13;r.derivative_x[lane]=int32_t(sx(r.dx[lane]/shift,bits));r.derivative_y[lane]=int32_t(sx(c[lane][3]/shift,bits));}
    }
    return r;
}
static n64::RdpTriangleAttributes::Pixel oracle_pixel(const n64::RdpTriangleAttributes::Row& row,unsigned x,unsigned coverage){
    n64::RdpTriangleAttributes::Pixel p;
    constexpr unsigned px[]={3,2,1,0,3,2,1,0},py[]={3,2,3,2,1,0,1,0};
    unsigned count=0,rank=16,offx=0,offy=0;
    for(unsigned bit=0;bit<8;++bit)if(coverage&(1u<<bit)){++count;const unsigned candidate=py[bit]*4+px[bit];if(candidate<rank){rank=candidate;offx=px[bit];offy=py[bit];}}
    for(unsigned lane=0;lane<8;++lane){
        p.raw[lane]=wrap(int64_t(row.initial[lane])+int64_t(row.dx[lane])*(int64_t(x)-row.start));
        if(lane<4){const int64_t q=floor_div(sx(p.raw[lane],32),16384);p.rgba[lane]=uint8_t(clip9(count==8?floor_div(q,4):floor_div(q*4+int64_t(offx)*row.derivative_x[lane]+int64_t(offy)*row.derivative_y[lane],16)));}
        else if(lane<7)p.stw[lane-4]=int32_t(sx(p.raw[lane]/65536,16));
        else{const int64_t q=p.raw[lane]/1024;const int64_t corrected=count==8?q/8:floor_div(q*4+int64_t(offx)*row.derivative_x[lane]+int64_t(offy)*row.derivative_y[lane],32);const unsigned e=unsigned(corrected)&0x7ffff;p.z=e<0x40000?e:e<0x60000?0x3ffff:0;}
    }
    return p;
}
int main(){
    uint32_t state=0x61a09535;uint64_t rows=0,pixels=0,normalizations=0;
    constexpr uint32_t boundaries[]={0,1,31,32,255,256,511,512,1023,1024,0x7fffffff,0x80000000,0xffffffff,0x3fffffff,0x40000000,0xffff0000};
    for(unsigned index=0;index<65536;++index){
        n64::RdpTriangle t;t.flip=index&1;t.shade=t.texture=t.depth=true;t.y[2]=int32_t(index%17)-8;
        t.edge[1]={index<256?boundaries[index%16]:rng(state),index<256?boundaries[index/16]:rng(state)};
        Coefficients c;
        for(unsigned lane=0;lane<8;++lane)for(unsigned group=0;group<4;++group){c[lane][group]=index<256?boundaries[(index+lane*3+group*7)%16]:rng(state);if(lane<4)t.color[group][lane]=c[lane][group];else if(lane<7)t.tex[group][lane-4]=c[lane][group];else t.z[group]=c[lane][group];}
        for(unsigned scan:{2u,7u,31u}){
            const unsigned start=(index*13+scan*7)%1024;n64::RdpTriangleAttributes::Row actual;
            check(n64::RdpTriangleAttributes::row(t,scan,start,actual));const auto expected=oracle_row(t,c,scan,start);
            check(actual.initial==expected.initial && actual.dx==expected.dx && actual.derivative_x==expected.derivative_x && actual.derivative_y==expected.derivative_y && actual.start==expected.start && actual.delta_z==expected.delta_z);++rows;
            for(unsigned x:{0u,1u,255u,511u,1023u})for(unsigned coverage:{0u,255u,unsigned(index&255)}){
                n64::RdpTriangleAttributes::Pixel p;check(n64::RdpTriangleAttributes::pixel(actual,x,uint8_t(coverage),p));const auto want=oracle_pixel(expected,x,coverage);
                check(p.raw==want.raw && p.rgba==want.rgba && p.stw==want.stw && p.z==want.z);++pixels;
            }
        }
    }
    for(unsigned upper=0;upper<65536;++upper)for(unsigned companion:{0u,1u,32767u,32768u,65535u}){
        check(n64::RdpTriangleAttributes::delta_z(upper<<16,companion<<16)==depth_delta(upper<<16,companion<<16));++normalizations;
    }
    n64::RdpTriangleAttributes::Row uniform;uniform.start=0;
    for(unsigned encoded=0;encoded<512;++encoded)for(unsigned mask=0;mask<256;++mask){
        for(unsigned lane=0;lane<4;++lane)uniform.initial[lane]=encoded<<16;
        n64::RdpTriangleAttributes::Pixel p;check(n64::RdpTriangleAttributes::pixel(uniform,0,uint8_t(mask),p));for(auto channel:p.rgba)check(channel==clip9(encoded));
    }
    uint64_t gradients=0,depths=0;
    for(unsigned encoded=0;encoded<8192;++encoded)for(unsigned axis=0;axis<2;++axis)for(unsigned mask=0;mask<256;++mask){
        const int32_t gradient=encoded<4096?int32_t(encoded):int32_t(encoded)-8192;
        uniform.derivative_x[0]=axis?0:gradient;uniform.derivative_y[0]=axis?gradient:0;
        uniform.initial[0]=uint32_t((encoded%512)<<16);
        n64::RdpTriangleAttributes::Pixel p;check(n64::RdpTriangleAttributes::pixel(uniform,0,uint8_t(mask),p));
        const auto expected=oracle_pixel(uniform,0,mask);check(p.rgba==expected.rgba);++gradients;
    }
    for(unsigned encoded=0;encoded<0x400000;++encoded)for(unsigned mask:{1u,2u,255u}){
        uniform.initial[7]=encoded<<10;uniform.derivative_x[7]=1048575;uniform.derivative_y[7]=-1048576;
        n64::RdpTriangleAttributes::Pixel p;check(n64::RdpTriangleAttributes::pixel(uniform,0,uint8_t(mask),p));
        check(p.z==oracle_pixel(uniform,0,mask).z);++depths;
    }
    n64::RdpTriangle t;t.y[2]=8;n64::RdpTriangleAttributes::Row sentinel;sentinel.initial.fill(0x13579bdf);
    for(auto scan:{0u,1u,1024u}){auto r=sentinel;check(!n64::RdpTriangleAttributes::row(t,scan,0,r));check(r.initial==sentinel.initial);}
    auto r=sentinel;check(!n64::RdpTriangleAttributes::row(t,2,1024,r));check(r.initial==sentinel.initial);
    for(int32_t high:{-8193,8192,INT32_MIN,INT32_MAX}){t.y[2]=high;r=sentinel;check(!n64::RdpTriangleAttributes::row(t,2,0,r));check(r.initial==sentinel.initial);}
    n64::RdpTriangleAttributes::Pixel p;p.raw.fill(0x2468ace0);check(!n64::RdpTriangleAttributes::pixel(uniform,1024,255,p));for(auto raw:p.raw)check(raw==0x2468ace0);
    std::cout<<rows<<" incremental row captures; "<<pixels<<" pixel captures; "<<normalizations<<" depth normalizations; "<<gradients<<" signed-gradient corrections; "<<depths<<" depth-code corrections; 131072 constant-shade masks and atomic bounds PASS\n";
}
