#include "machine.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
#include <stdexcept>
static void require(bool ok,const char* m) {if(!ok)throw std::runtime_error(m);}
constexpr uint64_t pc=0xffffffffa0400000ull,sentinel=0x12345678a5b6c7d8ull;
static void prepare(n64::Machine& m,uint32_t status=0x24000000) {
 m.cpu.state={};auto& s=m.cpu.state;s.pc=pc;s.next_pc=pc+4;s.cp0[12]=status;s.cp0[16]=2;
}
static uint32_t operation(unsigned fmt,unsigned fn,unsigned fs=2,unsigned fd=4) {return 0x44000000|(fmt<<21)|(fs<<11)|(fd<<6)|fn;}
static void execute(n64::Machine& m,uint32_t ins) {
 m.cpu.state.pc=pc;m.cpu.state.next_pc=pc+4;require(m.write(pc,4,ins),"instruction write");m.cpu.step();
}
int main(int argc,char** argv) {
 try {
 require(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
 n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& s=m.cpu.state;
 require(sizeof(float)==4 && sizeof(double)==8 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,"test reference needs binary IEEE host");
 uint64_t exact_cases=0;
 // Exact integers fit both binary precisions; native casts are independent and need no host rounding mode.
 for(unsigned raw=0;raw<65536;++raw)for(unsigned fn:{32u,33u})for(unsigned reg=0;reg<32;++reg) {
  prepare(m);const int value=raw<32768?int(raw):int(raw)-65536;s.fpr[reg]=uint32_t(value);s.fcr31=0x83f07c;
  execute(m,operation(20,fn,reg,reg));uint64_t want=0;
  if(fn==32) {const float reference=float(value);uint32_t bits=0;std::memcpy(&bits,&reference,4);want=bits;}
  else {const double reference=double(value);std::memcpy(&want,&reference,8);}
  require((fn==32?uint32_t(s.fpr[reg]):s.fpr[reg])==want && s.fcr31==0x80007c && !m.cpu.exception_taken,"all signed 16-bit integers and aliased destinations");++exact_cases;
 }
 struct Profile {unsigned fmt,fn;uint64_t input;};
 const Profile profiles[]={{20,32,1},{20,32,16777217},{17,32,0x0010000000000000},{17,32,0x7fefffffffffffff},{17,32,0x7ff0000000000000},{17,32,0x7ff8000000000000},{17,32,0x7ff0000000000001},{17,32,1},{17,36,0x3ff8000000000000},{17,36,0x41e0000000000000},{17,37,0x4340000000000000},{21,33,0x80000000000000},{21,33,0xff80000000000000},{21,33,0x7fffffffffffff}};
 uint64_t controls=0;
 for(unsigned profile=0;profile<14;++profile)for(unsigned rm=0;rm<4;++rm)for(unsigned enables=0;enables<32;++enables)for(unsigned oldflags=0;oldflags<32;++oldflags)for(unsigned flush=0;flush<2;++flush)for(unsigned cf=0;cf<2;++cf) {
  prepare(m);s.fpr[2]=profiles[profile].input;s.fpr[4]=sentinel;s.fcr31=rm+enables*128+oldflags*4+flush*0x1000000+cf*0x800000+0x3f000;
  uint64_t result=0;unsigned causes=0;bool narrow=profiles[profile].fn!=33 && profiles[profile].fn!=37;
  switch(profile) {
   case 0:result=0x3f800000;break;
   case 1:result=rm==2?0x4b800001:0x4b800000;causes=1;break;
   case 2:if(!flush || (enables&3))causes=32;else {result=rm==2?0x800000:0;causes=3;}break;
   case 3:result=rm==0 || rm==2?0x7f800000:0x7f7fffff;causes=5;break;
   case 4:result=0x7f800000;break;
   case 5:result=0x7fbfffff;causes=16;break;
   case 6:case 7:case 9:case 10:case 11:causes=32;break;
   case 8:result=rm==0 || rm==2?2:1;causes=1;break;
   case 12:result=0xc360000000000000;break;
   case 13:result=rm==0 || rm==2?0x4360000000000000:0x435fffffffffffff;causes=1;break;
  }
  const bool trap=bool((causes&32) || (causes&enables));const auto original=s.fcr31;
  execute(m,operation(profiles[profile].fmt,profiles[profile].fn));
  const uint32_t expected=(original&~0x3f000u)|(causes<<12)|(trap?0:causes*4);
  require(s.fcr31==expected && (s.cp0[13]&124)==(trap?60u:0u),"all rounding/enables/sticky/FS/condition combinations");
  require(s.fpr[4]==(trap?sentinel:narrow?(sentinel&0xffffffff00000000ull)|uint32_t(result):result) && s.fpr[2]==profiles[profile].input,"FPE suppresses destination, preserves source and flags");++controls;
 }
 // All register pairs, both modes, and all valid format/function pairs at exact one.
 for(unsigned fr=0;fr<2;++fr)for(unsigned fs=0;fs<32;++fs)for(unsigned fd=0;fd<32;++fd)for(unsigned fmt:{16u,17u,20u,21u})for(unsigned fn:{32u,33u,36u,37u,8u,9u,10u,11u,12u,13u,14u,15u}) {
  prepare(m,0x20000000|fr*0x4000000);const uint64_t input=fmt==16?0x3f800000:fmt==17?0x3ff0000000000000ull:1;
  s.fpr[fs]=input;s.fcr31=3;execute(m,operation(fmt,fn,fs,fd));
  const unsigned target=fn==32?16:fn==33?17:fn==36 || (fn>=12 && fn<=15)?20:21;
  const bool valid=target<=17?(fmt!=target):(fmt<=17);
  if(!fr && ((fs|fd)&1))require((s.cp0[13]&124)==40 && s.fcr31==3,"odd FR=0 RI convention");
  else if(!valid)require((s.cp0[13]&124)==60 && s.fcr31==0x20003,"invalid format assist");
  else require((target==16 || target==20?uint32_t(s.fpr[fd]):s.fpr[fd])==(target==16?0x3f800000:target==17?0x3ff0000000000000ull:1) && s.fcr31==3,"all register pairs/formats/exact conversion");
 }
 // Forced rounding must ignore FCSR rounding while preserving its control bits.
 for(unsigned fn=8;fn<=15;++fn)for(unsigned rm=0;rm<4;++rm)for(unsigned pattern=0;pattern<4;++pattern) {
  const uint32_t inputs[]={0x3fc00000,0x40200000,0xbfc00000,0xc0200000};const int answers[][4]={{2,1,2,1},{2,2,3,2},{-2,-1,-1,-2},{-2,-2,-2,-3}};
  prepare(m);s.fpr[2]=inputs[pattern];s.fcr31=rm;execute(m,operation(16,fn));
  const uint64_t wanted=uint64_t(int64_t(answers[pattern][fn%4]));require((fn>=12?uint32_t(s.fpr[4]):s.fpr[4])==(fn>=12?uint32_t(wanted):wanted) && s.fcr31==(rm|0x1004),"fixed ROUND/TRUNC/CEIL/FLOOR modes");
 }
 for(unsigned fmt=16;fmt<32;++fmt)for(unsigned fn:{32u,33u,36u,37u}) {
  prepare(m,0);s.fpr.fill(77);s.fcr31=0x180007c;execute(m,operation(fmt,fn));require((s.cp0[13]&0x3000007c)==0x1000002c && s.fcr31==0x180007c && s.fpr[4]==77,"CU1 precedence over formats");
 }
 for(unsigned mode=0;mode<3;++mode)for(unsigned wide=0;wide<2;++wide)for(unsigned fn:{8u,9u,10u,11u,37u}) {
  prepare(m,0x24000000|(mode<<3)|(wide?(mode==1?64:32):0));m.cpu.tlb.reset();require(m.cpu.tlb.write(0,0,0,(0x400000>>6)|23,(0x401000>>6)|23),"mode instruction map");
  require(m.write(pc,4,operation(16,fn)),"mode opcode");s.pc=0;s.next_pc=4;s.fpr[2]=0x3f800000;m.cpu.step();
  require((s.cp0[13]&124)==(mode && !wide?40u:0u),"long-target privilege policy");
 }
 for(bool bev:{false,true})for(bool nested:{false,true}) {
  prepare(m,0x24000080|(bev?0x400000:0)|(nested?2:0));s.fpr[2]=16777217;s.fpr[4]=sentinel;s.delay_slot=true;s.cp0[14]=123;s.cp0[13]=nested?0x80000000:0;s.fcr31=128;
  require(m.write(pc,4,operation(20,32)),"delay operation");m.cpu.step();require(s.cp0[14]==(nested?123:pc-4) && (s.cp0[13]&0x8000007c)==0x8000003c && s.fpr[4]==sentinel && s.fcr31==0x1080,"nested/delay FPE commit");
  require(s.pc==(bev?0xffffffffbfc00380ull:0xffffffff80000180ull),"FPE vectors");
 }
 prepare(m);s.fpr[2]=16777217;s.fpr[4]=sentinel;s.fcr31=128;require(m.write(pc,4,operation(20,32)),"pending conversion");
 std::vector<uint8_t> saved,after,replay;require(m.save(saved),"conversion save");m.cpu.step();m.save(after);require(m.restore(saved),"conversion restore");m.cpu.step();m.save(replay);require(after==replay,"pending conversion exact state replay");
 std::cout<<exact_cases<<" exact integer/index cases and "<<controls<<" exception-control combinations passed; all register pairs/formats, fixed modes, CU1/privilege/BD/BEV/state replay passed\n";return 0;
 }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
