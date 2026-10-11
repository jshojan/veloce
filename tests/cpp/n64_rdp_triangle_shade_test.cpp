#include "rdp_triangle_shade.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool value){if(!value){std::cerr<<"shade framebuffer mismatch\n";std::exit(1);}}
int main(){
    n64::RdpPacket packet;packet.count=packet.needed=12;
    packet.words[0]=0xcc80001000080000ull;
    packet.words[1]=0x00020000ffff0000ull;packet.words[2]=0;packet.words[3]=65536;
    packet.words[4]=0x0080008000800080ull;
    n64::RdpCombiner combiner;
    combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(4ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|4;
    const uint64_t image=(3ull<<51)|(3ull<<32)|64,mode=15ull<<36,scissor=(16ull<<12)|16;
    const std::vector<uint8_t> pristine(160,0x5a);
    for(unsigned dest=0;dest<4;++dest)for(unsigned read=0;read<2;++read){
        auto ram=pristine,expected=pristine;
        for(unsigned position:{4u,8u,9u,12u}){
            const unsigned count=position==4 || position==8?8:4;
            const unsigned memory=read?2:7;
            const unsigned coverage=dest==0?count-1:dest==1?(count+memory)%8:dest==2?7:memory;
            for(unsigned lane=0;lane<3;++lane)expected[64+position*4+lane]=128;
            expected[64+position*4+3]=uint8_t(coverage*32);
        }
        check(n64::RdpTriangleShade::draw(packet,image,mode|(dest<<8)|(read<<6),scissor,combiner,ram)==0);check(ram==expected);
    }
    unsigned rejections=0;
    const auto reject=[&](const n64::RdpPacket& p,uint64_t img,uint64_t m,const n64::RdpCombiner& c,unsigned reason,unsigned bytes=160){
        std::vector<uint8_t> ram(bytes,0x5a);const auto before=ram;
        check(n64::RdpTriangleShade::draw(p,img,m,scissor,c,ram)==reason);check(ram==before);++rejections;
    };
    const uint64_t allowed=(15ull<<36)|0xffff0000ull|0x340;
    for(unsigned bit=0;bit<56;++bit)if(!(allowed&(1ull<<bit)))reject(packet,image,mode|(1ull<<bit),combiner,1);
    for(unsigned bit=36;bit<40;++bit)reject(packet,image,mode^(1ull<<bit),combiner,1);
    reject(packet,image,mode|(1ull<<30),combiner,1);reject(packet,image,mode|(1ull<<31),combiner,1);
    reject(packet,image+1,mode,combiner,4);reject(packet,image,mode,combiner,5,75);
    reject(packet,image|(1ull<<53),mode,combiner,1);
    reject(packet,image^(1ull<<51),mode,combiner,1);
    auto broken=packet;broken.count=11;reject(broken,image,mode,combiner,1);
    for(unsigned source=0;source<3;++source){auto c=combiner;c.combine=(c.combine&~(7ull<<6))|(uint64_t(source)<<6);reject(packet,image,mode,c,1);}
    std::cout<<"Eight framebuffer/coverage goldens; "<<rejections<<" atomic profile/packet/bounds rejections PASS\n";
}
