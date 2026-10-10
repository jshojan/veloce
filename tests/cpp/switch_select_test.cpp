#include "aarch64.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Bus final:nx::Memory{
 uint32_t word=0;
 bool read(uint64_t at,unsigned size,uint64_t& value,bool instruction)override{if(at || size!=4 || !instruction)return false;value=word;return true;}
 bool write(uint64_t,unsigned,uint64_t)override{return false;}
};
// Enumerate complete truth sets independently of the decoder's condition pairs.
static bool holds(unsigned flags,unsigned cond){
 const unsigned truth[]={0xf0f0,0x0f0f,0xcccc,0x3333,0xff00,0x00ff,0xaaaa,0x5555,
                         0x0c0c,0xf3f3,0xaa55,0x55aa,0x0a05,0xf5fa,0xffff,0xffff};
 return (truth[cond]>>flags)&1;
}
static uint64_t expected(uint64_t a,uint64_t b,unsigned flags,unsigned cond,unsigned form){
 const unsigned width=form<4?32:64;uint64_t value=a;
 if(!holds(flags,cond)){
  // Independent bit-level reference, including carry propagation and inversion.
  switch(form%4){case 0:value=b;break;case 1:{value=b;unsigned bit=0;while(bit<64 && ((b>>bit)&1)){value&=~(uint64_t(1)<<bit);++bit;}if(bit<64)value|=uint64_t(1)<<bit;break;}
   case 2:value=0;for(unsigned bit=0;bit<64;++bit)if(!((b>>bit)&1))value|=uint64_t(1)<<bit;break;
   case 3:value=0;bool seen=false;for(unsigned bit=0;bit<64;++bit){const bool source=(b>>bit)&1;if(source!=seen)value|=uint64_t(1)<<bit;seen|=source;}break;}
 }
 return width==32?uint32_t(value):value;
}
int main(){try{
 const uint32_t words[]={0x1a810002,0x1a810402,0x5a810002,0x5a810402,0x9a810002,0x9a810402,0xda810002,0xda810402};
 std::vector<uint64_t> values;for(unsigned b=0;b<256;++b)values.push_back(uint64_t(b)*0x0101010101010101ULL);
 for(uint64_t value:{1ULL,2ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffff00000000ULL,0xffffffff80000000ULL,0x8000000000000001ULL,0x0123456789abcdefULL,0xfedcba9876543210ULL,0x100000001ULL,0xfffffffeULL,0xfffffffffffffffeULL})values.push_back(value);
 Bus bus;nx::Aarch64 cpu(bus);uint64_t cases=0;
 require(expected(123,~uint64_t(0),0,0,5)==0 && expected(123,0x80000000,0,0,3)==0x80000000,"independent select reference");
 for(auto a:values)for(auto b:values)for(unsigned form=0;form<8;++form)for(unsigned flags=0;flags<16;++flags)for(unsigned cond=0;cond<16;++cond){
  cpu.state={};cpu.state.x[0]=a;cpu.state.x[1]=b;cpu.state.x[15]=0x123456789abcdef0ULL;cpu.state.sp=0x12345678;cpu.state.nzcv=flags;
  auto want=cpu.state.x;want[2]=expected(a,b,flags,cond,form);bus.word=words[form]|(cond<<12);
  require(cpu.step(),"conditional-select encoding unsupported");
  require(cpu.state.x==want && cpu.state.nzcv==flags && cpu.state.sp==0x12345678 && cpu.state.pc==4 && cpu.state.steps==1,"select result/flags/source preservation");++cases;
 }
 for(unsigned form=0;form<8;++form)for(unsigned r=0;r<32;++r)for(unsigned alias=0;alias<5;++alias)for(unsigned flags=0;flags<16;++flags)for(unsigned cond=0;cond<16;++cond){
  const unsigned rn=r,rm=(r+7)%32,rd=alias==0?rn:alias==1?rm:alias==2?31:alias==3?(r+17)%32:2;
  cpu.state={};for(unsigned i=0;i<31;++i)cpu.state.x[i]=0x8000000100000001ULL+i*0x0101020304050607ULL;
  cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;auto want=cpu.state.x;
  const auto a=rn==31?0:want[rn],b=rm==31?0:want[rm],result=expected(a,b,flags,cond,form);if(rd!=31)want[rd]=result;
  bus.word=(words[form]&~(31u|(31u<<5)|(31u<<16)))|rd|(rn<<5)|(rm<<16)|(cond<<12);
  require(cpu.step() && cpu.state.x==want && cpu.state.nzcv==flags && cpu.state.sp==0xabcdef01,"select alias/XZR routing");++cases;
 }
 for(auto word:words)for(unsigned reserved:{1u<<29,1u<<11}){
  cpu.state={};cpu.state.x[2]=0x1234;cpu.state.nzcv=15;bus.word=word|reserved;
  require(!cpu.step() && cpu.state.fault==3 && cpu.state.fault_word==bus.word && !cpu.state.pc && cpu.state.x[2]==0x1234 && cpu.state.nzcv==15,"reserved select diagnostic atomicity");
 }
 std::cout<<cases<<" conditional-select snapshots, all conditions/NZCV, aliases/XZR and reserved controls PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
