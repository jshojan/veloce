#include "rdp_triangle_shade.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool v){if(!v){std::cerr<<"textured triangle mismatch\n";std::exit(1);}}
int main(){
    n64::RdpPacket packet;packet.count=packet.needed=12;
    packet.words[0]=0xca80001000080000ull;
    packet.words[1]=0x00020000ffff0000ull;packet.words[2]=0;packet.words[3]=65536;
    n64::RdpTexture texture;n64::RdpTmem tmem;
    texture.tiles[0].descriptor=(3ull<<51)|(1ull<<41);texture.tiles[0].bounds=(4ull<<12)|4;
    // Every texel has RGBA=(64,128,192,128), with canonical odd-row swaps.
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){const unsigned at=((y*4+x)^((y&1)?2:0))*2;
        tmem.bytes[at]=64;tmem.bytes[at+1]=128;tmem.bytes[2048+at]=192;tmem.bytes[2049+at]=128;
    }
    n64::RdpCombiner combiner;combiner.primitive=0xff808080;
    const uint64_t image=(3ull<<51)|(3ull<<32)|64,mode=(15ull<<36)|(3ull<<42),scissor=(16ull<<12)|16;
    unsigned programs=0;
    for(unsigned program=0;program<4;++program){
        // pass TEXEL0; TEXEL0 * primitive alpha; primitive * TEXEL0 alpha;
        // (TEXEL0 - primitive) * TEXEL0 + primitive.
        const unsigned a[]={15,1,3,1},b[]={15,8,8,3},c[]={31,10,8,1},d[]={1,7,7,3};
        combiner.combine=(uint64_t(a[program])<<37)|(uint64_t(b[program])<<24)|(uint64_t(c[program])<<32)|(uint64_t(d[program])<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|1;
        std::vector<uint8_t> ram(160,0x5a),want=ram;
        for(unsigned position:{4u,8u,9u,12u}){const unsigned channels[]={64,128,192};
            for(unsigned lane=0;lane<3;++lane){const unsigned tex=channels[lane],primitive=lane==0?255:128;
                const int value=program==0?int(tex):program==1?int((tex*128+128)/256):program==2?int((primitive*128+128)/256):int((int(tex)-int(primitive))*int(tex)+int(primitive)*256+128)/256;
                want[64+position*4+lane]=uint8_t(value);
            }
            want[64+position*4+3]=uint8_t((position==4 || position==8?7:3)*32);
        }
        check(n64::RdpTriangleShade::draw(packet,image,mode,scissor,combiner,ram,&texture,&tmem)==0 && ram==want);++programs;
    }
    unsigned palettes=0;
    combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(1ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|1;
    auto palette_packet=packet;palette_packet.words[4]=(16ull<<48)|(16ull<<32);
    auto palette_texture=texture;palette_texture.tiles[0].descriptor=(2ull<<53)|(1ull<<51)|(1ull<<41);
    n64::RdpTmem palette_memory;
    palette_memory.bytes[0]=1;palette_memory.bytes[1]=2;palette_memory.bytes[12]=3;palette_memory.bytes[13]=4;
    for(bool intensity:{false,true})for(bool nearest:{false,true}){
        for(unsigned index=1;index<=4;++index)for(unsigned bank=0;bank<4;++bank){const unsigned at=2048+index*8+bank*2;
            const unsigned value=intensity?((index*20+bank*10)*256+128):0xf801;
            palette_memory.bytes[at]=uint8_t(value>>8);palette_memory.bytes[at+1]=uint8_t(value);
        }
        std::vector<uint8_t> ram(160,0x5a),want=ram;
        for(unsigned position:{4u,8u,9u,12u}){
            for(unsigned lane=0;lane<3;++lane)want[64+position*4+lane]=uint8_t(intensity?(nearest?35:65):(lane==0?255:0));
            want[64+position*4+3]=uint8_t((position==4 || position==8?7:3)*32);
        }
        const auto palette_mode=mode|(1ull<<47)|(intensity?1ull<<46:0)|(1ull<<44)|(nearest?0:1ull<<45);
        check(n64::RdpTriangleShade::draw(palette_packet,image,palette_mode,scissor,combiner,ram,&palette_texture,&palette_memory)==0 && ram==want);++palettes;
    }
    combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(1ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|1;
    unsigned rejected=0;
    auto reject=[&](uint64_t m,const n64::RdpCombiner& c){std::vector<uint8_t> ram(160,0x5a);const auto before=ram;check(n64::RdpTriangleShade::draw(packet,image,m,scissor,c,ram,&texture,&tmem)==1 && ram==before);++rejected;};
    const uint64_t allowed=(15ull<<36)|(63ull<<42)|(1ull<<51)|0xffff0000ull|0x340;
    for(unsigned bit=0;bit<56;++bit)if(!(allowed&(1ull<<bit)))reject(mode|(1ull<<bit),combiner);
    reject(mode&~(1ull<<43),combiner);
    // Current/next texel feedback, LOD, key, noise and conversion coefficients
    // are still rejected, even though the pure combiner can route them.
    for(unsigned d:{0u,2u}){auto c=combiner;c.combine=(c.combine&~(7ull<<6))|(uint64_t(d)<<6);reject(mode,c);}
    for(unsigned factor:{6u,7u,9u,13u,15u}){auto c=combiner;c.combine=(1ull<<37)|(8ull<<24)|(uint64_t(factor)<<32)|(1ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|1;reject(mode,c);}
    std::cout<<programs<<" textured combiner framebuffer goldens; "<<palettes<<" TLUT framebuffer goldens; "<<rejected<<" atomic profile rejections PASS\n";
}
