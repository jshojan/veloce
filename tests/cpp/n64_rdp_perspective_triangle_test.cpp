#include "rdp_triangle_shade.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool v){if(!v){std::cerr<<"perspective framebuffer mismatch\n";std::exit(1);}}
int main(){
 n64::RdpPacket packet;packet.count=packet.needed=12;
 packet.words[0]=0xca87001000080000ull;packet.words[1]=0x00020000ffff0000ull;packet.words[3]=65536;
 // Constant S=64,T=96, with W=24576+4096*x-4096*y. Signed gradients
 // traverse the native interpolant path before division at each pixel.
 packet.words[4]=(64ull<<48)|(96ull<<32)|(24576ull<<16);
 packet.words[5]=4096ull<<16;packet.words[8]=packet.words[9]=61440ull<<16;
 n64::RdpTexture texture;auto& tile=texture.tiles[7];tile.descriptor=(2ull<<51)|(2ull<<41)|(3ull<<14)|(3ull<<4);tile.bounds=(28ull<<12)|28;
 n64::RdpTmem tmem;
 for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){const unsigned at=((y*8+x)^((y&1)?2:0))*2;const unsigned v=((x*4)<<11)|((y*4)<<6)|(((x*3+y*5)%32)<<1)|1;tmem.bytes[at]=uint8_t(v>>8);tmem.bytes[at+1]=uint8_t(v);}
 n64::RdpCombiner combiner;combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(1ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|1;
 const uint64_t image=(3ull<<51)|(3ull<<32)|64,mode=(15ull<<36)|(3ull<<42)|(1ull<<51),scissor=(16ull<<12)|16;
 const unsigned positions[]={4,8,9,12};
 const uint8_t goldens[2][4][3]={{{99,132,239},{132,198,82},{99,132,239},{165,0,123}},{{105,158,20},{132,198,82},{105,158,20},{175,0,131}}};
 for(unsigned filter=0;filter<2;++filter){std::vector<uint8_t> ram(160,0x5a),want=ram;
  for(unsigned p=0;p<4;++p){for(unsigned lane=0;lane<3;++lane)want[64+positions[p]*4+lane]=goldens[filter][p][lane];want[64+positions[p]*4+3]=(p==0 || p==1)?224:96;}
  check(n64::RdpTriangleShade::draw(packet,image,mode|(filter?1ull<<45:0),scissor,combiner,ram,&texture,&tmem)==0 && ram==want);
 }
 // LOD, depth, blending and two-cycle profiles remain atomic rejections.
 for(unsigned bit:{48u,49u,50u,52u,53u,4u,5u,14u}){std::vector<uint8_t> ram(160,0x5a);const auto before=ram;check(n64::RdpTriangleShade::draw(packet,image,mode|(1ull<<bit),scissor,combiner,ram,&texture,&tmem)==1 && ram==before);}
 std::cout<<"Varying-W point/filter framebuffer goldens and eight atomic profile rejections PASS\n";
}
