#include "arm.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:nds::Memory {
 uint32_t instruction=0;unsigned reads=0,writes=0;
 bool read(uint32_t,unsigned,uint32_t& value)override{++reads;value=instruction;return true;}
 bool write(uint32_t,unsigned,uint32_t)override{++writes;return true;}
};
static int64_t signed_value(uint32_t n,unsigned bits){return n<(1ull<<(bits-1))?n:int64_t(n)-(1ll<<bits);}
static bool predicate(unsigned cond,unsigned flags){const bool n=flags&16,z=flags&8,c=flags&4,v=flags&2;const bool yes[]={z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,n==v,n!=v,!z&&n==v,z||n!=v,true};return yes[cond];}
static uint32_t oracle(bool dual,bool uns,unsigned immediate,bool asr,unsigned amount,uint32_t source,bool& saturated) {
 const unsigned width=immediate+!uns;const int64_t minimum=uns?0:-(1ll<<(width-1)),maximum=(1ll<<(width-!uns))-1;
 auto lane=[&](int64_t value){saturated|=value<minimum || value>maximum;return uint32_t(std::max(minimum,std::min(value,maximum)));};
 if(dual){const uint32_t lo=lane(signed_value(source%65536,16))%65536,hi=lane(signed_value(source/65536,16))%65536;return lo+hi*65536;}
 int64_t value;
 if(asr) {const unsigned count=amount?amount:32;const int64_t divisor=1ll<<count;value=signed_value(source,32);value=value>=0?value/divisor:-((-value+divisor-1)/divisor);}
 else value=signed_value(uint32_t(uint64_t(source)*(1ull<<amount)),32);
 return lane(value);
}
int main(){try{
 Bus bus;nds::Arm cpu(bus,true,true);uint64_t checks=0;
 auto check=[&](bool dual,bool uns,unsigned imm,bool asr,unsigned amount,unsigned rd,unsigned rm,uint32_t source,unsigned flags,unsigned ge,unsigned cond){
  cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdf+r;cpu.state.r[rm]=source;cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);
  auto expected=cpu.state.r;auto psw=cpu.state.cpsr;bool saturated=false;
  if(predicate(cond,flags)){expected[rd]=oracle(dual,uns,imm,asr,amount,source,saturated);if(saturated)psw|=0x08000000;}
  expected[15]+=4;bus.instruction=(cond<<28)|(dual?0x06a00f30:0x06a00010)|(unsigned(uns)<<22)|(imm<<16)|(rd<<12)|rm|(dual?0:(unsigned(asr)<<6)|(amount<<7));
  bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==expected && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"ARMv6 saturation result/Q/flags/alias/access");++checks;
 };
 // Every scalar control encoding across full selected byte domains at four positions.
 for(bool uns:{false,true})for(unsigned imm=0;imm<32;++imm)for(bool asr:{false,true})for(unsigned amount=0;amount<32;++amount)for(unsigned position=0;position<4;++position)for(bool surround:{false,true})for(unsigned value=0;value<256;++value)for(unsigned flags:{0u,31u})
  check(false,uns,imm,asr,amount,4,0,(surround?~(255u<<(position*8)):0)|(value<<(position*8)),flags,flags%16,14);
 for(bool uns:{false,true})for(unsigned imm=0;imm<16;++imm)for(unsigned value=0;value<65536;++value)for(unsigned flags:{0u,31u})check(true,uns,imm,false,0,4,0,value|((65535u-value)<<16),flags,flags%16,14);
 for(bool uns:{false,true})for(unsigned imm:{0u,1u,7u,15u,30u,31u})for(bool asr:{false,true})for(unsigned amount:{0u,1u,8u,16u,24u,31u})for(unsigned position:{0u,16u})for(unsigned value=0;value<65536;++value)for(unsigned flags:{0u,31u})check(false,uns,imm,asr,amount,4,0,(value<<position)|((65535u-value)<<(16-position)),flags,flags%16,14);
 // All ordinary conditions and flag/GE states, aliases and saturation boundaries.
 for(bool dual:{false,true})for(bool uns:{false,true})for(unsigned imm=0;imm<(dual?16u:32u);++imm)for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)for(uint32_t source:{0u,0xffffffffu,0x80008000u,0x7fff7fffu})check(dual,uns,imm,false,0,4,0,source,flags,ge,cond);
 for(bool dual:{false,true})for(bool uns:{false,true})for(unsigned imm=0;imm<(dual?16u:32u);++imm)for(unsigned rd=0;rd<15;++rd)for(unsigned rm=0;rm<15;++rm)for(bool asr:{false,true})for(unsigned amount:{0u,31u})for(uint32_t source:{0x80007fffu,0x7fff8000u})check(dual,uns,imm,asr,amount,rd,rm,source,31,(rd+rm)%16,14);
 for(unsigned profile=0;profile<3;++profile)for(bool dual:{false,true})for(bool uns:{false,true})for(unsigned imm=0;imm<(dual?16u:32u);++imm)for(unsigned rd=0;rd<16;++rd)for(unsigned rm=0;rm<16;++rm)for(bool asr:{false,true})for(unsigned amount=0;amount<(dual?1u:32u);++amount){
  if(profile==2 && rd<15 && rm<15)continue;nds::Arm other(bus,profile>0,profile==2);other.reset(0x1000);const auto before=other.state.r;
  bus.instruction=0xe0000000|(dual?0x06a00f30:0x06a00010)|(unsigned(uns)<<22)|(imm<<16)|(rd<<12)|rm|(dual?0:(unsigned(asr)<<6)|(amount<<7));bus.reads=bus.writes=0;other.step();
  require(other.state.unsupported && other.state.r==before && other.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"pre-v6/PC saturation escaped or mutated state");++checks;
 }
 // Literal endpoints: USAT zero width; full signed width; ASR #32 and sticky Q.
 bool saturated=false;require(oracle(false,true,0,false,0,1,saturated)==0 && saturated,"USAT #0 anchor");saturated=false;
 require(oracle(false,false,31,false,0,0x80000000,saturated)==0x80000000 && !saturated,"SSAT #32 anchor");saturated=false;
 require(oracle(false,false,0,true,0,0x80000000,saturated)==0xffffffff && !saturated,"ASR #32 anchor");
 std::cout<<checks<<" ARMv6 saturation/control/alias/condition/Q/profile observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
