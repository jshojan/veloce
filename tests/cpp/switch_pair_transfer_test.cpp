#include "aarch64.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:nx::Memory {
 uint32_t instruction=0;std::array<uint8_t,4096> bytes{};uint64_t bad=~uint64_t(0);unsigned reads=0,writes=0;
 bool read(uint64_t at,unsigned size,uint64_t& value,bool code=false)override{if(code){value=instruction;return true;}++reads;if(at==bad || at>bytes.size() || size>bytes.size()-at)return false;value=0;for(unsigned n=0;n<size;++n)value|=uint64_t(bytes[at+n])<<(8*n);return true;}
 bool write(uint64_t at,unsigned size,uint64_t value)override{++writes;if(at==bad || at>bytes.size() || size>bytes.size()-at)return false;for(unsigned n=0;n<size;++n)bytes[at+n]=value>>(n*8);return true;}
};
static uint32_t encode(unsigned kind,unsigned mode,unsigned imm,unsigned rt=0,unsigned rt2=1,unsigned rn=8){const unsigned op=kind==4?1:kind%2?2:0;return 0x28000000|(op<<30)|((kind>=2)<<22)|(mode<<23)|(imm<<15)|(rt2<<10)|(rn<<5)|rt;}
int main(){try{Bus bus;nx::Aarch64 cpu(bus);uint64_t checks=0;
 auto one=[&](unsigned kind,unsigned mode,unsigned imm,uint64_t seed,unsigned rt=0,unsigned rt2=1,unsigned rn=8,unsigned misalign=0){
  cpu.state={};for(unsigned r=0;r<31;++r)cpu.state.x[r]=seed^(uint64_t(r)*0x0102030405060708ull);cpu.state.sp=2048;cpu.state.nzcv=seed&15;
  if(rn<31)cpu.state.x[rn]=2048+misalign;const auto before=cpu.state;auto expected=before;expected.pc=4;expected.steps=1;
  const unsigned size=kind%2 && kind!=4?8:4;const int displacement=(imm<64?int(imm):int(imm)-128)*int(size);
  const unsigned address=2048+misalign+(mode==1?0:displacement);
  for(unsigned n=0;n<bus.bytes.size();++n)bus.bytes[n]=uint8_t(seed+37*n);auto memory=bus.bytes;
  auto source=[&](unsigned r){return r==31?0:before.x[r];};
  for(unsigned lane=0;lane<2;++lane){const auto reg=lane?rt2:rt;const auto at=address+lane*size;
   if(kind<2){const auto value=source(reg);for(unsigned n=0;n<size;++n)memory[at+n]=uint8_t(value/(uint64_t(1)<<(8*n)));}
   else {uint64_t value=0;for(unsigned n=0;n<size;++n)value+=uint64_t(memory[at+n])*(uint64_t(1)<<(8*n));if(kind==4 && value>=2147483648ull)value+=0xffffffff00000000ull;if(reg<31)expected.x[reg]=value;}
  }
  if(mode==1 || mode==3){if(rn==31)expected.sp=2048+displacement;else expected.x[rn]=2048+misalign+displacement;}
  bus.instruction=encode(kind,mode,imm,rt,rt2,rn);bus.bad=~uint64_t(0);bus.reads=bus.writes=0;
  require(cpu.step() && cpu.state.x==expected.x && cpu.state.sp==expected.sp && cpu.state.pc==4 && cpu.state.nzcv==before.nzcv && bus.bytes==memory && bus.reads==(kind>=2?2u:0u) && bus.writes==(kind<2?2u:0u),"pair result/bytes/order/extension/writeback/flags/alias/access");++checks;
 };
 one(2,2,0,0x80000000); // Before implementation this reaches an unsupported opcode.
 for(unsigned kind=0;kind<5;++kind)for(unsigned mode=0;mode<4;++mode)if(kind!=4 || mode)for(unsigned imm=0;imm<128;++imm)for(unsigned value=0;value<256;++value)for(unsigned rn:{8u,31u})one(kind,mode,imm,uint64_t(value)*0x0101010101010101ull,0,1,rn);
 for(unsigned kind=0;kind<5;++kind)for(unsigned mode=0;mode<4;++mode)if(kind!=4 || mode)for(unsigned rn=0;rn<32;++rn)for(unsigned rt=0;rt<32;++rt)for(unsigned rt2=0;rt2<32;++rt2){
  if((kind>=2 && rt==rt2) || ((mode==1 || mode==3) && rn<31 && (rn==rt || rn==rt2)))continue;
  one(kind,mode,64,0xfedcba9876543210ull,rt,rt2,rn);
 }
 for(unsigned kind=0;kind<5;++kind)for(unsigned mode=0;mode<4;++mode)if(kind!=4 || mode)for(unsigned offset=1;offset<16;++offset)one(kind,mode,63,0x80000000ffffffffull,0,1,8,offset);
 // Exhaust the pair register fields for selected diagnostic-convention encodings.
 for(unsigned opc=0;opc<4;++opc)for(unsigned load=0;load<2;++load)for(unsigned mode=0;mode<4;++mode)for(unsigned rn=0;rn<32;++rn)for(unsigned rt=0;rt<32;++rt)for(unsigned rt2=0;rt2<32;++rt2){
  const bool invalid=opc==3 || (opc==1 && (!load || !mode)) || (load && rt==rt2) || ((mode==1 || mode==3) && rn<31 && (rn==rt || rn==rt2));if(!invalid)continue;
  cpu.state={};cpu.state.sp=2048;cpu.state.x[rn<31?rn:0]=2048;const auto before=cpu.state;const auto memory=bus.bytes;bus.instruction=0x28000000|(opc<<30)|(load<<22)|(mode<<23)|(rt2<<10)|(rn<<5)|rt;bus.reads=bus.writes=0;
  require(!cpu.step() && cpu.state.fault==3 && cpu.state.x==before.x && cpu.state.sp==before.sp && !cpu.state.pc && bus.bytes==memory && !bus.reads && !bus.writes,"pair reserved/overlap diagnostic before access");++checks;
 }
 for(unsigned kind:{0u,2u,4u})for(unsigned lane=0;lane<2;++lane){cpu.state={};cpu.state.x[8]=2048;cpu.state.x[0]=0x12345678;cpu.state.x[1]=0x89abcdef;const auto before=cpu.state;bus.instruction=encode(kind,3,0);bus.bad=2048+lane*4;bus.reads=bus.writes=0;
  require(!cpu.step() && cpu.state.fault==(kind?4u:5u) && cpu.state.fault_address==bus.bad && cpu.state.x==before.x && !cpu.state.pc,"pair fault leaves destinations/writeback/PC unchanged");require((kind?bus.reads:bus.writes)==lane+1,"pair first/second access fault order");++checks;
 }
 for(unsigned misalign=1;misalign<16;++misalign){cpu.state={};cpu.state.sp=2048+misalign;bus.instruction=encode(0,2,0,0,1,31);bus.reads=bus.writes=0;require(!cpu.step() && cpu.state.fault==6 && !bus.reads && !bus.writes,"pair SP alignment diagnostic");++checks;}
 std::cout<<checks<<" AArch64 paired transfer byte/extension/address/alias/fault observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
