#include "rdp_triangle_edges.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool v){if(!v){std::cerr<<"edge walk mismatch\n";std::exit(1);}}
static uint32_t sx(uint32_t raw,unsigned bits){raw&=(uint32_t(1)<<bits)-1;return raw&(uint32_t(1)<<(bits-1))?raw|~((uint32_t(1)<<bits)-1):raw;}
static uint32_t delta(uint32_t raw){const uint32_t s=sx(raw,30);return ((s>>2)|(s&0x80000000?0xc0000000:0))&~1u;}
int main(){
    uint64_t captures=0;
    const uint32_t inputs[]={0,1,2,3,4,7,0x1fff,0x2000,0xffff,0x10000,0x7ffffff,0x8000000,0xfffffff,0x1fffffff,0x20000000,0x3fffffff,0x7fffffff,0x80000000,0xffffffff};
    for(auto x:inputs)for(auto d:inputs)for(int top=-7;top<=7;++top)for(int mid=-9;mid<=9;++mid){
        n64::RdpTriangle t;t.y={20,mid,top};t.edge={{{x,d},{x^0xf0000000,d^0xc0000000},{~x,~d}}};
        const int start=top>=0?top/4*4:-((-top+3)/4)*4;
        uint32_t major=sx(t.edge[1][0],28)&~1u,minor=sx(t.edge[2][0],28)&~1u;
        uint32_t minorstep=delta(t.edge[2][1]);
        for(int y=start;y<=24;++y){
            if(y==mid){minor=sx(t.edge[0][0],28)&~1u;minorstep=delta(t.edge[0][1]);}
            n64::RdpTriangleEdges::Sample actual;check(n64::RdpTriangleEdges::sample(t,y,actual));
            check(actual.major==major && actual.minor==minor);++captures;
            major+=delta(t.edge[1][1]);minor+=minorstep;
        }
        n64::RdpTriangleEdges::Sample sentinel{123,456};check(!n64::RdpTriangleEdges::sample(t,start-1,sentinel) && sentinel.major==123 && sentinel.minor==456);
    }
    // All encoded slope low halfwords with a negative sign-bearing upper half.
    for(unsigned half=0;half<65536;++half){n64::RdpTriangle t;t.y={8191,0,-8192};t.edge={{{0x8000000,0x20000000|half},{0x7ffffff,0x1fff0000|half},{0xffffffff,0xffff0000|half}}};
        uint32_t major=sx(t.edge[1][0],28)&~1u,minor=sx(t.edge[2][0],28)&~1u;uint32_t inc=delta(t.edge[2][1]);
        for(int y=-8192;y<8192;++y){if(y==0){minor=sx(t.edge[0][0],28)&~1u;inc=delta(t.edge[0][1]);}if((y&255)==0 || y==-1 || y==8191){n64::RdpTriangleEdges::Sample a;check(n64::RdpTriangleEdges::sample(t,y,a));check(a.major==major && a.minor==minor);++captures;}major+=delta(t.edge[1][1]);minor+=inc;}
    }
    std::cout<<captures<<" quarter-edge captures match independent modular incremental walk PASS\n";
}
