#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
static int64_t signed32(uint32_t x){return x<0x80000000u?int64_t(x):int64_t(x)-0x100000000ll;}
static uint32_t arithmetic(unsigned op,uint32_t a,uint32_t imm){const int64_t signed_imm=imm<32768?imm:int64_t(imm)-65536;
 switch(op){case 8:case 9:return uint32_t(uint64_t(a)+uint64_t(signed_imm));case 10:return signed32(a)<signed_imm;case 11:return uint64_t(a)<(signed_imm<0?uint64_t(signed_imm+0x100000000ll):uint64_t(signed_imm));case 12:return a&imm;case 13:return a|imm;case 14:return a^imm;default:return imm*65536;}}
static void opcode(n64::Machine& m,uint32_t ins,uint32_t pc=0){for(unsigned n=0;n<4;++n)m.sp[4096+((pc+n)&4095)]=uint8_t(ins>>(24-n*8));m.sp_interface.pc=pc;m.rsp.redirect(pc);m.sp_interface.status=0;}
static void step(n64::Machine& m){require(m.rsp.step(m.sp_interface,m.dp_interface,m.sp,m.ram),"scalar instruction blocked");}
int main(int argc,char** argv){try{
 require(argc==2,"ROM required");std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& r=m.rsp;auto& sp=m.sp_interface;
 const uint32_t patterns[]={0,1,0x7fffffff,0x80000000,0xffffffff,0x55555555,0xaaaaaaaa,0x12345678};uint64_t immediates=0,memory=0,branches=0,registers=0;
 for(unsigned op=8;op<16;++op)for(uint32_t value:patterns)for(unsigned imm=0;imm<65536;++imm){
  opcode(m,(op<<26)|((op==15?0:2)<<21)|(4<<16)|imm);r.gpr[2]=value;step(m);require(r.gpr[4]==arithmetic(op,value,imm),"all 16-bit immediate values and eight operand patterns");++immediates;
 }
 // Exact arithmetic with all register triples, including source/destination aliases and r0.
 for(unsigned fn:{32u,33u,34u,35u,36u,37u,38u,39u,42u,43u})for(unsigned rs=0;rs<32;++rs)for(unsigned rt=0;rt<32;++rt)for(unsigned rd=0;rd<32;++rd){
  opcode(m,(rs<<21)|(rt<<16)|(rd<<11)|fn);for(unsigned n=0;n<32;++n)r.gpr[n]=n?patterns[n%8]:0;
  const uint32_t a=r.gpr[rs],b=r.gpr[rt];uint32_t want=0;
  switch(fn){case 32:case 33:want=a+b;break;case 34:case 35:want=a-b;break;case 36:want=a&b;break;case 37:want=a|b;break;case 38:want=a^b;break;case 39:want=~(a|b);break;case 42:want=signed32(a)<signed32(b);break;default:want=a<b;break;}
  step(m);require(r.gpr[rd]==(rd?want:0) && !r.gpr[0],"all ALU register triples/aliases/zero");++registers;
 }
 for(unsigned fn:{0u,2u,3u,4u,6u,7u})for(uint32_t value:patterns)for(unsigned amount=0;amount<256;++amount){
  const unsigned shift=amount%32;opcode(m,((fn<4?0:2)<<21)|(3<<16)|(4<<11)|((fn<4?shift:0)<<6)|fn);r.gpr[2]=amount;r.gpr[3]=value;
  uint32_t want=0;if(fn==0 || fn==4)want=uint32_t(uint64_t(value)*(uint64_t(1)<<shift));else if(fn==2 || fn==6)want=value/(uint64_t(1)<<shift);else{const int64_t v=signed32(value),d=int64_t(1)<<shift;want=uint32_t(v>=0?v/d:-((-v+d-1)/d));}
  step(m);require(r.gpr[4]==want,"all low-byte variable amounts and all fixed shifts");
 }
 // Every byte address, pattern and scalar load/store opcode, including unaligned wraps.
 for(unsigned raw=0;raw<256;++raw){
  for(unsigned n=0;n<4096;++n)m.sp[n]=uint8_t(n*73+raw);
  for(unsigned address=0;address<4096;++address)for(unsigned op:{32u,33u,35u,36u,37u,39u,40u,41u,43u}){
   opcode(m,(op<<26)|(2<<21)|(4<<16));r.gpr[2]=0xfffff000u|address;r.gpr[4]=0x12345678u+raw*0x01010101u;
   const unsigned width=op==32 || op==36 || op==40?1:op==33 || op==37 || op==41?2:4;const bool store=op>=40;
   const uint32_t source=r.gpr[4];uint32_t want=0;for(unsigned n=0;n<width;++n)want=want*256+uint8_t(((address+n)%4096)*73+raw);
   if(op==32 && want>=128)want|=0xffffff00u;
   if(op==33 && want>=32768)want|=0xffff0000u;
   step(m);
   if(store){for(unsigned n=0;n<width;++n){require(m.sp[(address+n)%4096]==uint8_t(source/(uint64_t(1)<<(8*(width-1-n)))),"store byte/address/wrap");m.sp[(address+n)%4096]=uint8_t(((address+n)%4096)*73+raw);}require(r.gpr[4]==source,"store source preserved");}
   else require(r.gpr[4]==want,"load sign/address/wrap");
   require(m.sp[(address+4095)%4096]==uint8_t(((address+4095)%4096)*73+raw) && m.sp[(address+width)%4096]==uint8_t(((address+width)%4096)*73+raw),"neighbor guards");++memory;
  }
 }
 for(unsigned pc=0;pc<4096;pc+=4)for(unsigned kind=0;kind<8;++kind)for(uint32_t a:patterns)for(unsigned imm:{0u,1u,32768u,65535u}){
  const bool regimm=kind>=4;const unsigned op=regimm?1:kind+4,rt=regimm?(kind-4)%2+((kind>=6)?16:0):(kind<2?3:0);
  opcode(m,(op<<26)|(2<<21)|(rt<<16)|imm,pc);r.gpr[2]=a;r.gpr[3]=1;r.gpr[31]=0xdeadbeef;const bool taken=kind==0?a==1:kind==1?a!=1:kind==2?signed32(a)<=0:kind==3?signed32(a)>0:(kind%2==0?signed32(a)<0:signed32(a)>=0);
  const uint32_t target=uint32_t(int64_t(pc)+4+(imm<32768?int64_t(imm):int64_t(imm)-65536)*4)&4092;step(m);
  require(sp.pc==((pc+4)&4095) && r.next_pc==(taken?target:(pc+8)&4095) && r.delay_slot,"all PC slots/conditions/displacement endpoints");
  if(kind>=6)require(r.gpr[31]==((pc+8)&4095),"unconditional REGIMM link");
  for(unsigned n=0;n<4;++n)m.sp[4096+((sp.pc+n)&4095)]=uint8_t(13>>(24-n*8));
  step(m);require(sp.pc==(taken?target:(pc+8)&4095) && sp.status==3,"BREAK in taken/untaken delay slot");++branches;
 }
 for(unsigned pc=0;pc<4096;pc+=4)for(unsigned rd=0;rd<32;++rd){
  opcode(m,(2<<21)|(rd<<11)|9,pc);r.gpr[2]=0xfffff00b;step(m);require(r.next_pc==8 && (rd?r.gpr[rd]==((pc+8)&4095):!r.gpr[0]),"JALR low target bits, all links and alias capture");
 }
 for(unsigned pc=0;pc<4096;pc+=4)for(unsigned op:{2u,3u})for(unsigned target:{0u,1u,2u,3u,1023u,0x3fffffeu,0x3ffffffu}){
  opcode(m,(op<<26)|target,pc);r.gpr[31]=0x12345678;step(m);require(r.next_pc==((target%1024)*4) && r.gpr[31]==(op==3?(pc+8)%4096:0x12345678),"J/JAL every PC slot and truncated high target bits");
 }
 // Shared control/semaphore side effects and explicit unsupported-operation diagnostics.
 sp.reset();sp.active_sp=16;sp.active_dram=0x120;sp.length=0xff8;
 const uint32_t controls[]={16,0x120,0xff8,0xff8,0,0,0,0};
 for(unsigned reg=0;reg<8;++reg){opcode(m,0x40040000|(reg<<11));step(m);require(r.gpr[4]==controls[reg],"MFC0 bridges all eight shared SP fields");}
 sp.semaphore=0;opcode(m,0x40003800);step(m);require(!r.gpr[0] && sp.semaphore==1,"MFC0 r0 still acquires semaphore");
 opcode(m,0x40820000);r.gpr[2]=0;step(m);opcode(m,0x40820800);r.gpr[2]=0x700000;step(m);
 for(unsigned n=0;n<8;++n)m.ram[0x700000+n]=uint8_t(n*37+11);
 opcode(m,0x40821000);r.gpr[2]=7;step(m);for(unsigned n=0;n<8;++n)require(m.sp[n]==uint8_t(n*37+11),"MTC0 starts real shared DMA into DMEM");
 for(unsigned n=0;n<8;++n)m.sp[n]=uint8_t(n*17+91);
 opcode(m,0x40821800);r.gpr[2]=7;step(m);for(unsigned n=0;n<8;++n)require(m.ram[0x700000+n]==uint8_t(n*17+91),"MTC0 starts real shared DMA into RAM");
 opcode(m,0x40822000);r.gpr[2]=0x400;step(m);require(sp.status==128,"MTC0 status signal command");

 sp.reset();opcode(m,0x40043800);step(m);require(r.gpr[4]==0 && sp.semaphore==1,"MFC0 acquires semaphore");opcode(m,0x40843800);r.gpr[4]=123;step(m);require(!sp.semaphore,"MTC0 releases semaphore");
 for(uint32_t bad:{0x40048000u,0x4a000012u,0x70000000u}){opcode(m,bad);const auto before=r.gpr;const auto count=r.instructions;require(!r.step(sp,m.dp_interface,m.sp,m.ram) && r.gpr==before && sp.pc==0 && sp.status==1 && r.blocked && r.blocked_word==bad && r.instructions==count,"unsupported control/vector operation never fabricates BREAK or IRQ");}
 for(unsigned flags=0;flags<8;++flags){opcode(m,13);sp.status=(flags%2)*1+(flags/2%2)*32+(flags/4)*64;sp.interrupt=0;const bool halted=flags%2;r.step(sp,m.dp_interface,m.sp,m.ram);require(sp.pc==(halted?0u:4u) && (sp.status&3)==(halted?1u:3u) && sp.interrupt==(!halted && flags/4),"HALT/BREAK/IOB combinations");}
 opcode(m,0);sp.status=32;step(m);require(sp.pc==4 && sp.status==33,"single NOP step settles HALT");
 // CPU clock and MI see actual RSP BREAK, then CPU ack clears the shared level.
 m.reset();opcode(m,13);sp.status=64;m.mi_mask=1;m.rsp_phase=1;m.cpu.state.cp0[12]=0x401;require(m.write(n64::sign32(0xa0000400),4,0),"CPU NOP");m.cpu.step();require(m.cpu.exception_taken && sp.interrupt && (m.cpu.state.cp0[13]&0x400),"real RSP BREAK drives CPU MI interrupt");require(m.write(n64::sign32(0xa4040010),4,8) && !m.interrupt_pending(),"CPU acknowledges RSP interrupt");
 // Pending branch, fractional phase, blocked diagnostics and all registers survive state replay.
 m.reset();opcode(m,0x10000006);step(m);for(unsigned n=0;n<4;++n)m.sp[4096+4+n]=uint8_t(13>>(24-n*8));m.rsp_phase=2;
 std::vector<uint8_t> saved,after,replay;require(m.save(saved),"RSP save");m.clock();m.save(after);require(m.restore(saved),"RSP restore");m.clock();m.save(replay);require(after==replay,"pending RSP branch/BREAK exact replay");
 for(unsigned field=0;field<38;++field){if(field==33)continue;auto bad=saved;const uint64_t invalid=field==0?1:field<32 || field==36?0x100000000ull:field==32?1:field==37?3:2;const size_t at=bad.size()-5352+field*8;for(unsigned n=0;n<8;++n)bad[at+n]=uint8_t(invalid>>(56-n*8));require(m.restore(saved) && !m.restore(bad) && m.save(replay) && replay==saved,"invalid RSP state rejected atomically");}
 auto old=saved;old.resize(old.size()-5352);old[6]='1';old[7]='3';require(m.restore(old) && r.gpr==std::array<uint32_t,32>{} && !r.instructions && r.next_pc==((sp.pc+4)&4095) && !m.rsp_phase,"S013 neutral scalar/phase conversion preserves SP PC");
 m.reset();require(!r.instructions && !r.blocked && !m.rsp_phase && sp.status==1,"RSP reset");
 std::cout<<immediates<<" immediate, "<<registers<<" register triples, "<<memory<<" memory and "<<branches<<" branch/BREAK cases passed; shifts/JALR, controls, CPU IRQ, unsupported diagnostics, S014 replay/rejection and S013 conversion passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
