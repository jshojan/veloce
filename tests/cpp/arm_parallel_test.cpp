#include "arm.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:nds::Memory {
 uint32_t ins=0;unsigned reads=0,writes=0;
 bool read(uint32_t,unsigned,uint32_t& value)override{++reads;value=ins;return true;}
 bool write(uint32_t,unsigned,uint32_t)override{++writes;return true;}
};
static int64_t lane(uint32_t word,unsigned index,unsigned bits,bool sign){const uint32_t raw=(word>>(index*bits))%uint32_t(1u<<bits);return sign && raw>=(1u<<(bits-1))?int64_t(raw)-(1ll<<bits):raw;}
static uint32_t select(uint32_t a,uint32_t b,unsigned ge){uint32_t out=0;for(unsigned n=0;n<4;++n)out+=((ge&(1u<<n)?a:b)>>(n*8)&255u)<<(n*8);return out;}
static uint32_t oracle(unsigned family,unsigned operation,uint32_t a,uint32_t b,uint32_t& psw){
 if(!family)return select(a,b,(psw>>16)&15);
 const unsigned bits=operation<4?16:8,lanes=32/bits,mode=family%4;const bool sign=family<4;uint32_t result=0,ge=0;
 for(unsigned n=0;n<lanes;++n){
  const unsigned second=(operation==1 || operation==2)?1-n:n;
  const bool sub=operation==3 || operation==7 || (operation==1 && !n) || (operation==2 && n);
  int64_t value=lane(a,n,bits,sign)+(sub?-1:1)*lane(b,second,bits,sign);
  if(mode==1){if(sign?value>=0:sub?value>=0:value>=(1ll<<bits))ge+=(bits==16?3u:1u)<<(n*(bits/8));}
  else if(mode==2){const int64_t low=sign?-(1ll<<(bits-1)):0,high=(1ll<<(bits-sign))-1;value=std::max(low,std::min(value,high));}
  else {const int64_t modulus=1ll<<(bits+1);value=(value%modulus+modulus)%modulus/2;}
  const auto modulus=1ll<<bits;result+=uint32_t((value%modulus+modulus)%modulus)<<(n*bits);
 }
 if(mode==1)psw=(psw&~0xf0000u)|(ge<<16);return result;
}
static bool predicate(unsigned c,unsigned f){bool n=f&16,z=f&8,k=f&4,v=f&2;const bool answer[]={z,!z,k,!k,n,!n,v,!v,k&&!z,!k||z,n==v,n!=v,!z&&n==v,z||n!=v,true};return answer[c];}
static uint32_t encoding(unsigned family,unsigned op,unsigned rd,unsigned rn,unsigned rm){return family?0xe6000f10|(family<<20)|(op<<5)|(rd<<12)|(rn<<16)|rm:0xe6800fb0|(rd<<12)|(rn<<16)|rm;}
static uint32_t pack_bytes(unsigned a){return a|((a^128)<<8)|((255-a)<<16)|((a^85)<<24);}
int main(){try{Bus bus;nds::Arm cpu(bus,true,true);uint64_t checks=0;
 auto check=[&](unsigned family,unsigned op,uint32_t a,uint32_t b,unsigned flags,unsigned ge,unsigned cond=14,unsigned rd=4,unsigned rn=0,unsigned rm=8){
  cpu.reset(0x1000);for(unsigned r=0;r<15;++r)cpu.state.r[r]=0x13579bdf+r;cpu.state.r[rn]=a;cpu.state.r[rm]=b;
  auto want=cpu.state.r;uint32_t psw=cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);
  if(predicate(cond,flags))want[rd]=oracle(family,op,want[rn],want[rm],psw);want[15]+=4;
  bus.ins=(encoding(family,op,rd,rn,rm)&0x0fffffff)|(cond<<28);bus.reads=bus.writes=0;cpu.step();
  require(!cpu.state.unsupported && cpu.state.r==want && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"parallel result/GE/Q/condition/alias/access");++checks;
 };
 for(unsigned family:{1u,2u,3u,5u,6u,7u})for(unsigned op:{4u,7u})for(unsigned pair=0;pair<65536;++pair)for(unsigned flags:{0u,31u})check(family,op,pack_bytes(pair%256),pack_bytes(pair/256)^0xaaaaaaaa,flags,flags%16);
 for(unsigned family:{1u,2u,3u,5u,6u,7u})for(unsigned op=0;op<4;++op)for(unsigned value=0;value<65536;++value)for(uint32_t b:{0u,0xffffffffu,0x7fff8000u,0x80007fffu,0x0001ffffu,0xffff0001u,0x00010001u,0x80008000u})for(unsigned flags:{0u,31u})check(family,op,value|((65535u-value)<<16),b,flags,flags%16);
 for(unsigned ge=0;ge<16;++ge)for(unsigned pair=0;pair<65536;++pair)for(unsigned flags:{0u,31u})check(0,0,pack_bytes(pair%256),pack_bytes(pair/256)^0xaaaaaaaa,flags,ge);
 for(unsigned family:{0u,1u,2u,3u,5u,6u,7u})for(unsigned op:{0u,1u,2u,3u,4u,7u}){if(!family && op)continue;
  for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond)for(uint32_t a:{0u,0xffffffffu,0x80008000u,0x7fff7fffu})for(uint32_t b:{0xaaaaaaaau,0x55555555u})check(family,op,a,b,flags,ge,cond);
  for(unsigned rd=0;rd<15;++rd)for(unsigned rn=0;rn<15;++rn)for(unsigned rm=0;rm<15;++rm)check(family,op,0x80017fff,0xffff0001,31,(rd+rn+rm)%16,14,rd,rn,rm);
 }
 // Full GE byte and flag writes; reserved bits stay untouched. No bus data access.
 for(unsigned fields:{4u,12u})for(unsigned value=0;value<65536;++value)for(unsigned flags:{0u,31u}){
  cpu.reset(0x1000);cpu.state.cpsr=0xdf|(flags<<27)|0x50000;cpu.state.r[3]=(value<<16)|0x3df;
  const bool invalid=cpu.state.r[3]&~0xf80f03dfu;const uint32_t mask=0xf0000|((fields&8)?0xf8000000:0),psw=invalid?cpu.state.cpsr:(cpu.state.cpsr&~mask)|(cpu.state.r[3]&mask);auto regs=cpu.state.r;if(!invalid)regs[15]+=4;
  bus.ins=0xe120f003|(fields<<16);bus.reads=bus.writes=0;cpu.step();require(cpu.state.r==regs && cpu.state.cpsr==psw && cpu.state.unsupported==invalid && bus.reads==1 && !bus.writes,"MSR GE/reserved source diagnostic");++checks;
 }
 for(unsigned fields:{4u,12u})for(unsigned rotation=0;rotation<16;++rotation)for(unsigned value=0;value<256;++value){
  cpu.reset(0x1000);cpu.state.cpsr|=0xa8000000|0x50000;const unsigned amount=rotation*2;const uint32_t raw=amount?(value>>amount)|(value<<(32-amount)):value;
  const bool invalid=raw&~0xf80f03dfu;const uint32_t mask=0xf0000|((fields&8)?0xf8000000:0),psw=invalid?cpu.state.cpsr:(cpu.state.cpsr&~mask)|(raw&mask);auto regs=cpu.state.r;if(!invalid)regs[15]+=4;
  bus.ins=0xe320f000|(fields<<16)|(rotation<<8)|value;bus.reads=bus.writes=0;cpu.step();require(cpu.state.r==regs && cpu.state.cpsr==psw && cpu.state.unsupported==invalid && bus.reads==1 && !bus.writes,"MSR immediate GE/flags/diagnostic");++checks;
 }
 for(unsigned profile=0;profile<3;++profile)for(unsigned family=0;family<8;++family)for(unsigned op=0;op<8;++op)for(unsigned rd=0;rd<16;++rd)for(unsigned rn=0;rn<16;++rn)for(unsigned rm=0;rm<16;++rm){
  const bool valid=family && family!=4 && (op<5 || op==7);if(profile==2 && valid && rd<15 && rn<15 && rm<15)continue;if(!family)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);const auto before=target.state.r;bus.ins=encoding(family,op,rd,rn,rm);bus.reads=bus.writes=0;target.step();
  require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"parallel profile/reserved/PC diagnostic mutation");++checks;
 }
 // Reserved family zero is distinct from the separately encoded SEL.
 for(unsigned profile=0;profile<3;++profile)for(unsigned op=0;op<8;++op)for(unsigned rd=0;rd<16;++rd)for(unsigned rn=0;rn<16;++rn)for(unsigned rm=0;rm<16;++rm){
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);auto before=target.state.r;bus.ins=0xe6000f10|(op<<5)|(rd<<12)|(rn<<16)|rm;bus.reads=bus.writes=0;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"reserved parallel family zero");++checks;
 }
 for(unsigned family:{1u,2u,3u,5u,6u,7u})for(unsigned op:{0u,1u,2u,3u,4u,7u})for(unsigned reserved=0;reserved<15;++reserved){
  cpu.reset(0x1000);auto before=cpu.state.r;bus.ins=(encoding(family,op,4,0,8)&~0xf00u)|(reserved<<8);bus.reads=bus.writes=0;cpu.step();require(cpu.state.unsupported && cpu.state.r==before && cpu.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"parallel reserved fixed field");++checks;
 }
 for(unsigned fields:{4u,12u})for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned cond=0;cond<15;++cond){
  cpu.reset(0x1000);cpu.state.cpsr=0xdf|(flags<<27)|(ge<<16);cpu.state.r[3]=0x280a0000;
  const uint32_t mask=0xf0000|((fields&8)?0xf8000000:0),psw=predicate(cond,flags)?(cpu.state.cpsr&~mask)|(cpu.state.r[3]&mask):cpu.state.cpsr;auto regs=cpu.state.r;regs[15]+=4;
  bus.ins=(cond<<28)|0x0120f003|(fields<<16);bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==regs && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"conditional MSR GE effects");++checks;
 }
 for(unsigned profile=0;profile<3;++profile)for(unsigned rd=0;rd<16;++rd)for(unsigned rn=0;rn<16;++rn)for(unsigned rm=0;rm<16;++rm){if(profile==2 && rd<15 && rn<15 && rm<15)continue;
  nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);const auto before=target.state.r;bus.ins=encoding(0,0,rd,rn,rm);bus.reads=bus.writes=0;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf && bus.reads==1 && !bus.writes,"SEL profile/PC diagnostics");++checks;
 }
 for(unsigned profile=0;profile<3;++profile)for(unsigned fields:{4u,12u})for(unsigned rm=0;rm<16;++rm){if(profile==2 && rm!=15)continue;nds::Arm target(bus,profile>0,profile==2);target.reset(0x1000);auto before=target.state.r;bus.ins=0xe120f000|(fields<<16)|rm;target.step();require(target.state.unsupported && target.state.r==before && target.state.cpsr==0xdf,"MSR GE profile/PC diagnostics");++checks;}
 // In User mode execution-state source bits are ignored, while GE/NZCVQ remain writable.
 for(unsigned fields:{4u,12u})for(unsigned flags=0;flags<32;++flags)for(unsigned ge=0;ge<16;++ge)for(unsigned states=0;states<4;++states){
  cpu.reset(0x1000);cpu.state.cpsr=0x10|0x50000|0x98000000;cpu.state.r[3]=(flags<<27)|(ge<<16)|((states&1)<<5)|((states>>1)<<24);
  const uint32_t mask=0xf0000|((fields&8)?0xf8000000:0),psw=(cpu.state.cpsr&~mask)|(cpu.state.r[3]&mask);auto regs=cpu.state.r;regs[15]+=4;
  bus.ins=0xe120f003|(fields<<16);bus.reads=bus.writes=0;cpu.step();require(!cpu.state.unsupported && cpu.state.r==regs && cpu.state.cpsr==psw && bus.reads==1 && !bus.writes,"User GE write ignores execution-state source bits");++checks;
 }
 uint32_t psw=0xdf;require(oracle(5,1,0x00000000,0x00010000,psw)==0x0000ffff && !(psw&0xf0000),"UASX borrow GE anchor");psw=0xdf;
 require(oracle(5,1,0x00000001,0x00010000,psw)==0 && (psw&0xf0000)==0x30000,"UASX no borrow GE anchor");psw=0xdf;
 require(oracle(7,7,0,0x01010101,psw)==0xffffffff,"unsigned halving negative modulo anchor");
 std::cout<<checks<<" ARMv6 parallel/SEL/GE/MSR/profile observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
