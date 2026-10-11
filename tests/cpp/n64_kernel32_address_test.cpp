#include "vr4300.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:n64::Memory {
 uint32_t instruction=0;unsigned reads=0,writes=0;uint64_t last=0;
 bool read(uint64_t address,unsigned bytes,uint64_t& value)override{++reads;last=address;value=(uint32_t(address)==0xa0004000 && bytes==4)?instruction:0x89abcdef01234567ull;return true;}
 bool write(uint64_t address,unsigned,uint64_t)override{++writes;last=address;return true;}
 bool write_masked(uint64_t address,unsigned,uint64_t,uint8_t)override{++writes;last=address;return true;}
};
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(0x4000);cpu.tlb.write(0,0x4000,0,(4u<<6)|23,(5u<<6)|23);cpu.tlb.write(1,0x6000,0,(6u<<6)|23,(7u<<6)|23);
 const auto initial=cpu.state;uint64_t checks=0;
 const unsigned ops[]={26,27,39,44,45,52,55,60,63};
 auto run=[&](unsigned op,unsigned imm,uint64_t base,unsigned status,bool delay){cpu.state=initial;cpu.state.gpr[4]=base;cpu.state.gpr[5]=0x1122334455667788ull;cpu.state.cp0[8]=0x1357;cpu.state.cp0[12]=status;cpu.state.llbit=true;cpu.state.delay_slot=delay;bus.instruction=op<<26|4u<<21|5u<<16|imm;bus.reads=bus.writes=0;cpu.step();};
 auto pair=[&](unsigned op,unsigned imm,uint64_t high,unsigned status,bool delay,uint32_t target=0x6000){
  const int offset=imm<32768?int(imm):int(imm)-65536;const uint32_t low=uint32_t(target+(imm&7)-offset);
  run(op,imm,low,status,delay);const auto want=cpu.state;const bool exception=cpu.exception_taken;const auto address=cpu.access_address;const auto reads=bus.reads,writes=bus.writes;const auto last=bus.last;
  run(op,imm,high|low,status,delay);require(cpu.exception_taken==exception && cpu.access_address==address,"Kernel32 ignored upper VA bits incorrectly");
  auto got=cpu.state;got.gpr[4]=want.gpr[4];require(got.gpr==want.gpr && got.cp0==want.cp0 && got.pc==want.pc && got.next_pc==want.next_pc && got.delay_slot==want.delay_slot && got.llbit==want.llbit,"Kernel32 operands/fault metadata differ");
  require(bus.reads==reads && bus.writes==writes && bus.last==last,"Kernel32 translated bus access differs");++checks;
 };
 for(unsigned op:ops)for(unsigned imm=0;imm<65536;++imm)for(uint64_t high:{0x1234567800000000ull,0xfedcba9800000000ull,0xffffffff00000000ull,0x8000000000000000ull})for(bool delay:{false,true})pair(op,imm,high,0,delay);
 // Low-half carries, sign boundaries and all compatibility segment boundaries.
 for(unsigned op:ops)for(uint32_t target:{0u,0x7ffffff8u,0x80000000u,0x9ffffff8u,0xa0000000u,0xbffffff8u,0xc0000000u,0xfffffff8u})for(unsigned imm:{0u,7u,32767u,32768u,65535u})for(uint64_t high:{0ull,0x1234567800000000ull,0xfedcba9800000000ull,0xffffffff00000000ull})for(bool delay:{false,true})pair(op,imm,high,0,delay,target);
 // Current effective privilege and its KX flag decide whether Kernel32 applies.
 for(unsigned privilege=0;privilege<3;++privilege)for(unsigned flags=0;flags<8;++flags)for(unsigned level:{0u,2u,4u,6u})for(unsigned op:ops)for(unsigned imm:{0u,7u,32767u,32768u,65535u})for(bool delay:{false,true}){
  const unsigned status=privilege*8+(flags<<5)+level;const bool kernel=level || !privilege;
  if(kernel && !(status&128)){pair(op,imm,0xfedcba9800000000ull,status,delay);continue;}
  run(op,imm,0xfedcba9800006000ull,status,delay);
  const bool forbidden=!kernel && !(status&(privilege==1?64:32));
  const bool store=op==44 || op==45 || op==60 || op==63;
  const unsigned code=forbidden?10:store?5:4;
  require(cpu.exception_taken && ((cpu.state.cp0[13]>>2)&31)==code,"extended/forbidden address fault priority");
  require(cpu.state.gpr[5]==0x1122334455667788ull && !bus.writes && bus.reads==1,"fault accessed data or changed result");
  if(!forbidden)require(cpu.state.cp0[8]==0xfedcba9800006000ull+n64::sign16(uint16_t(imm)),"extended BadVAddr lost high bits");
  ++checks;
 }
 // Wide modes keep valid noncanonical 40-bit addresses for actual TLB matching.
 cpu.tlb.write(2,0x100006000ull,0,(6u<<6)|23,(7u<<6)|23);
 for(unsigned status:{128u,72u,48u})for(unsigned op:ops){run(op,0,0x100006000ull,status,false);require(!cpu.exception_taken && cpu.access_address==0x100006000ull,"valid extended VA truncated");++checks;}
 // Ordinary word/FP addresses retain their high bits; CU1 enables FP accesses.
 for(unsigned status:{0u,8u,16u,128u,72u,48u})for(unsigned op:{32u,33u,34u,35u,36u,37u,38u,40u,41u,42u,43u,46u,49u,53u,57u,61u}){
  run(op,0,0xfedcba9800006000ull,status|0x24000000,false);
  const bool store=op==40 || op==41 || op==42 || op==43 || op==46 || op==57 || op==61;
  require(cpu.exception_taken && ((cpu.state.cp0[13]>>2)&31)==(store?5u:4u) && cpu.state.cp0[8]==0xfedcba9800006000ull,"ordinary/FP invalid address was truncated");++checks;
 }
 std::cout<<checks<<" Kernel32/extended privilege address observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
