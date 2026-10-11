#include "dp_interface.hpp"
#include <cstdlib>
#include <iostream>
using Blender=n64::RdpBlender;
using Depth=n64::RdpDepth;
static void check(bool v){if(!v){std::cerr<<"depth pipeline mismatch\n";std::exit(1);}}
static constexpr unsigned edges[]={0,131072,196608,229376,245760,253952,258048,260096,262144};
static constexpr unsigned steps[]={64,32,16,8,4,2,1,1};
static unsigned compress(unsigned z){z%=262144;unsigned band=0;while(z>=edges[band+1])++band;return (band*2048+(z-edges[band])/steps[band])*4;}
static unsigned decompress(unsigned word){const unsigned n=word/4%16384;return (edges[n/2048]+n%2048*steps[n/2048])%262144;}
static unsigned encoding(unsigned value){unsigned out=0;for(unsigned n=0;n<16;++n)if(value/(1u<<n)%2)out|=n;return out;}
static unsigned highest(unsigned value){unsigned out=0;while(value){out=out?out*2:1;value/=2;}return out;}
static Depth::Result depth_oracle(bool enabled,unsigned kind,bool aa,bool force,unsigned z,unsigned delta,unsigned word,unsigned hidden,unsigned count,unsigned memory){
 Depth::Result out;out.coverage=count;out.overflow=(count+memory)/8%2;
 const unsigned dzcode=encoding(delta);z%=262144;
 if(!enabled){out.pass=true;out.blend=force || (aa && !out.overflow);out.shift_b=dzcode<11?4:15-dzcode;return out;}
 out.stored_delta=(word%4)*4+hidden;
 out.shift_a=dzcode>out.stored_delta?std::min(4u,dzcode-out.stored_delta):0;
 out.shift_b=out.stored_delta>dzcode?std::min(4u,out.stored_delta-dzcode):0;
 unsigned stored=1u<<out.stored_delta;const unsigned precision=word/8192;bool coplanar=false;
 if(precision<3){if(stored==32768){stored=65535;coplanar=true;}else stored=std::max(stored*2,16u/(1u<<precision));}
 const unsigned tolerance=highest(delta|stored)*8,old=decompress(word);
 const bool front=z<old,far=coplanar || z+tolerance>=old,near=coplanar || int64_t(z)-tolerance<=old,max=old==262143;
 out.blend=force || (aa && !out.overflow && far);
 if(kind==2)out.pass=front || max;
 else if(kind==3)out.pass=far && near && !max;
 else if(kind==1 && front && far && out.overflow){const unsigned divisor=1u<<encoding(tolerance/8);out.coverage=((old/divisor-z/divisor)%16*count/8)%16;out.pass=true;}
 else out.pass=max || (out.overflow?front:near);
 return out;
}
static unsigned quotient(unsigned index){
 const int denominator=int(index/2048),numerator=int(index%2048);
 int remainder=(16-denominator+numerator/256)%8;bool carry=false;unsigned result=0;
 for(int bit=7;bit>=0;--bit){const int trial=2*remainder+(numerator/(1<<bit))%2+(carry?16-denominator:denominator);carry=trial>=16;remainder=trial%8;if(carry)result+=1u<<bit;}
 return result;
}
static const auto divisions=[](){std::array<unsigned,32768> out{};for(unsigned i=0;i<32768;++i)out[i]=quotient(i);return out;}();
struct Expected {bool write=false,enabled=false;std::array<unsigned,3> rgb{};unsigned coverage=0,latch=0;};
static std::array<unsigned,4> bytes(unsigned word){return {word/16777216,word/65536%256,word/256%256,word%256};}
static Expected oracle(uint64_t mode,Blender::Color color,unsigned shade,unsigned memory,unsigned count,unsigned cvbit,unsigned dz,unsigned fog,unsigned blend,unsigned latch,const Blender::DepthInput& depth){
 Expected out;const bool aa=mode%16>=8,force=mode%32768>=16384,read=mode%128>=64;
 const unsigned mem=read?memory/32%8:7;out.latch=read?memory/256:latch;
 const unsigned gain=((color[3]==255?256:color[3])*count+4)/8;
 if(mode/4096%2)count=gain/32%16;
 if(mode/8192%2)color[3]=std::min(255u,mode/4096%2?gain:count*32);
 const auto result=depth_oracle(mode&16,unsigned(mode>>10)%4,aa,force,depth.z,dz,depth.word,depth.hidden,count,mem);
 count=result.coverage;const bool wrap=result.overflow;out.enabled=result.blend;
 const unsigned dest=unsigned(mode/256)%4;
 if(dest==0){const unsigned value=out.enabled?count+mem:count-1;out.coverage=value/8%2?7:value%8;}
 else out.coverage=dest==1?(mem+count)%8:dest==2?7:mem;
 const bool accepted=result.pass && (!(mode%2) || color[3]>=blend%256) && (aa?count!=0:cvbit!=0);
 const unsigned p=unsigned(mode/(1ull<<30))%4,a=unsigned(mode/(1ull<<26))%4,m=unsigned(mode/(1ull<<22))%4,b=unsigned(mode/(1ull<<18))%4;
 const std::array<Blender::Color,4> colors={color,bytes(out.latch*256+mem*32),bytes(blend),bytes(fog)};
 const unsigned alpha=a==0?color[3]:a==1?fog%256:a==2?shade:0;
 const unsigned beta=b==0?255-alpha:b==1?mem*32:b==2?255:0;
 unsigned first=alpha/8,second=beta/8;
 if(b==1){first=first/(1u<<result.shift_a)/4*4;second=second/(1u<<result.shift_b)|3;}
 for(unsigned lane=0;lane<3;++lane){
  if(mode/128%2 && !wrap)out.rgb[lane]=colors[m][lane];
  else if(!out.enabled || (a==0 && b==0 && color[3]==255))out.rgb[lane]=colors[p][lane];
  else{const unsigned sum=colors[p][lane]*first+colors[m][lane]*(second+1);out.rgb[lane]=force?sum/32%256:divisions[(first/4*4+second/4*4+4)*512+(sum/4)%2048];}
 }
 out.write=accepted;return out;
}

