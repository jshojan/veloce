#include "dp_interface.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool value){if(!value){std::cerr<<"primitive depth mismatch\n";std::exit(1);}}
static unsigned compressed(unsigned z){
 constexpr unsigned edges[]={0,131072,196608,229376,245760,253952,258048,260096,262144};
 constexpr unsigned steps[]={64,32,16,8,4,2,1,1};unsigned band=0;while(z>=edges[band+1])++band;
 return (band*2048+(z-edges[band])/steps[band])*4;
}
static unsigned encoded(unsigned delta){unsigned out=0;for(unsigned n=0;n<16;++n)if(delta/(1u<<n)%2)out|=n;return out;}
static uint32_t operand(unsigned raw,unsigned field,unsigned partner){
 constexpr unsigned zs[]={0,1,16384,32767},ds[]={0,3,7,65535};
 return field?(zs[partner]*65536+raw):(raw*65536+ds[partner]);
}
int main(){uint64_t commands=0,pixels=0;
 std::vector<uint8_t> ram(4096);std::array<uint8_t,8192> sp{};
 for(unsigned raw=0;raw<65536;++raw)for(unsigned field=0;field<2;++field)for(unsigned partner=0;partner<4;++partner)for(unsigned alias=0;alias<4;++alias)for(unsigned source=0;source<2;++source){
  const auto value=operand(raw,field,partner);const uint64_t word=(uint64_t(46+alias*64)<<56)|0x00a5c33c00000000ull|value;
  for(unsigned n=0;n<8;++n)sp[0xff8+n]=ram[0xff8+n]=uint8_t(word>>(56-n*8));
  n64::DpInterface dp;dp.flags=source;bool irq=false;check(dp.write(0,0xff8) && dp.write(1,0x1000));dp.tick(ram,sp,irq);
  const unsigned z=(value/65536%32768)*8,delta=value%65536;
  check(!dp.blocked && !irq && dp.current==0x1000 && dp.primitive_depth.z()==z && dp.primitive_depth.delta()==delta && dp.primitive_depth.snapshot()==value%2147483648u);
  const auto saved=dp.primitive_depth.snapshot();for(unsigned bit:{31u,32u,63u})check(!dp.primitive_depth.restore(saved|(1ull<<bit)) && dp.primitive_depth.snapshot()==saved);
  dp.reset();check(dp.primitive_depth.snapshot()==0);++commands;
 }
 n64::RdpPacket packet;packet.count=packet.needed=14;packet.words[0]=0xcd80001000080000ull;packet.words[1]=0x00020000ffff0000ull;packet.words[3]=65536;packet.words[4]=0x0080008000800080ull;packet.words[12]=uint64_t(65536u<<13)<<32|65536;
 n64::RdpCombiner combiner;combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(4ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|4;
 const uint64_t mode=(15ull<<36)|16384|512|4|32,scissor=(16ull<<12)|16;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned field=0;field<2;++field)for(unsigned partner=0;partner<4;++partner)for(unsigned size:{2u,3u}){
  std::vector<uint8_t> memory(512,255);n64::HiddenRdram hidden(512);check(hidden.ordinary_burst(0,memory.data(),memory.size()));n64::RdpBlender blender;n64::RdpPrimitiveDepth primitive;
  const auto value=operand(raw,field,partner);primitive.command(value);const unsigned z=(value/65536%32768)*8,delta=value%65536,code=encoded(delta);
  check(n64::RdpTriangleShade::draw(packet,(uint64_t(size)<<51)|(3ull<<32)|64,mode,scissor,combiner,memory,nullptr,nullptr,nullptr,&blender,&hidden,192,&primitive)==0);
  for(unsigned n:{4u,8u,9u,12u}){const unsigned za=192+n*2;check(unsigned(memory[za])*256+memory[za+1]==compressed(z)+code/4 && hidden.halfword(za)==code%4);++pixels;}
 }
 // Primitive delta also drives the memory-alpha shifter when Z comparison is off.
 for(unsigned delta=0;delta<65536;++delta){n64::RdpPrimitiveDepth primitive;primitive.command(delta);n64::RdpBlender blender;
  const uint64_t m=(1ull<<18)|(1ull<<22)|64|16384;const auto result=blender.pixel(m,{37,191,243,128},0,0x69f73be0,4,1,primitive.delta());
  const unsigned code=encoded(delta),b=(28u/(1u<<(code<11?4:15-code)))|3;
  check(result.write && result.rgb[1]==(191u*16+247u*(b+1))/32%256);
  std::vector<uint8_t> memory(512,0);for(unsigned at=64;at<128;at+=4){memory[at]=0x69;memory[at+1]=0xf7;memory[at+2]=0x3b;memory[at+3]=0xe0;}
  n64::HiddenRdram hidden(512);n64::RdpBlender live;
  check(n64::RdpTriangleShade::draw(packet,(3ull<<51)|(3ull<<32)|64,(15ull<<36)|m|512|4,scissor,combiner,memory,nullptr,nullptr,nullptr,&live,&hidden,192,&primitive)==0);
  check(memory[64+4*4+1]==(128u*16+247u*(b+1))/32%256);

 }
 std::cout<<commands<<" complete primitive field/alias/DRAM-XBUS captures; "<<pixels<<" RGBA16/RGBA32 Z stores; 65536 raw delta shifter checks; reset/state rejection PASS\n";
}
