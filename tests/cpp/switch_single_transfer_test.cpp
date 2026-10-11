#include "aarch64.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:nx::Memory {
 uint32_t instruction=0;std::array<uint8_t,65536> bytes{};uint64_t bad=~uint64_t(0);unsigned reads=0,writes=0;uint64_t observed=0;unsigned width=0;
 bool read(uint64_t at,unsigned size,uint64_t& value,bool code=false)override{if(code){value=instruction;return true;}++reads;observed=at;width=size;if(at==bad || at>bytes.size() || size>bytes.size()-at)return false;value=0;for(unsigned n=0;n<size;++n)value|=uint64_t(bytes[at+n])<<(8*n);return true;}
 bool write(uint64_t at,unsigned size,uint64_t value)override{++writes;observed=at;width=size;if(at==bad || at>bytes.size() || size>bytes.size()-at)return false;for(unsigned n=0;n<size;++n)bytes[at+n]=value>>(8*n);return true;}
};
static uint32_t encoding(unsigned size,unsigned op,unsigned kind,unsigned immediate,unsigned rt=0,unsigned rn=8,unsigned rm=11,unsigned option=3,bool scaled=false){
 const uint32_t base=0x38000000|(size<<30)|(op<<22)|(rn<<5)|rt;
 if(kind==4)return base|0x01000000|(immediate<<10);
 if(kind==5)return base|0x00200800|(rm<<16)|(option<<13)|(unsigned(scaled)<<12);
 return base|(immediate<<12)|(kind<<10);
}
static bool valid(unsigned size,unsigned op){return op<2 || (op==2 && size<3) || (op==3 && size<2);}
int main(){try{Bus bus;nx::Aarch64 cpu(bus);uint64_t checks=0;
 for(unsigned n=0;n<bus.bytes.size();++n)bus.bytes[n]=uint8_t(37*n);
 auto one=[&](unsigned size,unsigned op,unsigned kind,unsigned immediate,uint64_t seed,unsigned rt=0,unsigned rn=8,unsigned rm=11,unsigned option=3,bool scaled=false,uint64_t index=1,unsigned flags=15){
  cpu.state={};for(unsigned r=0;r<31;++r)cpu.state.x[r]=seed^(r*0x123456789abcdefull);cpu.state.sp=1024;cpu.state.nzcv=flags;
  if(kind==5 && rm<31)cpu.state.x[rm]=index;if(rt<31)cpu.state.x[rt]=seed;if(rn<31)cpu.state.x[rn]=1024;
  const auto before=cpu.state;auto expected=before;const unsigned bytes=1u<<size;
  uint64_t offset=0;
  if(kind==4)offset=uint64_t(immediate)*bytes;
  else if(kind==5){const uint64_t raw=rm==31?0:before.x[rm];const unsigned bits=option%2?64:32;offset=bits==32?raw%4294967296ull:raw;if(option>=6 && bits==32 && offset>=2147483648ull)offset+=0xffffffff00000000ull;offset*=scaled?bytes:1;}
  else offset=uint64_t(immediate<256?int(immediate):int(immediate)-512);
  const uint64_t base=rn==31?before.sp:before.x[rn],address=base+(kind==1?0:offset);
  require(address+bytes<=bus.bytes.size(),"test domain mapped");std::array<uint8_t,24> memory{};for(unsigned n=0;n<24;++n)memory[n]=bus.bytes[address-8+n];
  const auto source=rt==31?0:before.x[rt];
  if(!op){for(unsigned n=0;n<bytes;++n)memory[8+n]=source>>(n*8);}
  else {uint64_t value=0;for(unsigned n=0;n<bytes;++n)value+=uint64_t(memory[8+n])*(uint64_t(1)<<(n*8));if(op>=2 && value>=(uint64_t(1)<<(bytes*8-1))){value|=(~uint64_t(0))<<(bytes*8);}if(op==3)value%=4294967296ull;if(rt<31)expected.x[rt]=value;}
  if(kind==1 || kind==3){if(rn==31)expected.sp=base+offset;else expected.x[rn]=base+offset;}
  bus.instruction=encoding(size,op,kind,immediate,rt,rn,rm,option,scaled);bus.reads=bus.writes=0;bus.bad=~uint64_t(0);
  require(cpu.step() && cpu.state.x==expected.x && cpu.state.sp==expected.sp && cpu.state.nzcv==flags && cpu.state.pc==4 && bus.observed==address && bus.width==bytes && bus.reads==unsigned(op!=0) && bus.writes==unsigned(!op),"single memory bytes/sign/width/address/base/alias/flags");for(unsigned n=0;n<24;++n)require(bus.bytes[address-8+n]==memory[n],"single surrounding byte preservation");
  // Restore only bytes touched by stores, keeping the address-dependent source oracle stable.
  if(!op)for(unsigned n=0;n<bytes;++n)bus.bytes[address+n]=uint8_t(37*(address+n));++checks;
 };
 one(0,2,0,0,0); // New signed unscaled load must fail on the prior core.
 for(unsigned size=0;size<4;++size)for(unsigned op=0;op<4;++op)if(valid(size,op))for(unsigned kind=0;kind<4;++kind)for(unsigned immediate=0;immediate<512;++immediate)for(unsigned value=0;value<256;++value)for(unsigned rn:{8u,31u})one(size,op,kind,immediate,uint64_t(value)*0x0101010101010101ull,0,rn,11,3,false,1,value%16);
 for(unsigned size=0;size<4;++size)for(unsigned op=0;op<4;++op)if(valid(size,op))for(unsigned immediate=0;immediate<4096;++immediate)for(unsigned rn:{8u,31u})for(uint64_t value:{0ull,0x80000000ffffffffull,0xffffffffffffffffull})one(size,op,4,immediate,value,0,rn);
 for(unsigned size=0;size<4;++size)for(unsigned op=0;op<4;++op)if(valid(size,op))for(unsigned option:{2u,3u,6u,7u})for(bool scaled:{false,true})for(uint64_t index:{0ull,1ull,255ull,0xffffffff00000001ull,0xffffffffffffffffull}){
  if(option==2 && index==0xffffffffffffffffull)continue;if((option==3 || option==7) && index==0xffffffff00000001ull)continue;
  one(size,op,5,0,0x80000000ffffffffull,0,8,11,option,scaled,index);
 }
 for(unsigned size=0;size<4;++size)for(unsigned op=0;op<4;++op)if(valid(size,op))for(unsigned kind=0;kind<6;++kind)for(unsigned rn=0;rn<32;++rn)for(unsigned rt=0;rt<32;++rt){if((kind==1 || kind==3) && rn<31 && rn==rt)continue;one(size,op,kind,1,0x80000000ffffffffull,rt,rn,31,3,false,0);}
 for(unsigned size=0;size<4;++size)for(unsigned op=0;op<4;++op)if(valid(size,op))for(unsigned option:{2u,3u,6u,7u})for(bool scaled:{false,true})for(unsigned rn=0;rn<32;++rn)for(unsigned rt=0;rt<32;++rt)for(unsigned rm=0;rm<32;++rm)one(size,op,5,0,1,rt,rn,rm,option,scaled,1,rm%16);
 for(unsigned size=0;size<4;++size)for(unsigned op=0;op<4;++op)for(unsigned kind=0;kind<6;++kind)for(unsigned rn=0;rn<32;++rn)for(unsigned rt=0;rt<32;++rt){if(valid(size,op) && !((kind==1 || kind==3) && rn<31 && rn==rt))continue;cpu.state={};cpu.state.sp=1024;cpu.state.x[rn<31?rn:0]=1024;const auto before=cpu.state;bus.instruction=encoding(size,op,kind,0,rt,rn);bus.reads=bus.writes=0;require(!cpu.step() && cpu.state.fault==3 && cpu.state.x==before.x && cpu.state.sp==before.sp && !bus.reads && !bus.writes,"single reserved/overlap diagnostic");++checks;}
 for(unsigned option:{0u,1u,4u,5u}){cpu.state={};bus.instruction=encoding(2,1,5,0,0,8,11,option);bus.reads=bus.writes=0;require(!cpu.step() && cpu.state.fault==3 && !bus.reads && !bus.writes,"single reserved index extension");++checks;}
 for(unsigned kind=0;kind<6;++kind)for(unsigned op:{0u,1u,2u}){cpu.state={};cpu.state.x[8]=1024;cpu.state.x[0]=0x89abcdef;cpu.state.x[11]=0;const auto before=cpu.state;bus.bad=1024;bus.instruction=encoding(0,op,kind,0);require(!cpu.step() && cpu.state.fault==(op?4u:5u) && cpu.state.x==before.x && !cpu.state.pc && cpu.state.fault_address==1024,"single bus fault before destination/writeback");++checks;}
 for(unsigned kind=0;kind<6;++kind)for(unsigned misalign=1;misalign<16;++misalign){cpu.state={};cpu.state.sp=1024+misalign;bus.instruction=encoding(0,1,kind,0,0,31);bus.reads=bus.writes=0;require(!cpu.step() && cpu.state.fault==6 && !bus.reads && !bus.writes,"single SP alignment profile");++checks;}
 std::cout<<checks<<" AArch64 single transfer sign/address/bytes/flags/aliases/fault observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
