#include "vr4300.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:n64::Memory {
 uint32_t instruction=0;unsigned reads=0,writes=0;
 bool read(uint64_t address,unsigned bytes,uint64_t& value)override{++reads;if(bytes==4 && uint32_t(address)==0xa0004000){value=instruction;return true;}value=0x12345678;return true;}
 bool write(uint64_t,unsigned,uint64_t)override{++writes;return true;}
 bool write_masked(uint64_t,unsigned,uint64_t,uint8_t)override{++writes;return true;}
};
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(0x4000);cpu.tlb.write(0,0x4000,0,(4u<<6)|23,(5u<<6)|23);
 auto base=cpu.state;for(unsigned r=1;r<32;++r)base.gpr[r]=0xfedcba9876540000ull+r;
 base.hi=0x5555555555555555ull;base.lo=0xaaaaaaaaaaaaaaaaull;base.llbit=true;base.cp0[8]=0x2468;base.cp0[14]=0x1357;
 const unsigned functions[]={20,22,23,28,29,30,31,44,45,46,47,56,58,59,60,62,63};
 const unsigned majors[]={24,25,26,27,39,44,45,52,55,60,63};uint64_t count=0;
 auto blocked=[&](uint32_t instruction,uint32_t status,bool delay=false,bool bev=false){
  cpu.state=base;auto& s=cpu.state;s.cp0[12]=status|(bev?1u<<22:0);s.delay_slot=delay;bus.instruction=instruction;bus.reads=bus.writes=0;cpu.step();
  require(cpu.exception_taken && cpu.instruction_valid && ((s.cp0[13]>>2)&31)==10,"missing doubleword-mode RI");
  require(s.gpr==base.gpr && s.hi==base.hi && s.lo==base.lo && s.llbit && s.cp0[8]==base.cp0[8],"RI modified operands, LLbit or BadVAddr");
  require(bus.reads==1 && !bus.writes,"RI performed data bus access");
  require(s.cp0[14]==base.pc-(delay?4:0) && bool(s.cp0[13]&0x80000000)==delay && s.cp0[12]==(status|(bev?1u<<22:0)|2),"RI metadata");
  require(s.pc==n64::sign32(bev?0xbfc00380:0x80000180) && s.next_pc==s.pc+4 && !s.delay_slot,"RI vector");++count;
 };
 // Every SPECIAL operand payload for the 17 doubleword integer functions.
 for(unsigned status:{8u,16u})for(unsigned fn:functions)for(uint32_t payload=0;payload<(1u<<20);++payload)blocked((payload<<6)|fn,status);
 // Every immediate, and every base/target allocation at four offset patterns.
 for(unsigned status:{8u,16u})for(unsigned op:majors){
  for(unsigned imm=0;imm<65536;++imm)blocked(op<<26|4u<<21|5u<<16|imm,status);
  for(unsigned rs=0;rs<32;++rs)for(unsigned rt=0;rt<32;++rt)for(unsigned imm:{0u,32767u,32768u,65535u})blocked(op<<26|rs<<21|rt<<16|imm,status);
 }
 // All extension-flag combinations: the current privilege's SX/UX controls RI;
 // KX and the other privilege's flag cannot enable a 32-bit user/supervisor op.
 for(unsigned mode:{1u,2u})for(unsigned flags=0;flags<8;++flags)for(bool delay:{false,true})for(bool bev:{false,true}){
  const unsigned status=mode*8+(flags<<5);if(status&(mode==1?64:32))continue;
  for(unsigned fn:functions)blocked(4u<<21|5u<<16|6u<<11|fn,status,delay,bev);
  for(unsigned op:majors)blocked(op<<26|4u<<21|5u<<16|1,status,delay,bev);
 }
 // Kernel32/kernel64, extended User/Supervisor and EXL/ERL override controls.
 for(unsigned privilege=0;privilege<3;++privilege)for(unsigned flags=0;flags<8;++flags)for(unsigned level:{0u,2u,4u,6u}) {
  const unsigned status=privilege*8+(flags<<5)+level;
  if(!level && privilege && !(status&(privilege==1?64:32)))continue;
  for(unsigned fn:functions){cpu.state=base;cpu.state.cp0[12]=status;cpu.state.gpr[4]=0x6000;cpu.state.gpr[5]=2;bus.instruction=(fn>=56?0u:4u)<<21|5u<<16|((fn>=28 && fn<=31)?0u:6u)<<11|fn;cpu.step();require(!cpu.exception_taken,"allowed doubleword arithmetic rejected");++count;}
  for(unsigned op:majors){cpu.state=base;cpu.state.cp0[12]=status;cpu.state.gpr[4]=0x4000;cpu.state.gpr[5]=2;bus.instruction=op<<26|4u<<21|5u<<16;cpu.step();require(!cpu.exception_taken,"allowed doubleword memory/immediate rejected");++count;}
 }
 std::cout<<count<<" doubleword privilege/payload/metadata/no-data observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
