#include "rdp_coverage.hpp"
#include <bitset>
#include <cstdlib>
#include <iostream>
static void check(bool v){if(!v){std::cerr<<"coverage mismatch\n";std::exit(1);}}
// Independently enumerated hardware sample coordinates, indexed by mask bit.
constexpr unsigned xs[]={3,2,1,0,3,2,1,0},ys[]={3,2,3,2,1,0,1,0};
static uint8_t point_oracle(const n64::RdpTriangleSpan::Row& r,unsigned pixel){
    if(!r.valid || pixel<r.first || pixel>r.last)return 0;
    unsigned mask=0;
    for(unsigned bit=0;bit<8;++bit){const unsigned point=pixel*8+2*xs[bit];if(r.enabled[ys[bit]] && r.left[ys[bit]]<=point && point<r.right[ys[bit]])mask|=1u<<bit;}
    return uint8_t(mask);
}
int main(){
    for(unsigned mask=0;mask<256;++mask){
        const auto d=n64::RdpCoverage::derivatives(uint8_t(mask));unsigned x=0,y=0;
        if(mask){unsigned best=99;for(unsigned bit=0;bit<8;++bit)if(mask&(1u<<bit)){const unsigned rank=ys[bit]*4+xs[bit];if(rank<best){best=rank;x=xs[bit];y=ys[bit];}}}
        check(d.count==std::bitset<8>(mask).count() && d.cvbit==unsigned(bool(mask&128)) && d.x==x && d.y==y);
    }
    for(unsigned dest=0;dest<4;++dest)for(unsigned blend=0;blend<2;++blend)for(unsigned count=0;count<=8;++count)for(unsigned memory=0;memory<8;++memory){
        const unsigned clamp=blend?std::min(7u,count+memory):count?count-1:7;
        const unsigned expected[]={clamp,(count+memory)%8,7,memory};check(n64::RdpCoverage::stored(dest,blend,count,memory)==expected[dest]);
    }
    uint64_t boundaries=0,mixed=0;
    n64::RdpTriangleSpan::Row row;row.valid=true;row.first=0;row.last=1023;row.enabled.fill(true);
    for(unsigned left=0;left<8192;++left)for(unsigned right=0;right<8192;++right){
        row.left.fill(left);row.right.fill(right);
        for(unsigned pixel:{left/8,right/8}){check(n64::RdpCoverage::mask(row,pixel)==point_oracle(row,pixel));++boundaries;}
    }
    row.first=1;row.last=2;
    for(unsigned active=0;active<16;++active)for(unsigned valid=0;valid<2;++valid)for(unsigned left=0;left<32;++left)for(unsigned right=0;right<32;++right)for(unsigned rotate=0;rotate<4;++rotate){
        row.valid=valid;
        for(unsigned sub=0;sub<4;++sub){row.enabled[sub]=(active>>sub)&1;row.left[sub]=(left+sub*3+rotate)%32;row.right[sub]=(right+sub*5+rotate)%32;}
        for(unsigned pixel=0;pixel<5;++pixel){check(n64::RdpCoverage::mask(row,pixel)==point_oracle(row,pixel));++mixed;}
    }
    n64::RdpTriangle t;t.flip=true;t.y={32,16,0};t.edge={{{4u<<16,0xffff0000},{0,0},{0,65536}}};
    const uint64_t scissor=(32ull<<12)|32;
    const unsigned full[]={0,1,2,3,3,2,1,0};
    for(unsigned scan=0;scan<8;++scan){const auto r=n64::RdpTriangleSpan::row(t,scissor,scan);check(r.valid);
        for(unsigned pixel=0;pixel<8;++pixel){const unsigned expected=pixel<full[scan]?255:pixel==full[scan]?(scan<4?12:232):0;check(n64::RdpCoverage::mask(r,pixel)==expected);}
    }
    std::cout<<boundaries<<" boundary-pair masks; "<<mixed<<" mixed-row/range masks; all 256 derivatives, 576 destination states and golden triangle masks PASS\n";
}
