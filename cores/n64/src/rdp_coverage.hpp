#pragma once
#include "rdp_triangle_span.hpp"
#include <array>
namespace n64 {
// Packed count/x/y for all eight-bit sample masks. The coverage bit is the
// high bit of the mask itself. This table replaces repeated pixel scans.
inline constexpr auto coverage_derivatives_table=[] {
    std::array<uint16_t,256> table{};
    for(unsigned mask=0;mask<256;++mask){
        unsigned count=0,x=0,y=0;
        for(unsigned bit=0;bit<8;++bit)count+=(mask>>bit)&1;
        bool found=false;
        for(unsigned row=0;row<4 && !found;++row)for(unsigned col=0;col<4;++col){
            if((col&1)!=(row&1))continue;
            const unsigned bit=(row<2?4:0)+(3-col);
            if(mask&(1u<<bit)){x=col;y=row;found=true;break;}
        }
        table[mask]=uint16_t(count|(x<<4)|(y<<6));
    }
    return table;
}();
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
        const unsigned packed=coverage_derivatives_table[mask];
        return {packed&15u,unsigned(mask>>7),(packed>>4)&3u,(packed>>6)&3u};
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