int main(){uint64_t decisions=0;
 for(unsigned word=0;word<65536;++word)for(unsigned bits=0;bits<4;++bits)for(unsigned kind=0;kind<4;++kind)for(unsigned partner=0;partner<8;++partner){
  constexpr unsigned dz[]={1,3,7,16,255,1024,32768,65535};
  const unsigned z=partner%4==0?decompress(word):partner%4==1?(decompress(word)+1)%262144:partner%4==2?word*4:262143;
  const uint64_t mode=(1ull<<18)|(1ull<<22)|(kind<<10)|64|16|(partner&1?8:0)|(partner&2?16384:0)|(partner&4?4096|8192:0)|(word%4<<8)|(word%2);
  const Blender::Color color={word%256,word/256,(word*17)%256,(word*73+partner)%256};
  Blender state;state.fog=0x17c96b83;state.blend=0xe143297f;state.memory_rgb=0x614f33;
  Blender::DepthInput depth{z,uint16_t(word),bits};const unsigned count=partner+1,memory=0x529ba800+((word%8)*32+17),cv=word%2;
  const auto want=oracle(mode,color,word%256,memory,count,cv,dz[partner],state.fog,state.blend,state.memory_rgb,depth);
  const auto got=state.pixel(mode,color,word%256,memory,count,cv,dz[partner],&depth);
  check(got.write==want.write && got.enabled==want.enabled && got.coverage==want.coverage && got.rgb==want.rgb && state.memory_rgb==want.latch);++decisions;
 }
 n64::RdpPacket packet;packet.count=packet.needed=14;packet.words[0]=0xcd80001000080000ull;
 packet.words[1]=0x00020000ffff0000ull;packet.words[2]=0;packet.words[3]=65536;packet.words[4]=0x0080008000800080ull;packet.words[12]=uint64_t(65536u<<13)<<32;
 n64::RdpCombiner combiner;combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(4ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|4;
 const uint64_t image=(2ull<<51)|(3ull<<32)|64,mode=(15ull<<36)|16384|512|64,scissor=(16ull<<12)|16;
 for(unsigned flags:{16u,32u,48u}){
  std::vector<uint8_t> ram(256,255);for(unsigned at=128;at<160;at+=2){ram[at]=0xe0;ram[at+1]=1;}
  n64::HiddenRdram h(256);check(h.ordinary_burst(0,ram.data(),ram.size()));n64::RdpBlender state;
  check(n64::RdpTriangleShade::draw(packet,image,mode|flags,scissor,combiner,ram,nullptr,nullptr,nullptr,&state,&h,128)==0);
  for(unsigned n:{4u,8u,9u,12u}){const unsigned at=64+n*2,za=128+n*2;check(unsigned(ram[at])*256+ram[at+1]==0x8421 && h.halfword(at)==3);
   check(unsigned(ram[za])*256+ram[za+1]==((flags&32)?compress(65536):0xe001) && h.halfword(za)==((flags&32)?0u:3u));}
 }
 for(unsigned flags:{16u,32u,48u}){
  std::vector<uint8_t> ram(512,255);for(unsigned at=192;at<224;at+=2){ram[at]=0xe0;ram[at+1]=1;}
  n64::HiddenRdram h(512);check(h.ordinary_burst(0,ram.data(),ram.size()));n64::RdpBlender state;
  const uint64_t image32=(3ull<<51)|(3ull<<32)|64;
  check(n64::RdpTriangleShade::draw(packet,image32,mode|flags,scissor,combiner,ram,nullptr,nullptr,nullptr,&state,&h,192)==0);
  for(unsigned n:{4u,8u,9u,12u}){const unsigned at=64+n*4,za=192+n*2;
   check(ram[at]==128 && ram[at+1]==128 && ram[at+2]==128 && ram[at+3]==224 && h.halfword(at)==0 && h.halfword(at+2)==0);
   check(unsigned(ram[za])*256+ram[za+1]==((flags&32)?compress(65536):0xe001) && h.halfword(za)==((flags&32)?0u:3u));}
 }
 // Rejection must preserve all visible state including the retained RGB latch.
 for(unsigned base:{129u,132u,240u}){std::vector<uint8_t> ram(256,255);const auto before=ram;n64::HiddenRdram h(256);const auto metadata=h.snapshot();n64::RdpBlender state;state.memory_rgb=0x123456;const auto latch=state.snapshot();
  check(n64::RdpTriangleShade::draw(packet,image,mode|48,scissor,combiner,ram,nullptr,nullptr,nullptr,&state,&h,base)==(base==240?5u:4u) && ram==before && h.snapshot()==metadata && state.snapshot()==latch);
 }
 // When color and Z alias, the accepted color write precedes the Z write;
 // the latter must preserve the color history rather than replacing it.
 {std::vector<uint8_t> ram(256,255);n64::HiddenRdram h(256);check(h.ordinary_burst(0,ram.data(),ram.size()));n64::RdpBlender state;
  check(n64::RdpTriangleShade::draw(packet,image,mode|48,scissor,combiner,ram,nullptr,nullptr,nullptr,&state,&h,64)==0);
  for(unsigned n:{4u,8u,9u,12u}){check(unsigned(ram[64+n*2])*256+ram[65+n*2]==compress(65536) && h.halfword(64+n*2)==0);check(((h.snapshot().history>>(2*(n%8)))&3)==3);}
 }
 // Exercise the settled DP command dispatcher and retained depth-image address.
 {std::vector<uint8_t> ram(512,255);n64::HiddenRdram h(512);check(h.ordinary_burst(0,ram.data(),ram.size()));n64::DpInterface dp;
  dp.color_image=image;dp.other_mode=mode|48;dp.scissor=scissor;dp.combiner=combiner;
  std::vector<uint64_t> words={0xfe00000000000080ull};for(unsigned n=0;n<14;++n)words.push_back(packet.words[n]);words.push_back(0xe900000000000000ull);
  unsigned at=256;for(auto word:words){for(int byte=7;byte>=0;--byte)ram[at++]=uint8_t(word>>(byte*8));}
  check(dp.write(0,256) && dp.write(1,at));std::array<uint8_t,8192> sp{};bool interrupt=false;
  for(unsigned n=0;n<words.size();++n)dp.tick(ram,sp,interrupt,&h);
  check(interrupt && !dp.blocked && dp.current==at && dp.depth_image==128);
  for(unsigned n:{4u,8u,9u,12u})check(unsigned(ram[128+n*2])*256+ram[129+n*2]==compress(65536));
 }
 std::cout<<decisions<<" independent combined depth/blender decisions; triangle compare/update, alias ordering and atomic bounds PASS\n";
}
