#include "rdp_triangle_shade.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool value){if(!value){std::cerr<<"dither mismatch\n";std::exit(1);}}
static unsigned magic(unsigned x,unsigned y){constexpr unsigned rows[4][4]={{0,6,1,7},{4,2,5,3},{3,5,2,4},{7,1,6,0}};return rows[y%4][x%4];}
static unsigned bayer(unsigned x,unsigned y){constexpr unsigned rows[4][4]={{0,4,1,5},{4,0,5,1},{3,7,2,6},{7,3,6,2}};return rows[y%4][x%4];}
static unsigned round(unsigned color,unsigned seed){return color%8>seed?std::min(255u,color/8*8+8):color;}
#ifdef PRIMARY_DITHER
extern "C" {void reference_dither_seeds(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned*);unsigned reference_dither_channel(unsigned,unsigned);}
#endif
int main(){uint64_t seeds=0,rounds=0,alpha=0,triangles=0;
 for(unsigned rgb=0;rgb<4;++rgb)for(unsigned a=0;a<4;++a){check(n64::RdpDither::supported(rgb,a)==(rgb!=2 && a!=2));if(rgb==2 || a==2)continue;
  for(unsigned x=0;x<32;++x)for(unsigned y=0;y<32;++y)for(bool field:{false,true}){
   const auto got=n64::RdpDither::sample(rgb,a,x,y,field);const unsigned base=rgb==0?magic(x,y/(field?2:1)):bayer(x,y/(field?2:1));
   check(got.rgb==(rgb==3?7:base) && got.alpha==(a==3?0:a==0?base:7-base));
#ifdef PRIMARY_DITHER
   unsigned expected[2]{};reference_dither_seeds(rgb,a,x,y,field,expected);check(got.rgb==expected[0] && got.alpha==expected[1]);
#endif
   ++seeds;
  }
 }
 for(unsigned value=0;value<256;++value)for(unsigned seed=0;seed<8;++seed){check(n64::RdpDither::channel(value,seed)==round(value,seed));
#ifdef PRIMARY_DITHER
 check(n64::RdpDither::channel(value,seed)==reference_dither_channel(value,seed));
#endif
 ++rounds;}
 for(unsigned value=0;value<256;++value)for(unsigned count=0;count<=8;++count)for(unsigned flags=0;flags<4;++flags)for(unsigned seed=0;seed<8;++seed)for(unsigned shade=0;shade<256;++shade){
  n64::RdpBlender blender;blender.blend=127;
  const uint64_t mode=(2ull<<26)|(1ull<<22)|64|16384|512|8|1|(flags%2?4096:0)|(flags/2?8192:0);
  const unsigned gain=((value==255?256:value)*count+4)/8,coverage=flags%2?gain/32%16:count;
  const unsigned opacity=flags/2?std::min(255u,flags%2?gain:coverage*32):std::min(255u,(value==255?256:value)+seed);
  const unsigned a=std::min(255u,shade+seed)/8,b=(255-std::min(255u,shade+seed))/8;
  const auto got=blender.pixel(mode,{37,191,243,value},shade,0x5283a8e0, count,1,1,nullptr,seed);
  check(got.write==(opacity>=127 && coverage!=0) && got.coverage==7 && got.rgb[1]==(191u*a+131u*(b+1))/32%256);++alpha;
 }
 n64::RdpPacket packet;packet.count=packet.needed=12;packet.words[0]=0xcc80001000080000ull;packet.words[1]=0x00020000ffff0000ull;packet.words[3]=65536;
 n64::RdpCombiner combiner;combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(4ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|4;
 const uint64_t scissor=(16ull<<12)|16;
 for(unsigned value=0;value<256;++value)for(unsigned rgb:{0u,1u,3u})for(unsigned a:{0u,1u,3u})for(unsigned size:{2u,3u})for(bool ignored:{false,true}){
  packet.words[4]=uint64_t(value)*0x0001000100010001ull;
  std::vector<uint8_t> ram(160,0x5a);n64::HiddenRdram hidden(160);n64::RdpBlender blender;
  const uint64_t mode=(uint64_t(rgb)<<38)|(uint64_t(a)<<36)|512|(ignored?((3ull<<54)|(15ull<<32)|(1ull<<15)|(2047ull<<41)):0);
  check(n64::RdpTriangleShade::draw(packet,(uint64_t(size)<<51)|(3ull<<32)|64,mode,scissor,combiner,ram,nullptr,nullptr,nullptr,&blender,&hidden)==0);
  const unsigned bytes=size==2?2:4;
  for(unsigned position:{4u,8u,9u,12u}){const unsigned x=position%4,y=position/4,seed=rgb==3?7:rgb==0?magic(x,y):bayer(x,y),color=round(value,seed),at=64+position*bytes;
   if(size==2){check(unsigned(ram[at])*256+ram[at+1]==(color/8*2114+1) && hidden.halfword(at)==3);}
   else check(ram[at]==color && ram[at+1]==color && ram[at+2]==color && ram[at+3]==224 && hidden.halfword(at)==color%2*3);
  }++triangles;
 }

 for(unsigned value=0;value<256;++value)for(unsigned rgb:{0u,1u,3u})for(unsigned ad:{0u,1u,3u})for(unsigned flags=0;flags<4;++flags)for(unsigned size:{2u,3u}){
  packet.words[4]=uint64_t(value)*0x0001000100010001ull;std::vector<uint8_t> ram(160,0x5a);n64::HiddenRdram hidden(160);n64::RdpBlender blender;blender.blend=127;
  const uint64_t mode=(uint64_t(rgb)<<38)|(uint64_t(ad)<<36)|(2ull<<26)|(1ull<<22)|64|16384|512|8|1|(flags%2?4096:0)|(flags/2?8192:0);
  check(n64::RdpTriangleShade::draw(packet,(uint64_t(size)<<51)|(3ull<<32)|64,mode,scissor,combiner,ram,nullptr,nullptr,nullptr,&blender,&hidden)==0);
  for(unsigned position:{4u,8u,9u,12u}){const unsigned x=position%4,y=position/4,base=rgb==0?magic(x,y):bayer(x,y),cd=rgb==3?7:base,seed=ad==3?0:ad==0?base:7-base;
   const unsigned initial=position==4 || position==8?8:4,gain=((value==255?256:value)*initial+4)/8,count=flags%2?gain/32%16:initial;
   const unsigned opacity=flags/2?std::min(255u,flags%2?gain:count*32):std::min(255u,(value==255?256:value)+seed);
   const unsigned a=std::min(255u,value+seed)/8,b=(255-std::min(255u,value+seed))/8,bytes=size==2?2:4,at=64+position*bytes;
   if(opacity<127 || !count){for(unsigned n=0;n<bytes;++n)check(ram[at+n]==0x5a);continue;}
   std::array<unsigned,3> color{};const std::array<unsigned,3> memory=size==2?std::array<unsigned,3>{88,72,104}:std::array<unsigned,3>{90,90,90};
   for(unsigned lane=0;lane<3;++lane)color[lane]=round((value*a+memory[lane]*(b+1))/32%256,cd);
   if(size==2)check(unsigned(ram[at])*256+ram[at+1]==color[0]/8*2048+color[1]/8*64+color[2]/8*2+1);
   else check(ram[at]==color[0] && ram[at+1]==color[1] && ram[at+2]==color[2] && ram[at+3]==224);
  }++triangles;
 }
 // Rejected RGB still passes through dithering before retained color history.
 {packet.words[4]=0x000d000d000d000dull;std::vector<uint8_t> ram(160,0x5a);const auto before=ram;n64::HiddenRdram hidden(160);n64::RdpBlender blender;blender.blend=255;
  check(n64::RdpTriangleShade::draw(packet,(3ull<<51)|(3ull<<32)|64,(3ull<<36)|512|1,scissor,combiner,ram,nullptr,nullptr,nullptr,&blender,&hidden)==0);
  check(ram==before && hidden.snapshot().history==3);
 }
 std::cout<<seeds<<" periodic/field matrix seeds; "<<rounds<<" complete channel rounds; "<<alpha<<" alpha/coverage/shade decisions; "<<triangles<<" RGBA16/RGBA32 triangles and ignored fields PASS\n";
}
