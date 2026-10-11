#include "arm.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:nds::Memory {
 uint32_t ins=0;unsigned reads=0,writes=0;
 bool read(uint32_t,unsigned,uint32_t& value)override{++reads;value=ins;return true;}
 bool write(uint32_t,unsigned,uint32_t)override{++writes;return true;}
};
static bool predicate(unsigned c,unsigned f){const bool n=f&16,z=f&8,k=f&4,v=f&2;const bool yes[]={z,!z,k,!k,n,!n,v,!v,k&&!z,!k||z,n==v,n!=v,!z&&n==v,z||n!=v,true};return yes[c];}
static uint32_t high_word(unsigned kind,bool rounded,uint32_t a,uint32_t b,uint32_t seed){
 const int64_t x=a<2147483648u?a:int64_t(a)-4294967296ll,y=b<2147483648u?b:int64_t(b)-4294967296ll;
 int64_t product=x*y;if(kind==2)product=-product;if(rounded)product+=2147483648ll;
 const int64_t quotient=product>=0?product/4294967296ll:-((-product+4294967295ll)/4294967296ll);
 const int64_t value=quotient+(kind?int64_t(seed):0);return uint32_t((value%4294967296ll+4294967296ll)%4294967296ll);
}
static std::array<uint32_t,2> unsigned_accumulate(uint32_t a,uint32_t b,uint32_t low_seed,uint32_t high_seed){
 const uint64_t low=uint64_t(a%65536)*(b%65536),middle=uint64_t(a/65536)*(b%65536)+uint64_t(b/65536)*(a%65536)+low/65536;
 const uint64_t high=uint64_t(a/65536)*(b/65536)+middle/65536;
 const uint64_t sum=low%65536+(middle%65536)*65536+low_seed+high_seed;
 return {uint32_t(sum%4294967296ull),uint32_t(high+sum/4294967296ull)};
}
int main(){try{Bus bus;nds::Arm cpu(bus,true,true);uint64_t checks=0;
 auto high=[&](unsigned kind,bool rounded,uint32_t a,uint32_t b,uint32_t seed,unsigned flags,unsigned ge,unsigned cond=14,unsigned rd=4,unsigned rm=0,unsigned rs=8,unsigned ra=10){
  if(!kind)ra=15;cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdf+r;cpu.state.r[rm]=a;cpu.state.r[rs]=b;if(ra!=15)cpu.state.r[ra]=seed;cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);auto want=cpu.state.r;const auto psw=cpu.state.cpsr;
  if(predicate(cond,flags))want[rd]=high_word(kind,rounded,want[rm],want[rs],ra==15?0:want[ra]);want[15]+=4;
  bus.ins=(cond<<28)|0x07500010|(rd<<16)|(ra<<12)|(rs<<8)|rm|(kind==2?192:0)|(unsigned(rounded)<<5);bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"high multiply signed/round/subtract/flags/alias/condition/access");++checks;
 };
 auto umaal=[&](uint32_t a,uint32_t b,uint32_t lo_seed,uint32_t hi_seed,unsigned flags,unsigned ge,unsigned cond=14,unsigned lo=4,unsigned hi=5,unsigned rm=0,unsigned rs=8){
  cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdf+r;cpu.state.r[rm]=a;cpu.state.r[rs]=b;cpu.state.r[lo]=lo_seed;cpu.state.r[hi]=hi_seed;cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);auto want=cpu.state.r;const auto psw=cpu.state.cpsr;
  if(predicate(cond,flags)){const auto words=unsigned_accumulate(want[rm],want[rs],want[lo],want[hi]);want[lo]=words[0];want[hi]=words[1];}want[15]+=4;
  bus.ins=(cond<<28)|0x00400090|(hi<<16)|(lo<<12)|(rs<<8)|rm;bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"UMAAL separate seeds/carry/flags/alias/condition/access");++checks;
 };
 for(unsigned value=0;value<65536;++value)for(unsigned position:{0u,16u})for(bool ones:{false,true})for(uint32_t b:{0u,1u,0xffffffffu,0x7fffffffu,0x80000000u,0x00010001u,0xffff0001u,0x80000001u})for(unsigned flags:{0u,31u}){
  const uint32_t a=(value<<position)|(ones?(65535u<<(16-position)):0);
  for(unsigned kind=0;kind<3;++kind)for(bool rounded:{false,true})high(kind,rounded,a,b,flags?0x7fffffff:0xffffffff,flags,flags%16);
  umaal(a,b,flags?0x7fffffff:0xffffffff,flags?0x80000000:1,flags,flags%16);
 }
 for(unsigned kind=0;kind<3;++kind)for(bool rounded:{false,true})for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)for(uint32_t a:{0u,0xffffffffu,0x7fffffffu,0x80000000u})for(uint32_t b:{0xffffffffu,0x80000000u})high(kind,rounded,a,b,0xffffffff,flags,ge,cond);
 for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)for(uint32_t a:{0u,0xffffffffu,0x7fffffffu,0x80000000u})for(uint32_t b:{0xffffffffu,0x80000000u})umaal(a,b,0xffffffff,1,flags,ge,cond);
 for(unsigned kind=0;kind<3;++kind)for(bool rounded:{false,true})for(unsigned rd=0;rd<15;++rd)for(unsigned rm=0;rm<15;++rm)for(unsigned rs=0;rs<15;++rs)for(unsigned ra=0;ra<(kind?15u:1u);++ra)high(kind,rounded,0x80000001,0x7fffffff,0xffffffff,31,(rd+rm+rs+ra)%16,14,rd,rm,rs,ra);
 for(unsigned lo=0;lo<15;++lo)for(unsigned hi=0;hi<15;++hi)if(lo!=hi)for(unsigned rm=0;rm<15;++rm)for(unsigned rs=0;rs<15;++rs)umaal(0x80000001,0xffffffff,0x7fffffff,0x80000000,31,(lo+hi+rm+rs)%16,14,lo,hi,rm,rs);
 for(unsigned profile=0;profile<3;++profile)for(unsigned op=0;op<4;++op)for(bool rounded:{false,true})for(unsigned rd=0;rd<16;++rd)for(unsigned rm=0;rm<16;++rm)for(unsigned rs=0;rs<16;++rs)for(unsigned ra=0;ra<16;++ra){if(profile==2 && (op==0 || (op==3 && ra!=15)) && rd<15 && rm<15 && rs<15)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);auto before=target.state.r;bus.ins=0xe7500010|(rd<<16)|(ra<<12)|(rs<<8)|rm|(op<<6)|(unsigned(rounded)<<5);bus.reads=bus.writes=0;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"high multiply profile/reserved/PC diagnostics");++checks;
 }
 for(unsigned profile=0;profile<3;++profile)for(unsigned lo=0;lo<16;++lo)for(unsigned hi=0;hi<16;++hi)for(unsigned rm=0;rm<16;++rm)for(unsigned rs=0;rs<16;++rs){if(profile==2 && lo!=hi && lo<15 && hi<15 && rm<15 && rs<15)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);auto before=target.state.r;bus.ins=0xe0400090|(hi<<16)|(lo<<12)|(rs<<8)|rm;bus.reads=bus.writes=0;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"UMAAL profile/PC/duplicate destination diagnostics");++checks;
 }
 require(high_word(0,false,0xffffffff,0x80000000,0)==0 && high_word(0,true,0xffffffff,0x80000000,0)==1,"positive half rounding anchor");require(high_word(0,false,1,0x80000000,0)==0xffffffff && high_word(0,true,1,0x80000000,0)==0,"negative half rounding anchor");
 require(high_word(2,true,1,0x80000000,0)==1,"subtract-before-round anchor");require(unsigned_accumulate(0xffffffff,0xffffffff,0xffffffff,0xffffffff)==std::array<uint32_t,2>{0xffffffff,0xffffffff},"UMAAL maximum literal anchor");
 std::cout<<checks<<" ARMv6 high-word/rounding/UMAAL/flags/aliases/profile observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
