#pragma once
#include "rdp_triangle_span.hpp"
namespace n64 {
// Eight native subpixel samples, encoded in the RDP's compact coverage byte.
struct RdpCoverage {
    struct Derivatives {unsigned count=0,cvbit=0,x=0,y=0;};
    static uint8_t mask(const RdpTriangleSpan::Row& row,unsigned pixel){
        if(!row.valid || pixel<row.first || pixel>row.last)return 0;
        unsigned result=0,base=pixel*8;
        for(unsigned sub=0;sub<4;++sub)if(row.enabled[sub]){
            const unsigned first=row.left[sub]<=base?0:std::min(4u,(row.left[sub]-base+1)/2);
            const unsigned end=row.right[sub]<=base?0:std::min(4u,(row.right[sub]-base+1)/2);
            const unsigned bits=(15u>>first)&(240u>>end)&(10u>>(sub&1));
            result|=bits<<(sub<2?4:0);
        }
        return uint8_t(result);
    }
    static Derivatives derivatives(uint8_t mask){
        Derivatives result;result.cvbit=mask>>7;
        for(unsigned bit=0;bit<8;++bit)result.count+=(mask>>bit)&1;
        if(!mask)return result;
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){
            if((x&1)!=(y&1))continue;
            const unsigned bit=(y<2?4:0)+(3-x);
            if(mask&(1u<<bit)){result.x=x;result.y=y;return result;}
        }
        return result;
    }
    static unsigned stored(unsigned destination,bool blend,unsigned count,unsigned memory){
        switch(destination&3){
        case 0:{const unsigned sum=blend?count+memory:count-1;return (sum&8)?7:sum&7;}
        case 1:return (count+memory)&7;
        case 2:return 7;
        default:return memory;
        }
    }
};
}
