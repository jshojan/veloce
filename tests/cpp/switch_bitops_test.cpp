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
// Independent per-bit source mapping; no native shift/reverse/count helpers.
static uint64_t expected(uint64_t a,uint64_t b,unsigned form){
 const unsigned width=form<4 || (form>=8 && form<13)?32:64;
 const auto bit=[&](unsigned n){return bool((a>>n)&1);};uint64_t result=0;
 if(form<8){const unsigned op=form%4,amount=b%width;
  for(unsigned i=0;i<width;++i){bool set=false;
   if(op==0)set=i>=amount && bit(i-amount);
   else if(op==1)set=i+amount<width && bit(i+amount);
   else if(op==2)set=bit(i+amount<width?i+amount:width-1);
   else set=bit((i+amount)%width);
   if(set)result|=uint64_t(1)<<i;
  }
 }else{
  const unsigned op=form<13?form-8:form-13;
  // Table order: RBIT, REV16, REV32, [REV64], CLZ, CLS.
  const unsigned count_op=width==32?3:4;
  if(op>=count_op){const bool wanted=op==count_op?false:bit(width-1);unsigned count=0;
   for(unsigned i=width;i>0 && bit(i-1)==wanted;--i)++count;
   result=op==count_op?count:count-1;
  }else for(unsigned i=0;i<width;++i){unsigned source=width-1-i;
   if(op>0){const unsigned block=op==1?16:op==2?32:64;source=(i/block)*block+(block/8-1-(i%block)/8)*8+i%8;}
   if(bit(source))result|=uint64_t(1)<<i;
  }
 }
 return result;
}
int main(){try{
 const uint32_t words[]={0x1ac12002,0x1ac12402,0x1ac12802,0x1ac12c02,0x9ac12002,0x9ac12402,0x9ac12802,0x9ac12c02,
  0x5ac00002,0x5ac00402,0x5ac00802,0x5ac01002,0x5ac01402,0xdac00002,0xdac00402,0xdac00802,0xdac00c02,0xdac01002,0xdac01402};
 std::vector<uint64_t> values;for(unsigned b=0;b<256;++b)values.push_back(uint64_t(b)*0x0101010101010101ULL);
 for(uint64_t value:{1ULL,2ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffff00000000ULL,0xffffffff80000000ULL,0x8000000000000001ULL,0x0123456789abcdefULL,0xfedcba9876543210ULL,0x100000001ULL,0xfffffffeULL,0xfffffffffffffffeULL})values.push_back(value);
 std::vector<uint64_t> counts;for(unsigned i=0;i<256;++i)counts.push_back(i);
 for(uint64_t v:{0x10000ULL,0x100000000ULL,0x8000000000000000ULL,0xffffffff00000001ULL,0xffffffffffffffffULL,0xffffffffffffffc0ULL})counts.push_back(v);
 require(expected(1,0,8)==0x80000000 && expected(0,0,11)==32 && expected(~uint64_t(0),0,18)==63 && expected(0x0123456789abcdefULL,0,16)==0xefcdab8967452301ULL,"independent bit reference");
 Bus bus;nx::Aarch64 cpu(bus);uint64_t cases=0;
 for(unsigned form=0;form<19;++form)for(auto a:values){
  const auto& shifts=form<8?counts:std::vector<uint64_t>{0};
  for(auto b:shifts){const auto result=expected(a,b,form);
   for(unsigned flags=0;flags<16;++flags){cpu.state={};cpu.state.x[0]=a;cpu.state.x[1]=b;cpu.state.x[15]=0x123456789abcdef0ULL;cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;
    auto want=cpu.state.x;want[2]=result;bus.word=words[form];
    require(cpu.step(),"bit-operation encoding unsupported");
    require(cpu.state.x==want && cpu.state.nzcv==flags && cpu.state.sp==0xabcdef01 && cpu.state.pc==4 && cpu.state.steps==1,"bit result or unrelated-state preservation");++cases;
   }
  }
 }
 for(unsigned form=0;form<19;++form)for(unsigned r=0;r<32;++r)for(unsigned alias=0;alias<5;++alias)for(unsigned flags=0;flags<16;++flags){
  const unsigned rn=r,rm=(r+7)%32,rd=alias==0?rn:alias==1?rm:alias==2?31:alias==3?(r+17)%32:2;
  cpu.state={};for(unsigned i=0;i<31;++i)cpu.state.x[i]=0x8000000100000001ULL+i*0x0101020304050607ULL;
  cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;auto want=cpu.state.x;
  const auto result=expected(rn==31?0:want[rn],rm==31?0:want[rm],form);if(rd!=31)want[rd]=result;
  bus.word=(words[form]&~(31u|(31u<<5)|(form<8?31u<<16:0)))|rd|(rn<<5)|(form<8?rm<<16:0);
  require(cpu.step() && cpu.state.x==want && cpu.state.nzcv==flags && cpu.state.sp==0xabcdef01,"bit alias/XZR routing");++cases;
 }
 for(uint32_t word:{0x5ac00c02u,0x5ac01802u,0xdac01802u,0x5ac10002u,0xdac10002u,0x7ac00002u,0x3ac12002u,0x1ac13002u}){
  cpu.state={};cpu.state.x[2]=0x1234;cpu.state.nzcv=15;bus.word=word;
  require(!cpu.step() && cpu.state.fault==3 && cpu.state.fault_word==word && !cpu.state.pc && cpu.state.x[2]==0x1234 && cpu.state.nzcv==15,"unallocated bit operation rejection");
 }
 std::cout<<cases<<" bit-operation snapshots, masked counts, source/flag preservation, aliases/XZR and reserved controls PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
