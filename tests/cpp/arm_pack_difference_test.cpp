#include "arm.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:nds::Memory {
 uint32_t ins=0;unsigned reads=0,writes=0;
 bool read(uint32_t,unsigned,uint32_t& value)override{++reads;value=ins;return true;}
 bool write(uint32_t,unsigned,uint32_t)override{++writes;return true;}
};
static bool predicate(unsigned c,unsigned f){const bool n=f&16,z=f&8,k=f&4,v=f&2;const bool yes[]={z,!z,k,!k,n,!n,v,!v,k&&!z,!k||z,n==v,n!=v,!z&&n==v,z||n!=v,true};return yes[c];}
static uint32_t packed(bool top,unsigned amount,uint32_t a,uint32_t b){
 if(!top)return a%65536+uint32_t((uint64_t(b)*(1ull<<amount))%4294967296/65536)*65536;
 const int64_t value=b<2147483648u?b:int64_t(b)-4294967296ll,denominator=1ll<<(amount?amount:32);
 const int64_t shifted=value>=0?value/denominator:-((-value+denominator-1)/denominator);
 return (a/65536)*65536+uint32_t((shifted%65536+65536)%65536);
}
static uint32_t difference(uint32_t a,uint32_t b,uint32_t seed){uint64_t sum=seed;for(unsigned n=0;n<4;++n){const int64_t x=a%256,y=b%256;sum+=x>=y?x-y:y-x;a/=256;b/=256;}return uint32_t(sum%4294967296);}
static uint32_t operand_for_sum(unsigned sum){uint32_t word=0;for(unsigned n=0;n<4;++n){unsigned byte=sum>255?255:sum;word+=byte<<(n*8);sum-=byte;}return word;}
int main(){try{Bus bus;nds::Arm cpu(bus,true,true);uint64_t checks=0;
 auto pack=[&](bool top,unsigned amount,uint32_t a,uint32_t b,unsigned flags,unsigned ge,unsigned cond=14,unsigned rd=4,unsigned rn=0,unsigned rm=8){
  cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdf+r;cpu.state.r[rn]=a;cpu.state.r[rm]=b;cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);auto want=cpu.state.r;const auto psw=cpu.state.cpsr;
  if(predicate(cond,flags))want[rd]=packed(top,amount,want[rn],want[rm]);want[15]+=4;
  bus.ins=(cond<<28)|0x06800010|(rn<<16)|(rd<<12)|(amount<<7)|(unsigned(top)<<6)|rm;bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"pack result/ASR32/flags/alias/condition/access");++checks;
 };
 auto sad=[&](uint32_t a,uint32_t b,uint32_t seed,unsigned flags,unsigned ge,unsigned cond=14,unsigned rd=4,unsigned rm=0,unsigned rs=8,unsigned ra=10){
  cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdf+r;cpu.state.r[rm]=a;cpu.state.r[rs]=b;if(ra!=15)cpu.state.r[ra]=seed;cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);auto want=cpu.state.r;const auto psw=cpu.state.cpsr;
  if(predicate(cond,flags))want[rd]=difference(want[rm],want[rs],ra==15?0:want[ra]);want[15]+=4;
  bus.ins=(cond<<28)|0x07800010|(rd<<16)|(ra<<12)|(rs<<8)|rm;bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"difference unsigned lanes/accumulate/wrap/flags/alias/condition/access");++checks;
 };
 for(bool top:{false,true})for(unsigned amount=0;amount<32;++amount)for(unsigned position:{0u,16u})for(bool ones:{false,true})for(unsigned value=0;value<65536;++value)for(unsigned flags:{0u,31u})pack(top,amount,flags?0x89abcdef:0x12345678,(value<<position)|(ones?(65535u<<(16-position)):0),flags,flags%16);
 for(unsigned position=0;position<4;++position)for(unsigned pair=0;pair<65536;++pair)for(unsigned flags:{0u,31u})for(unsigned ra:{10u,15u}){
  const auto mask=255u<<(position*8);const auto a=(0x78563412u&~mask)|((pair%256)<<(position*8)),b=(0x000180ffu&~mask)|((pair/256)<<(position*8));sad(a,b,flags?0xffffffff:0x7fffffff,flags,flags%16,14,4,0,8,ra);
 }
 // Every possible difference sum and a complete selected near-wrap seed interval.
 for(unsigned sum=0;sum<=1020;++sum)for(unsigned offset=0;offset<4096;++offset)for(unsigned flags:{0u,31u})sad(operand_for_sum(sum),0,0xfffff000u+offset,flags,flags%16);
 for(unsigned sum=0;sum<=1020;++sum)for(uint32_t seed:{0u,1u,0x7fffffffu,0x80000000u,0xffff0000u,0xfffffffeu,0xffffffffu})sad(operand_for_sum(sum),0,seed,31,sum%16);
 for(bool top:{false,true})for(unsigned amount=0;amount<32;++amount){
  for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)for(uint32_t b:{0u,0xffffffffu,0x80000000u,0x7fffffffu})pack(top,amount,0x89abcdef,b,flags,ge,cond);
  for(unsigned rd=0;rd<15;++rd)for(unsigned rn=0;rn<15;++rn)for(unsigned rm=0;rm<15;++rm)pack(top,amount,0x89abcdef,0x80017fff,31,(rd+rn+rm)%16,14,rd,rn,rm);
 }
 for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)for(unsigned ra:{10u,15u})for(uint32_t b:{0u,0xffffffffu,0x80018001u,0x7fff7fffu})sad(0xff008001,b,0xffffffff,flags,ge,cond,4,0,8,ra);
 for(unsigned rd=0;rd<15;++rd)for(unsigned rm=0;rm<15;++rm)for(unsigned rs=0;rs<15;++rs)for(unsigned ra=0;ra<16;++ra)sad(0xff008001,0x0180ff00,0xfffffffe,31,(rd+rm+rs+ra)%16,14,rd,rm,rs,ra);
 for(unsigned profile=0;profile<3;++profile)for(bool top:{false,true})for(unsigned amount=0;amount<32;++amount)for(unsigned rd=0;rd<16;++rd)for(unsigned rn=0;rn<16;++rn)for(unsigned rm=0;rm<16;++rm){if(profile==2 && rd<15 && rn<15 && rm<15)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);auto before=target.state.r;bus.ins=0xe6800010|(rn<<16)|(rd<<12)|(amount<<7)|(unsigned(top)<<6)|rm;bus.reads=bus.writes=0;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"pack profile/PC diagnostics");++checks;
 }
 for(unsigned profile=0;profile<3;++profile)for(unsigned rd=0;rd<16;++rd)for(unsigned rm=0;rm<16;++rm)for(unsigned rs=0;rs<16;++rs)for(unsigned ra=0;ra<16;++ra){if(profile==2 && rd<15 && rm<15 && rs<15)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);auto before=target.state.r;bus.ins=0xe7800010|(rd<<16)|(ra<<12)|(rs<<8)|rm;bus.reads=bus.writes=0;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"difference profile/PC diagnostics");++checks;
 }
 require(packed(true,0,0x12345678,0x80000000)==0x1234ffff && packed(true,0,0x12345678,0x7fffffff)==0x12340000,"ASR32 literal anchors");require(difference(0,0xffffffff,0xffffffff)==1019,"unsigned sum/wrap literal anchor");
 std::cout<<checks<<" ARMv6 packing/difference/flags/aliases/profile observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
