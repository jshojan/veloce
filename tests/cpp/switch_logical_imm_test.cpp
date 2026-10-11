#include "aarch64.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Bus final:nx::Memory{uint32_t word=0;
 bool read(uint64_t at,unsigned size,uint64_t& value,bool instruction)override{if(at || size!=4 || !instruction)return false;value=word;return true;}
 bool write(uint64_t,unsigned,uint64_t)override{return false;}
};
struct Mask{bool valid=false;uint64_t value=0;};
// Forward construction from element/run lengths, independent of DecodeBitMasks.
static auto table(unsigned width){
 std::array<Mask,8192> out{};
 for(unsigned element=2;element<=width;element*=2)for(unsigned ones=1;ones<element;++ones)for(unsigned r=0;r<64;++r){
  const unsigned n=element==64,s=(~(element*2-1)&63)|(ones-1),index=(n<<12)|(r<<6)|s;
  uint64_t value=0;for(unsigned bit=0;bit<width;++bit)if((bit+r)%element<ones)value|=uint64_t(1)<<bit;
  require(!out[index].valid,"duplicate forward mask encoding");out[index]={true,value};
 }return out;
}
static uint64_t result(uint64_t a,uint64_t mask,unsigned op,unsigned width){
 uint64_t out=0;for(unsigned bit=0;bit<width;++bit){const bool x=(a>>bit)&1,y=(mask>>bit)&1;
  if(op==0 || op==3?x&&y:op==1?x||y:x!=y)out|=uint64_t(1)<<bit;
 }return out;
}
static unsigned flags_for(uint64_t value,unsigned width){return unsigned(bool(value>>(width-1)))*8+unsigned(value==0)*4;}
int main(){try{
 std::vector<uint64_t> values;for(unsigned b=0;b<256;++b)values.push_back(uint64_t(b)*0x0101010101010101ULL);
 for(uint64_t v:{1ULL,2ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffff00000000ULL,0xffffffff80000000ULL,0x8000000000000001ULL,0x0123456789abcdefULL,0xfedcba9876543210ULL,0x100000001ULL,0xfffffffeULL,0xfffffffffffffffeULL})values.push_back(v);
 Bus bus;nx::Aarch64 cpu(bus);uint64_t snapshots=0,aliases=0,rejected=0;unsigned legal=0;
 for(unsigned width:{32u,64u}){
  const auto masks=table(width);require(masks[0].valid && masks[0].value==(width==32?1ULL:0x100000001ULL) && !masks[63].valid,"forward mask references");
  for(unsigned index=0;index<8192;++index){const unsigned n=index>>12,r=(index>>6)&63,s=index&63;
   for(unsigned op=0;op<4;++op){bus.word=0x12000002|(width==64?0x80000000:0)|(op<<29)|(n<<22)|(r<<16)|(s<<10);
    if(!masks[index].valid){cpu.state={};cpu.state.x[2]=0x1234;cpu.state.nzcv=15;cpu.state.sp=0xabcdef01;
     require(!cpu.step() && cpu.state.fault==3 && cpu.state.fault_word==bus.word && !cpu.state.pc && cpu.state.x[2]==0x1234 && cpu.state.nzcv==15 && cpu.state.sp==0xabcdef01,"reserved logical mask atomicity");++rejected;continue;}
    if(op==0)++legal;
    for(auto a:values){const auto want=result(a,masks[index].value,op,width);
     for(unsigned flags=0;flags<16;++flags){cpu.state={};cpu.state.x[0]=a;cpu.state.x[15]=0x123456789abcdef0ULL;cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;
      auto expected=cpu.state.x;expected[2]=want;require(cpu.step(),"logical-immediate encoding unsupported");
      require(cpu.state.x==expected && cpu.state.nzcv==(op==3?flags_for(want,width):flags) && cpu.state.sp==0xabcdef01 && cpu.state.pc==4 && cpu.state.steps==1,"logical immediate result/flags/source preservation");++snapshots;
     }
    }
   }
  }
  for(unsigned op=0;op<4;++op)for(unsigned r=0;r<32;++r)for(unsigned alias=0;alias<4;++alias)for(unsigned flags=0;flags<16;++flags)for(unsigned shape=0;shape<6;++shape){
   const unsigned rn=r,rd=alias==0?rn:alias==1?31:alias==2?(r+17)%32:2;
   const unsigned element=shape==0?2:shape==1?4:shape==2?8:shape==3?16:shape==4?32:width,ones=element-1,rotate=shape*11;
   const unsigned n=element==64,s=(~(element*2-1)&63)|(ones-1);const auto mask=masks[(n<<12)|(rotate<<6)|s].value;
   cpu.state={};for(unsigned i=0;i<31;++i)cpu.state.x[i]=0x8000000100000001ULL+i*0x0101020304050607ULL;
   cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;auto expected=cpu.state.x;uint64_t sp=cpu.state.sp;
   const auto want=result(rn==31?0:expected[rn],mask,op,width);if(rd!=31)expected[rd]=want;else if(op!=3)sp=want;
   bus.word=0x12000000|(width==64?0x80000000:0)|(op<<29)|(n<<22)|(rotate<<16)|(s<<10)|(rn<<5)|rd;
   require(cpu.step() && cpu.state.x==expected && cpu.state.sp==sp && cpu.state.nzcv==(op==3?flags_for(want,width):flags),"logical immediate aliases/SP/XZR routing");++aliases;
  }
 }
 require(legal==11328 && rejected==20224,"legal/reserved encoded mask totals");
 std::cout<<snapshots<<" logical-immediate + "<<aliases<<" alias snapshots PASS; "<<rejected<<" reserved encodings reject; "<<legal<<" masks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
