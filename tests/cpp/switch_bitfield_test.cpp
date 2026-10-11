#include "aarch64.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Bus final:nx::Memory{uint32_t word=0;
 bool read(uint64_t at,unsigned size,uint64_t& value,bool instruction)override{if(at || size!=4 || !instruction)return false;value=word;return true;}
 bool write(uint64_t,unsigned,uint64_t)override{return false;}
};
// Independent contiguous field placement rather than architectural rotate/masks.
static uint64_t field(uint64_t source,uint64_t old,unsigned width,unsigned op,unsigned r,unsigned s){
 uint64_t result=0;const unsigned start=s>=r?0:width-r,length=s>=r?s-r+1:s+1;
 for(unsigned i=0;i<width;++i){bool bit=false;
  if(i>=start && i<start+length)bit=(source>>(s>=r?r+i:i-start))&1;
  else if(op==1)bit=(old>>i)&1;
  else if(op==0 && i>=start+length)bit=(source>>s)&1;
  if(bit)result|=uint64_t(1)<<i;
 }
 return result;
}
static uint64_t extract(uint64_t high,uint64_t low,unsigned width,unsigned amount){
 uint64_t result=0;for(unsigned i=0;i<width;++i){const unsigned at=i+amount;
  if((at<width?low>>at:high>>(at-width))&1)result|=uint64_t(1)<<i;
 }return result;
}
int main(){try{
 std::vector<uint64_t> values;for(unsigned b=0;b<256;++b)values.push_back(uint64_t(b)*0x0101010101010101ULL);
 for(uint64_t v:{1ULL,2ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffff00000000ULL,0xffffffff80000000ULL,0x8000000000000001ULL,0x0123456789abcdefULL,0xfedcba9876543210ULL,0x100000001ULL,0xfffffffeULL,0xfffffffffffffffeULL})values.push_back(v);
 require(field(0x80,0,64,0,0,7)==0xffffffffffffff80ULL && field(0xff,0,64,2,56,7)==0xff00 && field(0x12,~uint64_t(0),32,1,28,3)==0xffffff2f && extract(1,0,64,1)==0x8000000000000000ULL,"independent field references");
 Bus bus;nx::Aarch64 cpu(bus);uint64_t fields=0,extracts=0,aliases=0;
 for(unsigned width:{32u,64u})for(unsigned op=0;op<3;++op)for(unsigned r=0;r<width;++r)for(unsigned s=0;s<width;++s)for(auto a:values)for(unsigned seed=0;seed<2;++seed){
  const uint64_t old=seed?~uint64_t(0):0,want=field(a,old,width,op,r,s);
  bus.word=0x13000002|(op<<29)|(width==64?0x80400000:0)|(r<<16)|(s<<10);
  for(unsigned flags=0;flags<16;++flags){cpu.state={};cpu.state.x[0]=a;cpu.state.x[2]=old;cpu.state.x[15]=0x123456789abcdef0ULL;cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;
   auto expected=cpu.state.x;expected[2]=want;require(cpu.step(),"bitfield encoding unsupported");
   require(cpu.state.x==expected && cpu.state.nzcv==flags && cpu.state.sp==0xabcdef01 && cpu.state.pc==4 && cpu.state.steps==1,"field result or unrelated-state preservation");++fields;
  }
 }
 for(unsigned width:{32u,64u})for(unsigned amount=0;amount<width;++amount)for(auto a:values)for(auto b:values){
  const auto want=extract(a,b,width,amount);bus.word=0x13810002|(width==64?0x80400000:0)|(amount<<10);
  for(unsigned flags=0;flags<16;++flags){cpu.state={};cpu.state.x[0]=a;cpu.state.x[1]=b;cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;auto expected=cpu.state.x;expected[2]=want;
   require(cpu.step() && cpu.state.x==expected && cpu.state.nzcv==flags && cpu.state.sp==0xabcdef01 && cpu.state.pc==4 && cpu.state.steps==1,"EXTR order, width or flag preservation");++extracts;
  }
 }
 for(unsigned width:{32u,64u})for(unsigned op=0;op<4;++op)for(unsigned r=0;r<32;++r)for(unsigned alias=0;alias<5;++alias)for(unsigned flags=0;flags<16;++flags)for(unsigned shape=0;shape<4;++shape){
  const unsigned rn=r,rm=(r+7)%32,rd=alias==0?rn:alias==1?rm:alias==2?31:alias==3?(r+17)%32:2;
  const unsigned immr=shape==0?0:shape==1?1:width-1,imms=shape==0?width-1:shape==1?0:shape==2?width-1:0;
  cpu.state={};for(unsigned i=0;i<31;++i)cpu.state.x[i]=0x8000000100000001ULL+i*0x0101020304050607ULL;
  cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;auto expected=cpu.state.x;
  const auto get=[&](unsigned n){return n==31?0:expected[n];};
  const auto want=op==3?extract(get(rn),get(rm),width,immr):field(get(rn),get(rd),width,op,immr,imms);if(rd!=31)expected[rd]=want;
  bus.word=(op==3?0x13800000:0x13000000|(op<<29))|(width==64?0x80400000:0)|rd|(rn<<5)|(op==3?rm<<16:immr<<16)|(op==3?immr<<10:imms<<10);
  require(cpu.step() && cpu.state.x==expected && cpu.state.nzcv==flags && cpu.state.sp==0xabcdef01,"field/extract aliases and XZR");++aliases;
 }
 for(uint32_t word:{0x13400002u,0x93000002u,0x13200002u,0x13008002u,0x73000002u,0xf3400002u,0x13c10002u,0x93810002u,0x13818002u,0x13a10002u,0x33810002u}){
  cpu.state={};cpu.state.x[2]=0x1234;cpu.state.nzcv=15;bus.word=word;
  require(!cpu.step() && cpu.state.fault==3 && cpu.state.fault_word==word && !cpu.state.pc && cpu.state.x[2]==0x1234 && cpu.state.nzcv==15,"reserved field/extract rejection");
 }
 std::cout<<fields<<" field + "<<extracts<<" extract + "<<aliases<<" alias snapshots PASS; reserved controls reject\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
