#pragma once
#include <array>
#include <cstdint>
namespace n64 {
// RDP ST/W reciprocal interpolation. Returns 17 coordinate bits plus underflow
// (bit 17) and overflow (bit 18); saturation precedes the tile coordinate path.
// Integer endpoint rounding has one native exception at segment six.
struct RdpTextureDivider {
    struct Pair {uint32_t s=0,t=0;};
    static Pair divide(uint16_t s,uint16_t t,uint16_t w,bool perspective){
        if(!perspective)return {uint32_t(signed16(s))&131071,uint32_t(signed16(t))&131071};
        const auto entry=table()[w&32767];
        return {axis(s,w,entry),axis(t,w,entry)};
    }
    static uint16_t saturate(uint32_t value){
        if(value&262144)return 32767;
        if(value&131072)return 32768;
        const int32_t coordinate=int32_t(value&131071)-((value&65536)?131072:0);
        return coordinate>32767?32767:coordinate<-32768?32768:uint16_t(coordinate);
    }
private:
    struct Reciprocal {unsigned shift=0,value=0;};
    static int32_t signed16(uint16_t raw){return raw<32768?int32_t(raw):int32_t(raw)-65536;}
    static int64_t floor_shift(int64_t value,unsigned shift){
        // For negative values, -1-value is nonnegative even at INT64_MIN.
        // This computes floor(value / 2^shift) without signed division or a
        // platform-dependent right shift of a negative integer.
        return value>=0 ? value>>shift : -1-((-1-value)>>shift);
    }
    static unsigned point(unsigned segment){return segment==6?14979:(1048576+(64+segment)/2)/(64+segment);}
    static const std::array<Reciprocal,32768>& table(){
        static const auto entries=[](){std::array<Reciprocal,32768> result{};
            for(unsigned w=0;w<32768;++w){unsigned shift=0;while(shift<14 && !(w&(16384u>>shift)))++shift;
                const unsigned normalized=(w<<shift)&16383,segment=normalized>>8;
                const int64_t slope=int64_t(point(segment+1))-point(segment);
                result[w]={shift,unsigned(int64_t(point(segment))+floor_shift(slope*int64_t(normalized&255),8))&32767};
            }return result;
        }();return entries;
    }
    static uint32_t axis(uint16_t coordinate,uint16_t w,Reciprocal reciprocal){
        const int64_t product=int64_t(signed16(coordinate))*reciprocal.value;
        const uint32_t mask=0x3fffffffu&(~0u<<(29-reciprocal.shift));
        const uint32_t selected=uint32_t(product)&mask;
        const int64_t scaled=reciprocal.shift==14?product*2:floor_shift(product,13-reciprocal.shift);
        const int64_t sign_source=reciprocal.shift==14?product:scaled;
        uint32_t flags=selected && selected!=mask?((uint32_t(sign_source)&(1u<<29))?131072:262144):0;
        if(!w || (w&32768))flags|=262144;
        return (uint32_t(scaled)&131071)|flags;
    }
};
}
