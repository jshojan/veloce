#include "arm.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:nds::Memory {
 uint32_t instruction=0;unsigned reads=0,writes=0;
 bool read(uint32_t,unsigned,uint32_t& value)override{++reads;value=instruction;return true;}
 bool write(uint32_t,unsigned,uint32_t)override{++writes;return true;}
};
static uint32_t reference(unsigned kind,unsigned rotation,uint32_t source,uint32_t base) {
 unsigned bytes[4];for(unsigned n=0;n<4;++n)bytes[n]=(source>>(8*((n+rotation)%4)))%256;
 auto lane=[&](unsigned index,unsigned bits)->int64_t {
  const unsigned value=bytes[index]+(bits==16?bytes[(index+1)%4]*256:0);
  return kind<4 && value>=(1u<<(bits-1))?int64_t(value)-(1ll<<bits):value;
 };
 if(kind%4==0) {const uint32_t low=uint32_t(base%65536+lane(0,8))%65536,high=uint32_t(base/65536+lane(2,8))%65536;return low+high*65536;}
 return uint32_t(int64_t(base)+lane(0,kind%4==3?16:8));
}
static bool predicate(unsigned cond,unsigned flags) {
 const bool n=flags&16,z=flags&8,c=flags&4,v=flags&2;
 const bool yes[]={z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,n==v,n!=v,!z&&n==v,z||n!=v,true};return yes[cond];
}
int main(){try {
 Bus bus;nds::Arm cpu(bus,true,true);uint64_t checks=0;
 auto check=[&](bool thumb,unsigned kind,unsigned rot,unsigned rd,unsigned rm,unsigned rn,uint32_t value,uint32_t base,unsigned flags,unsigned ge,unsigned cond) {
  cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdfu+r;
  cpu.state.r[rm]=value;if(rn<15)cpu.state.r[rn]=base;
  cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16)|(thumb?32:0);auto expected=cpu.state.r;const auto psw=cpu.state.cpsr;
  if(thumb || predicate(cond,flags))expected[rd]=reference(kind,rot,expected[rm],rn<15?expected[rn]:0);
  expected[15]+=thumb?2:4;
  const unsigned tk=kind==3?0:kind==2?1:kind==7?2:3;
  bus.instruction=thumb?0xb200|(tk<<6)|(rm<<3)|rd:(cond<<28)|0x06800070|(kind<<20)|(rn<<16)|(rd<<12)|(rot<<10)|rm;
  bus.reads=bus.writes=0;cpu.step();
  require(!cpu.state.unsupported && cpu.state.r==expected && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"extension result/alias/CPSR/PC/data access");++checks;
 };
 for(unsigned kind:{0u,2u,3u,4u,6u,7u})for(unsigned rot=0;rot<4;++rot)for(unsigned rn:{8u,15u})for(unsigned flags:{0u,31u})for(unsigned value=0;value<65536;++value)
  check(false,kind,rot,4,0,rn,value|((65535u-value)<<16),flags?0x80008000:0x7fffffff,flags,flags%16,14);
 for(unsigned kind:{2u,3u,6u,7u})for(unsigned flags:{0u,31u})for(unsigned value=0;value<65536;++value)check(true,kind,0,4,0,15,value|((65535u-value)<<16),0,flags,flags%16,14);
 // Every register field, including destination/source/accumulator aliases.
 for(unsigned kind:{0u,2u,3u,4u,6u,7u})for(unsigned rot=0;rot<4;++rot)for(unsigned rd=0;rd<15;++rd)for(unsigned rm=0;rm<15;++rm)for(unsigned rn=0;rn<16;++rn)
  for(uint32_t source:{0x80ff7f00u,0x80007fffu})check(false,kind,rot,rd,rm,rn,source,0xffff8000,31,(rd+rm+rn)%16,14);
 for(unsigned kind:{0u,2u,3u,4u,6u,7u})for(unsigned rot=0;rot<4;++rot)for(unsigned rn:{8u,15u})for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)
  for(uint32_t source:{0u,0xffffffffu,0x80007fffu,0x7fff8000u})check(false,kind,rot,4,0,rn,source,0xffff8000,flags,ge,cond);
 for(unsigned kind:{2u,3u,6u,7u})for(unsigned rd=0;rd<8;++rd)for(unsigned rm=0;rm<8;++rm)for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)
  check(true,kind,0,rd,rm,15,0x800080ff,0,flags,ge,14);
 // Unsupported CPU profiles, PC operands and reserved fields stop atomically.
 for(unsigned profile=0;profile<3;++profile)for(unsigned kind=0;kind<8;++kind)for(unsigned rn=0;rn<16;++rn)for(unsigned rd=0;rd<16;++rd)for(unsigned rm=0;rm<16;++rm)for(unsigned rot=0;rot<4;++rot)for(unsigned reserved=0;reserved<4;++reserved) {
  if(profile==2 && kind!=1 && kind!=5 && rd<15 && rm<15 && !reserved)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);const auto before=target.state.r;
  bus.instruction=0xe6800070|(kind<<20)|(rn<<16)|(rd<<12)|(rot<<10)|(reserved<<8)|rm;target.step();
  require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf,"extension diagnostic mutation/profile escape");++checks;
 }
 for(unsigned profile=0;profile<2;++profile)for(unsigned word=0;word<256;++word) {
  nds::Arm target(bus,profile==1);target.reset(0x1000);target.state.cpsr|=32;const auto before=target.state.r;
  bus.instruction=0xb200|word;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xff,"Thumb pre-v6 extension escape");++checks;
 }
 std::cout<<checks<<" ARMv6 extend/accumulate/Thumb/profile observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
