#include "aarch64.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Bus final:nx::Memory{uint32_t word=0;
 bool read(uint64_t at,unsigned size,uint64_t& value,bool instruction)override{if(at || size!=4 || !instruction)return false;value=word;return true;}
 bool write(uint64_t,unsigned,uint64_t)override{return false;}
};
static bool holds(unsigned flags,unsigned cond){const unsigned truth[]={0xf0f0,0x0f0f,0xcccc,0x3333,0xff00,0x00ff,0xaaaa,0x5555,0x0c0c,0xf3f3,0xaa55,0x55aa,0x0a05,0xf5fa,0xffff,0xffff};return (truth[cond]>>flags)&1;}
// Independent one-bit full adder; signed overflow is carry into/out of sign bit.
static unsigned arithmetic(uint64_t a,uint64_t b,unsigned width,bool subtract){
 unsigned carry=subtract,sign_carry=0;uint64_t result=0;
 for(unsigned bit=0;bit<width;++bit){const unsigned left=(a>>bit)&1,right=((b>>bit)&1)^unsigned(subtract);
  if(bit==width-1)sign_carry=carry;
  const unsigned sum=left+right+carry;if(sum&1)result|=uint64_t(1)<<bit;carry=sum>>1;
 }return unsigned(result>>(width-1))*8+unsigned(result==0)*4+carry*2+(sign_carry^carry);
}
int main(){try{
 const std::vector<uint64_t> values={0,1,2,0x7fffffff,0x80000000,0xffffffff,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,~uint64_t(0),0xffffffff00000000ULL,0xffffffff80000000ULL,0x8000000000000001ULL,0x0123456789abcdefULL,0xfedcba9876543210ULL,0xfffffffffffffffeULL};
 require(arithmetic(0,0,64,true)==6 && arithmetic(0x7fffffff,1,32,false)==9 && arithmetic(0x80000000,1,32,true)==3,"independent compare flags references");
 Bus bus;nx::Aarch64 cpu(bus);uint64_t registers=0,immediates=0,aliases=0;
 for(unsigned width:{32u,64u})for(bool immediate:{false,true})for(bool subtract:{false,true})for(auto a:values){
  std::vector<uint64_t> rights=values;if(immediate){rights.clear();for(unsigned i=0;i<32;++i)rights.push_back(i);}
  for(auto b:rights){const auto calculated=arithmetic(a,b,width,subtract);
   for(unsigned flags=0;flags<16;++flags)for(unsigned cond=0;cond<16;++cond)for(unsigned fallback=0;fallback<16;++fallback){
    cpu.state={};cpu.state.x[0]=a;cpu.state.x[1]=b;cpu.state.x[15]=0x123456789abcdef0ULL;cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;const auto expected=cpu.state.x;
    bus.word=0x3a400000|(width==64?0x80000000:0)|(subtract?0x40000000:0)|(immediate?0x800:0)|((immediate?unsigned(b):1u)<<16)|(cond<<12)|fallback;
    require(cpu.step(),"conditional-compare encoding unsupported");
    require(cpu.state.x==expected && cpu.state.nzcv==(holds(flags,cond)?calculated:fallback) && cpu.state.sp==0xabcdef01 && cpu.state.pc==4 && cpu.state.steps==1,"conditional compare branch, arithmetic or state preservation");
    if(immediate)++immediates;else ++registers;
   }
  }
 }
 for(unsigned width:{32u,64u})for(bool subtract:{false,true})for(unsigned r=0;r<32;++r)for(unsigned shape=0;shape<3;++shape)for(unsigned flags=0;flags<16;++flags)for(unsigned cond=0;cond<16;++cond)for(unsigned fallback=0;fallback<16;++fallback){
  const unsigned rn=shape==2?31:r,rm=shape==0?r:shape==1?(r+7)%32:r;
  cpu.state={};for(unsigned i=0;i<31;++i)cpu.state.x[i]=0x8000000100000001ULL+i*0x0101020304050607ULL;
  cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;const auto expected=cpu.state.x;
  const auto get=[&](unsigned n){return n==31?0:expected[n];};const auto calculated=arithmetic(get(rn),get(rm),width,subtract);
  bus.word=0x3a400000|(width==64?0x80000000:0)|(subtract?0x40000000:0)|(rm<<16)|(rn<<5)|(cond<<12)|fallback;
  require(cpu.step() && cpu.state.x==expected && cpu.state.nzcv==(holds(flags,cond)?calculated:fallback) && cpu.state.sp==0xabcdef01,"conditional compare operand aliases/XZR");++aliases;
 }
 for(uint32_t base:{0x3a410000u,0x7a410000u,0xba410000u,0xfa410000u,0x3a410800u,0x7a410800u,0xba410800u,0xfa410800u})for(unsigned control=0;control<3;++control){
  bus.word=control==0?base|16:control==1?base|1024:base&~(1u<<29);cpu.state={};cpu.state.x[0]=0x1234;cpu.state.nzcv=15;cpu.state.sp=0xabcdef01;const auto expected=cpu.state.x;
  require(!cpu.step() && cpu.state.fault==3 && cpu.state.fault_word==bus.word && cpu.state.x==expected && cpu.state.nzcv==15 && cpu.state.sp==0xabcdef01 && !cpu.state.pc,"reserved compare encoding atomicity");
 }
 std::cout<<registers<<" register + "<<immediates<<" immediate + "<<aliases<<" alias snapshots PASS; all conditions/input/fallback NZCV, reserved controls reject\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
