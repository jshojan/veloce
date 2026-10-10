#include "aarch64.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Bus final : nx::Memory {
 uint32_t word=0;
 bool read(uint64_t at,unsigned size,uint64_t& value,bool instruction) override{if(at || size!=4 || !instruction)return false;value=word;return true;}
 bool write(uint64_t,unsigned,uint64_t) override{return false;}
};
struct Wide{uint64_t low=0,high=0;};
static Wide multiply(uint64_t a,uint64_t b){
 Wide out;
 for(unsigned bit=0;bit<64;++bit)if((b>>bit)&1){const auto old=out.low;out.low+=a<<bit;out.high+=(bit?a>>(64-bit):0)+unsigned(out.low<old);}
 return out;
}
static Wide signed_product(uint64_t a,uint64_t b,unsigned width){
 const uint64_t mask=width==64?~uint64_t(0):0xffffffffu,sign=uint64_t(1)<<(width-1);
 a&=mask;b&=mask;const bool negative=bool(a&sign)!=bool(b&sign);
 auto out=multiply((a&sign)?(-a)&mask:a,(b&sign)?(-b)&mask:b);
 if(negative){out.low=~out.low+1;out.high=~out.high+unsigned(out.low==0);}
 return out;
}
static uint64_t long_divide(uint64_t a,uint64_t b,unsigned width){
 if(!b)return 0;
 uint64_t remainder=0,quotient=0;
 for(unsigned left=width;left>0;--left){const unsigned bit=left-1;const bool carry=remainder>>63;remainder=(remainder<<1)|((a>>bit)&1);if(carry || remainder>=b){remainder-=b;quotient|=uint64_t(1)<<bit;}}
 return quotient;
}
static uint64_t divide(uint64_t a,uint64_t b,unsigned width,bool is_signed){
 const uint64_t mask=width==64?~uint64_t(0):0xffffffffu,sign=uint64_t(1)<<(width-1);
 a&=mask;b&=mask;
 if(!is_signed)return long_divide(a,b,width);
 const bool negative=bool(a&sign)!=bool(b&sign);
 const auto value=long_divide((a&sign)?(-a)&mask:a,(b&sign)?(-b)&mask:b,width);
 return (negative?-value:value)&mask;
}
static uint64_t expected(unsigned form,uint64_t a,uint64_t b,uint64_t accumulator){
 const unsigned width=form<4?32:64;const uint64_t mask=width==64?~uint64_t(0):0xffffffffu;
 if(form==2 || form==6)return divide(a,b,width,false);
 if(form==3 || form==7)return divide(a,b,width,true);
 if(form==12)return signed_product(a,b,64).high;
 if(form==13)return multiply(a,b).high;
 const bool subtract=form==1 || form==5 || form==9 || form==11;
 const auto product=form==8 || form==9?signed_product(a,b,32).low:
    form==10 || form==11?multiply(a&0xffffffffu,b&0xffffffffu).low:multiply(a&mask,b&mask).low;
 return (subtract?accumulator-product:accumulator+product)&mask;
}
int main(){try{
 const uint32_t words[]={0x1b010c02,0x1b018c02,0x1ac10802,0x1ac10c02,0x9b010c02,0x9b018c02,0x9ac10802,0x9ac10c02,0x9b210c02,0x9b218c02,0x9ba10c02,0x9ba18c02,0x9b417c02,0x9bc17c02};
 std::vector<uint64_t> values;for(unsigned value=0;value<256;++value)values.push_back(uint64_t(value)*0x0101010101010101ULL);
 for(uint64_t value:{1ULL,2ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffff00000000ULL,0xffffffff80000000ULL,0x8000000000000001ULL,0x0123456789abcdefULL,0xfedcba9876543210ULL,0x0000000100000001ULL,0xfffffffeULL,0xfffffffffffffffeULL})values.push_back(value);
 require(expected(7,0x8000000000000000ULL,~uint64_t(0),0)==0x8000000000000000ULL && expected(3,0x80000000,0xffffffff,0)==0x80000000,"signed divide overflow reference");
 require(expected(6,1,0,0)==0 && expected(7,~uint64_t(0),0,0)==0 && expected(12,~uint64_t(0),2,0)==~uint64_t(0),"zero divisor/high-product reference");
 Bus bus;nx::Aarch64 cpu(bus);uint64_t cases=0;
 for(auto a:values)for(auto b:values)for(unsigned form=0;form<14;++form)for(unsigned seed=0;seed<2;++seed){
  const auto accumulator=seed?~uint64_t(0):0,want=expected(form,a,b,accumulator);bus.word=words[form];
  for(unsigned flags=0;flags<16;++flags){
   cpu.state={};cpu.state.x[0]=a;cpu.state.x[1]=b;cpu.state.x[3]=accumulator;cpu.state.x[15]=0x13579bdf2468ace0ULL;cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;
   require(cpu.step(),"multiply/divide encoding unsupported");
   require(cpu.state.x[2]==want && cpu.state.nzcv==flags && cpu.state.x[0]==a && cpu.state.x[1]==b && cpu.state.x[3]==accumulator && cpu.state.x[15]==0x13579bdf2468ace0ULL && cpu.state.sp==0xabcdef01 && cpu.state.pc==4 && cpu.state.steps==1,"multiply/divide result or unrelated-state preservation");++cases;
  }
 }
 // All register numbers in each role, destructive aliases, XZR and SP isolation.
 for(unsigned form=0;form<14;++form)for(unsigned r=0;r<32;++r)for(unsigned alias=0;alias<6;++alias){
  const unsigned rn=r,rm=(r+7)%32,ra=(r+13)%32,rd=alias==0?rn:alias==1?rm:alias==2?ra:alias==3?31:alias==4?(r+17)%32:3;
  cpu.state={};for(unsigned reg=0;reg<31;++reg)cpu.state.x[reg]=0x8000000100000001ULL+reg*0x0101020304050607ULL;
  cpu.state.sp=0x12345678;cpu.state.nzcv=15;auto want=cpu.state.x;
  const auto operand=[&](unsigned reg){return reg==31?0:want[reg];};
  const auto result=expected(form,operand(rn),operand(rm),operand(ra));if(rd!=31)want[rd]=result;
  const bool three_source=form!=2 && form!=3 && form!=6 && form!=7 && form!=12 && form!=13;
  bus.word=(words[form]&~(31u|(31u<<5)|(31u<<16)|(three_source?31u<<10:0)))|rd|(rn<<5)|(rm<<16)|(three_source?ra<<10:0);
  require(cpu.step() && cpu.state.x==want && cpu.state.nzcv==15 && cpu.state.sp==0x12345678,"register alias/XZR multiply-divide routing");++cases;
 }
 for(unsigned type:{1u,2u,3u,4u,5u,6u,7u}){
  bus.word=0x1b010c02|(type<<21);cpu.state={};cpu.state.x[2]=0x1234;
  require(!cpu.step() && cpu.state.fault==3 && !cpu.state.pc && cpu.state.x[2]==0x1234,"32-bit unallocated long/high multiply rejection");
 }
 for(uint32_t word:{0x9b610c02u,0x9b810c02u,0x9be10c02u,0x9b410c02u,0x9b41fc02u,0x9bc10c02u,0x9bc1fc02u,0x1ac18002u}){
  bus.word=word;cpu.state={};cpu.state.x[2]=0x1234;
  require(!cpu.step() && cpu.state.fault==3 && cpu.state.fault_word==word && !cpu.state.pc && cpu.state.x[2]==0x1234,"unallocated multiply/divide diagnostic atomicity");
 }
 std::cout<<cases<<" multiply/divide snapshots, flag/source preservation, register aliases/XZR and reserved controls PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
