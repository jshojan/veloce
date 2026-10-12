#include "rdp_texture_coordinates.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool v){if(!v){std::cerr<<"texture coordinate mismatch\n";std::exit(1);}}
static int32_t sign(uint32_t value){value%=65536;return value<32768?int32_t(value):int32_t(value)-65536;}
static int32_t floor_div(int32_t value,int32_t scale){const int32_t q=value/scale;return q-(value%scale<0);}
static int32_t modulo(int32_t value,int32_t period){return (value%period+period)%period;}
static n64::RdpTextureCoordinates::Axis oracle(uint16_t raw,unsigned shift,unsigned low,unsigned high,bool clamp,unsigned mask,bool mirror,bool t){
    int32_t shifted=shift<11?floor_div(sign(raw),int32_t(1u<<shift)):sign(uint32_t(raw)*(1u<<(16-shift)));
    int32_t relative=shifted-int32_t(low*8);
    unsigned fraction=unsigned(modulo(relative,32));int32_t position=floor_div(relative,32);
    if(clamp || mask==0){
        if(floor_div(shifted,8)>=int32_t(high)){position=modulo(int32_t(high/4)-int32_t(low/4),1024);fraction=0;}
        else if(relative<0){position=0;fraction=0;}
    }
    int32_t neighbor=position+1;
    if(mask){
        const int32_t period=1<<std::min(mask,10u);
        const bool reverse=mirror && modulo(floor_div(position,period),2)==1;
        position=modulo(position,period);
        if(reverse)position=period-1-position;
        if(mirror)neighbor=reverse?std::max(0,position-1):std::min(period-1,position+1);
        else neighbor=position==period-1?(t?position-position%256:0):position+1;
    }
    return {position,neighbor,fraction};
}
static void compare(const n64::RdpTexture::Tile& tile,bool t,uint16_t raw,unsigned shift,unsigned low,unsigned high,bool clamp,unsigned mask,bool mirror){
    const auto got=n64::RdpTextureCoordinates::axis(tile,t,raw),want=oracle(raw,shift,low,high,clamp,mask,mirror,t);
    check(got.texel==want.texel && got.neighbor==want.neighbor && got.fraction==want.fraction);
}
static n64::RdpTexture::Tile descriptor(bool t,unsigned shift,unsigned low,unsigned high,bool clamp,unsigned mask,bool mirror){
    n64::RdpTexture::Tile tile;
    tile.descriptor=(uint64_t(shift)<<(t?10:0))|(uint64_t(mask)<<(t?14:4))|(uint64_t(mirror)<<(t?18:8))|(uint64_t(clamp)<<(t?19:9));
    tile.bounds=(uint64_t(low)<<(t?32:44))|(uint64_t(high)<<(t?0:12));return tile;
}
int main(){
    uint64_t domain=0,bounds=0,pairs=0;
    for(unsigned t=0;t<2;++t)for(unsigned shift=0;shift<16;++shift)for(unsigned mask=0;mask<16;++mask)for(unsigned flags=0;flags<4;++flags){
        const bool clamp=flags&1,mirror=flags&2;const auto tile=descriptor(t,shift,5,77,clamp,mask,mirror);
        for(unsigned raw=0;raw<65536;++raw){compare(tile,t,uint16_t(raw),shift,5,77,clamp,mask,mirror);++domain;}
    }
    // Every 12-bit origin/upper-bound pair, four raw probes from the unshifted
    // bounds, under deterministically varied shift/mask/flag settings.
    for(unsigned t=0;t<2;++t)for(unsigned low=0;low<4096;++low)for(unsigned high=0;high<4096;++high){
        const unsigned shift=(low+high)%16,mask=(low^high)%16,flags=(low*3+high)%4;
        const bool clamp=flags&1,mirror=flags&2;const auto tile=descriptor(t,shift,low,high,clamp,mask,mirror);
        for(unsigned raw:{low*8,(low*8+65535)%65536,high*8,(high*8+65535)%65536}){compare(tile,t,uint16_t(raw),shift,low,high,clamp,mask,mirror);++bounds;}
    }
    for(unsigned raw=0;raw<65536;++raw){
        n64::RdpTexture::Tile tile;
        const unsigned shifts=raw&15,shiftt=(raw>>4)&15,masks=(raw>>8)&15,maskt=(raw>>12)&15;
        tile.descriptor=shifts|(uint64_t(shiftt)<<10)|(uint64_t(masks)<<4)|(uint64_t(maskt)<<14)|(1ull<<8)|(1ull<<19);
        tile.bounds=(7ull<<44)|(13ull<<32)|(117ull<<12)|221;
        const auto got=n64::RdpTextureCoordinates::sample(tile,uint16_t(raw),uint16_t(raw^0xa55a));
        const auto s=oracle(uint16_t(raw),shifts,7,117,false,masks,true,false),t=oracle(uint16_t(raw^0xa55a),shiftt,13,221,true,maskt,false,true);
        check(got.s.texel==s.texel && got.s.neighbor==s.neighbor && got.s.fraction==s.fraction && got.t.texel==t.texel && got.t.neighbor==t.neighbor && got.t.fraction==t.fraction);++pairs;
    }
    uint64_t simple=0;
    for(unsigned mask:{4u,5u,6u,8u})for(unsigned high:{0u,63u,4095u})for(unsigned flags=0;flags<4;++flags){
        const bool clamp_s=flags&1,clamp_t=flags&2;
        auto tile=descriptor(false,0,0,high,clamp_s,mask,false);
        tile.descriptor|=uint64_t(mask)<<14|uint64_t(clamp_t)<<19;
        tile.bounds|=uint64_t(high);
        const auto setup=n64::RdpTextureCoordinates::prepare_simple(tile);
        check(setup.valid);
        for(unsigned raw=0;raw<65536;++raw){
            const auto got=n64::RdpTextureCoordinates::sample_simple(setup,uint16_t(raw),uint16_t(raw^0xa55a));
            const auto s=oracle(uint16_t(raw),0,0,high,clamp_s,mask,false,false);
            const auto t=oracle(uint16_t(raw^0xa55a),0,0,high,clamp_t,mask,false,true);
            check(got.s.texel==s.texel && got.s.neighbor==s.neighbor && got.s.fraction==s.fraction && got.t.texel==t.texel && got.t.neighbor==t.neighbor && got.t.fraction==t.fraction);
            ++simple;
        }
    }
    for(auto tile:{descriptor(false,1,0,63,false,5,false),descriptor(false,0,1,63,false,5,false),
                   descriptor(false,0,0,63,false,0,false),descriptor(false,0,0,63,false,9,false),
                   descriptor(false,0,0,63,false,5,true)}){
        tile.descriptor|=uint64_t(5)<<14;
        tile.bounds|=uint64_t(63);
        check(!n64::RdpTextureCoordinates::prepare_simple(tile).valid);
    }
    // Explicit unequal S/T wrap and mirror endpoint goldens.
    auto tile=descriptor(false,0,0,4095,false,10,false);
    check(n64::RdpTextureCoordinates::axis(tile,false,32736).neighbor==0);
    tile=descriptor(true,0,0,4095,false,10,false);
    check(n64::RdpTextureCoordinates::axis(tile,true,32736).neighbor==768);
    std::cout<<domain<<" coordinate/shift/mask/flag captures; "<<bounds<<" bound-pair captures; "<<pairs<<" paired-axis captures; "<<simple<<" simple-tile captures; wrap goldens PASS\n";
}
