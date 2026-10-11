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
static int64_t signed_bits(uint32_t value,unsigned bits){const int64_t span=1ll<<bits;return value<(span/2)?value:int64_t(value)-span;}
// Oracle decodes independent halfword indices instead of rotating a packed word.
static int64_t products(uint32_t a,uint32_t b,bool exchange,bool subtract){
 const auto a0=signed_bits(a%65536,16),a1=signed_bits(a/65536,16);
 const auto b0=signed_bits(exchange?b/65536:b%65536,16),b1=signed_bits(exchange?b%65536:b/65536,16);
 return a0*b0+(subtract?-a1*b1:a1*b1);
}
// Carry the signed delta through a separate low-word addition, avoiding core's 64-bit accumulator equation.
static std::array<uint32_t,2> wide(uint32_t low,uint32_t high,int64_t delta){
 const int64_t total=int64_t(low)+delta;
 const int64_t carry=total>=0?total/4294967296ll:-((-total+4294967295ll)/4294967296ll);
 const int64_t rest=total-carry*4294967296ll;
 return {uint32_t(rest),uint32_t(int64_t(high)+carry)};
}
int main(){try{Bus bus;nds::Arm cpu(bus,true,true);uint64_t checks=0;
 auto check=[&](unsigned kind,bool exchange,bool subtract,uint32_t a,uint32_t b,uint32_t low,uint32_t high,unsigned flags,unsigned ge,unsigned cond=14,unsigned rd=4,unsigned rm=0,unsigned rs=8,unsigned ra=10){
  // kind 0 = SMU, 1 = SML, 2 = long; long uses rd as low, ra as high.
  if(!kind)ra=15;cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdf+r;
  cpu.state.r[rm]=a;cpu.state.r[rs]=b;if(kind==1)cpu.state.r[ra]=low;if(kind==2){cpu.state.r[rd]=low;cpu.state.r[ra]=high;}
  cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);auto want=cpu.state.r;uint32_t psw=cpu.state.cpsr;
  if(predicate(cond,flags)){
   const auto delta=products(want[rm],want[rs],exchange,subtract);
   if(kind==2){auto result=wide(want[rd],want[ra],delta);want[rd]=result[0];want[ra]=result[1];}
   else {const int64_t result=delta+(kind?signed_bits(want[ra],32):0);want[rd]=uint32_t(result);if(result<-2147483648ll || result>2147483647ll)psw|=0x08000000;}
  }
  want[15]+=4;
  bus.ins=(cond<<28)|(kind==2?0x07400010:0x07000010)|((kind==2?ra:rd)<<16)|((kind==2?rd:ra)<<12)|(rs<<8)|rm|(unsigned(exchange)<<5)|(unsigned(subtract)<<6);
  bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"dual multiply result/carry/Q/GE/alias/condition/access");++checks;
 };
 check(0,false,false,0x80008000,0x80008000,0,0,0,0); // +2^31 wraps, sets Q.
 check(1,false,false,0x80008000,0x80008000,0xffffffff,0,0,0); // cancellation: final +2^31-1, Q stays clear.
 for(unsigned value=0;value<65536;++value)for(unsigned position:{0u,16u})for(uint32_t surround:{0u,65535u})for(uint32_t b:{0u,0x00010001u,0xffffffffu,0x80008000u,0x7fff7fffu,0x7fff8000u,0xffff0001u,0x80000001u})for(unsigned flags:{0u,31u})for(unsigned kind=0;kind<3;++kind)for(bool exchange:{false,true})for(bool subtract:{false,true})
  check(kind,exchange,subtract,(value<<position)|(surround<<(16-position)),b,flags?0x7fffffff:0xffffffff,flags?0x80000000:1,flags,flags%16,14,4,0,8,kind==2?5:10);
 for(unsigned kind=0;kind<3;++kind)for(bool exchange:{false,true})for(bool subtract:{false,true})for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)for(uint32_t a:{0u,0xffffffffu,0x7fff8000u,0x80008000u})for(uint32_t seed:{0u,1u,0x7fffffffu,0x80000000u,0xffffffffu})
  check(kind,exchange,subtract,a,0x8000ffff,seed,~seed,flags,ge,cond,4,0,8,kind==2?5:10);
 for(unsigned kind=0;kind<3;++kind)for(bool exchange:{false,true})for(bool subtract:{false,true})for(unsigned rd=0;rd<15;++rd)for(unsigned rm=0;rm<15;++rm)for(unsigned rs=0;rs<15;++rs)for(unsigned ra=0;ra<(kind?15u:1u);++ra){if(kind==2 && rd==ra)continue;
  check(kind,exchange,subtract,0x8000ffff,0x7fff8000,0xffffffff,0x80000000,0,(rd+rm+rs+ra)%16,14,rd,rm,rs,ra);
 }
 for(unsigned profile=0;profile<3;++profile)for(bool long_form:{false,true})for(unsigned op=0;op<8;++op)for(unsigned rd=0;rd<16;++rd)for(unsigned rm=0;rm<16;++rm)for(unsigned rs=0;rs<16;++rs)for(unsigned ra=0;ra<16;++ra){
  const bool valid=op<4 && rd<15 && rm<15 && rs<15 && (!long_form || (ra<15 && rd!=ra));if(profile==2 && valid)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);const auto before=target.state.r;bus.ins=0xe7000010|(unsigned(long_form)<<22)|(rd<<16)|(ra<<12)|(rs<<8)|rm|(op<<5);bus.reads=bus.writes=0;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"dual multiply invalid profile/PC/reserved/dest diagnostics");++checks;
 }
 require(products(0x80008000,0x80008000,false,false)==2147483648ll,"positive overflow literal");require(products(0x8000ffff,0x7fff8000,false,true)==1073741824ll,"subtract literal");require(wide(0,0,-1)==std::array<uint32_t,2>{0xffffffff,0xffffffff},"negative borrow literal");
 std::cout<<checks<<" ARMv6 dual multiply/exchange/overflow/carry/aliases observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
