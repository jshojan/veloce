#include "dp_interface.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool v){if(!v){std::cerr<<"triangle fill mismatch\n";std::exit(1);}}
static n64::RdpPacket packet(unsigned op){
    n64::RdpPacket p;
    p.push(0,uint64_t(op)<<56|(1ull<<55)|(32ull<<32)|(16ull<<16));
    p.push(8,(4ull<<48)|0xffff0000u);p.push(16,0);p.push(24,65536);
    while(!p.ready())p.push(p.count*8,0x2900000000000000ull+p.count);
    return p;
}
int main(){
    const uint64_t image=(3ull<<51)|(7ull<<32)|64,mode=3ull<<52,scissor=(32ull<<12)|32;
    const unsigned lengths[]={1,2,3,4,5,4,3,2};
    for(unsigned op=8;op<16;++op)for(unsigned alias=0;alias<4;++alias)for(unsigned field=0;field<3;++field){
        auto p=packet(op+alias*64);std::vector<uint8_t> ram(2048,0x5a),expected=ram;
        uint64_t clip=scissor;if(field)clip|=(1ull<<25)|(uint64_t(field==2)<<24);
        for(unsigned y=0;y<8;++y)if(!field || (y&1)==unsigned(field==2))for(unsigned x=0;x<lengths[y];++x)for(unsigned b=0;b<4;++b)expected[64+(y*8+x)*4+b]=uint8_t(0x11223344u>>(24-8*b));
        check(n64::RdpTriangleFill::draw(p,image,mode,clip,0x11223344,ram)==0 && ram==expected);
        for(unsigned raw: {0x10u,0x20u,0x40u,0x50u}){auto before=ram;check(n64::RdpTriangleFill::draw(p,image,mode|raw,clip,0,ram)==3 && ram==before);}
        auto before=ram;check(n64::RdpTriangleFill::draw(p,image|1,mode,clip,0,ram)==4 && ram==before);
        check(n64::RdpTriangleFill::draw(p,image,0,clip,0,ram)==3 && ram==before);
        auto small=std::vector<uint8_t>(200,0x5a);const auto old=small;check(n64::RdpTriangleFill::draw(p,image,mode,clip,0,small)==5 && small==old);
        // Actual DP parser: coefficients headed by FullSync remain payload.
        n64::DpInterface dp;dp.color_image=image;dp.other_mode=mode;dp.scissor=clip;dp.fill_color=0x11223344;ram.assign(2048,0x5a);
        for(unsigned n=0;n<p.count;++n)for(unsigned b=0;b<8;++b)ram[1024+n*8+b]=uint8_t(p.words[n]>>(56-b*8));
        const uint64_t sync=0x2900000000000000ull;for(unsigned b=0;b<8;++b)ram[1024+p.count*8+b]=uint8_t(sync>>(56-b*8));
        auto wanted=ram;for(unsigned n=64;n<320;++n)wanted[n]=expected[n];
        check(dp.write(0,1024) && dp.write(1,1024+p.count*8+8));std::array<uint8_t,8192> sp{};bool irq=false;
        for(unsigned n=0;n<p.count;++n){dp.tick(ram,sp,irq);check(!irq && !dp.blocked);}
        check(ram==wanted && !dp.packet.count);dp.tick(ram,sp,irq);check(irq && !dp.pipeline && dp.current==dp.end);
    }
    // Mirrored geometry: bit 55 clear puts the major edge on the right.
    auto p=packet(8);p.words[0]&=~(1ull<<55);p.words[1]=65536;p.words[2]=4ull<<48;p.words[3]=(4ull<<48)|0xffff0000u;
    const auto rows=n64::RdpTriangleFill::spans([&]{n64::RdpTriangle t;check(n64::RdpTriangle::decode(p,t));return t;}(),scissor);
    const unsigned left[]={3,2,1,0,0,1,2,3};
    for(unsigned y=0;y<8;++y)check(rows[y].valid && rows[y].left==left[y] && rows[y].right==4);
    std::cout<<"96 native packet/field fill dispatches; golden orientations, RAM guards, kill-profile/bounds rejection and payload/real FullSync PASS\n";
}
