#include "vr4300.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus final:n64::Memory{
 uint32_t instruction=0;unsigned reads=0,writes=0;bool fail=false;
 bool read(uint64_t address,unsigned bytes,uint64_t& value)override{++reads;if(uint32_t(address)==0xa0000400 && bytes==4){value=instruction;return true;}value=0x12345678;return !fail;}
 bool write(uint64_t,unsigned,uint64_t)override{++writes;return !fail;}
 bool write_masked(uint64_t,unsigned,uint64_t,uint8_t)override{++writes;return !fail;}
};
static uint32_t ins(unsigned op,unsigned base=1,unsigned target=5,unsigned immediate=0){return op<<26|base<<21|target<<16|immediate;}
int main(){try{
 Bus bus;n64::VR4300 cpu(bus);cpu.reset(n64::sign32(0xa0000400));cpu.state.cp0[12]=128;const auto initial=cpu.state;uint64_t count=0;
 auto check=[&](uint32_t physical,unsigned op){auto& s=cpu.state;s=initial;s.gpr[1]=0x9000000000000000ull|physical;s.gpr[5]=0xfeed1234;s.cp0[18]=(physical&~7u)|3;s.cp0[8]=77;bus.instruction=ins(op);bus.reads=bus.writes=0;cpu.step();require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==23,"missing physical Watch exception");require(s.gpr[5]==0xfeed1234 && s.cp0[8]==77 && bus.reads==1 && !bus.writes && s.cp0[14]==initial.pc,"Watch performed data access or changed operands");++count;};
 // Every 8-byte watch granule across the full 32-bit physical address space,
 // at both read and write senses. Match precedes unsupported physical buses.
 for(uint32_t tag=0;tag<(1u<<29);++tag){check(tag<<3,35);check(tag<<3,43);}
 // All byte lanes and R/W selections at all scalar widths, plus signed/merge/FP/linked forms.
 constexpr unsigned ops[]={32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,48,49,52,53,55,56,57,60,61,63};
 for(unsigned op:ops)for(unsigned lane=0;lane<8;++lane)for(unsigned flags=0;flags<4;++flags)for(bool match:{false,true})for(bool exl:{false,true}){
  auto& s=cpu.state;s=initial;s.cp0[12]|=0x24000000|(exl?2:0);s.cp0[16]=3;s.cp0[18]=0x8000|flags;s.cp0[8]=77;s.gpr[1]=n64::sign32((match?0x80008000:0x80008008)+lane);s.gpr[5]=0xfeed1234;s.llbit=true;bus.reads=bus.writes=0;bus.instruction=ins(op,1,4);cpu.step();
  const bool store=op==40 || op==41 || op==42 || op==43 || op==44 || op==45 || op==46 || op==56 || op==57 || op==60 || op==61 || op==63;
  const unsigned width=op==32 || op==36 || op==40?1:op==33 || op==37 || op==41?2:op==44 || op==45 || op==52 || op==53 || op==55 || op==60 || op==61 || op==63?8:4;
  const bool merge=op==34 || op==38 || op==42 || op==44 || op==45 || op==46;
  const bool aligned=merge || !(lane%(width));const bool trap=aligned && match && !exl && (flags&(store?1:2));
  require(cpu.exception_taken==(!aligned || trap),"lane/flags/EXL exception selection");
  if(trap)require(((s.cp0[13]>>2)&31)==23 && bus.reads==1 && !bus.writes && s.cp0[8]==77,"Watch bus/fault precedence");
  else if(!aligned)require(((s.cp0[13]>>2)&31)==(store?5u:4u),"alignment must precede Watch");++count;
 }
 for(unsigned bit=0;bit<32;++bit)for(bool wide:{false,true})for(unsigned reg:{18u,19u}){auto& s=cpu.state;s=initial;s.gpr[5]=uint64_t(1)<<bit;bus.instruction=(16u<<26)|((wide?5u:4u)<<21)|(5u<<16)|(reg<<11);cpu.step();const auto value=(uint64_t(1)<<bit)&(reg==18?0xfffffffbu:15u);require(!cpu.exception_taken && s.cp0[reg]==value,"Watch register reserved bits");++count;
  for(bool read_wide:{false,true}){s.pc=initial.pc;s.next_pc=s.pc+4;bus.instruction=(16u<<26)|((read_wide?1u:0u)<<21)|(6u<<16)|(reg<<11);cpu.step();const auto want=read_wide?value:value|((value&0x80000000)?0xffffffff00000000ull:0);require(!cpu.exception_taken && s.gpr[6]==want,"Watch MFC0/DMFC0 readback");++count;}
 }
 for(unsigned op:{35u,43u})for(bool delay:{false,true})for(bool bev:{false,true}){auto& s=cpu.state;s=initial;s.cp0[12]|=bev?1u<<22:0;s.cp0[18]=0x8003;s.gpr[1]=n64::sign32(0xa0008000);s.delay_slot=delay;s.cp0[8]=77;bus.instruction=ins(op);cpu.step();require(cpu.exception_taken && (s.cp0[13]&0x8000007c)==((delay?0x80000000u:0)|92) && s.cp0[14]==initial.pc-(delay?4:0) && s.pc==n64::sign32(bev?0xbfc00380:0x80000180) && s.cp0[8]==77,"Watch BD/EPC/BEV metadata");++count;}
 for(unsigned op:{35u,43u}){
  auto& s=cpu.state;s=initial;s.cp0[18]=0x20003;s.gpr[1]=0x20000;bus.instruction=ins(op);cpu.step();require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==(op==35?2u:3u) && s.cp0[8]==0x20000,"TLB miss before Watch");++count;
  s=initial;cpu.tlb.write(1,0x20000,0,(8u<<6)|23,(8u<<6)|23);s.cp0[18]=0x8003;s.gpr[1]=0x20000;bus.instruction=ins(op);bus.reads=bus.writes=0;cpu.step();require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==23 && bus.reads==1 && !bus.writes,"Watch compared VA instead of translated PA");cpu.tlb.reset();++count;
  for(bool enabled:{false,true}){s=initial;s.cp0[18]=0x8000|(enabled?3:0);s.gpr[1]=n64::sign32(0xa0008000);bus.instruction=ins(op);bus.fail=true;cpu.step();require(cpu.exception_taken && ((s.cp0[13]>>2)&31)==(enabled?23u:7u),"Watch before data bus error");bus.fail=false;++count;}
 }
 cpu.state=initial;cpu.state.cp0[18]=0x403;cpu.state.gpr[1]=n64::sign32(0x80008000);bus.instruction=ins(47,1,0);cpu.step();require(!cpu.exception_taken,"instruction fetch or CACHE triggered Watch");++count;
 std::cout<<count<<" physical granule, lane/flags/width/EXL and Watch register observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
