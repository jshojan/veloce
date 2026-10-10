#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
constexpr uint64_t pc=0xffffffffa0400000ull,sentinel=0x12345678a5b6c7d8ull;
static void prepare(n64::Machine& m,uint32_t status=0x24000000){m.cpu.state={};auto& s=m.cpu.state;s.pc=pc;s.next_pc=pc+4;s.cp0[12]=status;s.cp0[16]=2;}
static uint32_t operation(unsigned fmt,unsigned fn,unsigned fs=2,unsigned ft=6,unsigned fd=4){return 0x44000000|(fmt<<21)|((fn<4?ft:0)<<16)|(fs<<11)|(fd<<6)|fn;}
static void execute(n64::Machine& m,uint32_t ins){m.cpu.state.pc=pc;m.cpu.state.next_pc=pc+4;require(m.write(pc,4,ins),"instruction write");m.cpu.step();}
int main(int argc,char** argv){try{
 require(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& s=m.cpu.state;
 uint64_t registers=0,controls=0;
 for(unsigned fmt:{16u,17u})for(unsigned fr=0;fr<2;++fr)for(unsigned fn=0;fn<8;++fn)for(unsigned fs=0;fs<32;++fs)for(unsigned ft=0;ft<(fn<4?32u:1u);++ft)for(unsigned fd=0;fd<32;++fd){
  prepare(m,0x20000000|fr*0x4000000);const uint64_t one=fmt==16?0x3f800000:0x3ff0000000000000ull,two=fmt==16?0x40000000:0x4000000000000000ull,sign=fmt==16?0x80000000:0x8000000000000000ull;
  s.fpr.fill(one);s.fcr31=0x83f07c;execute(m,operation(fmt,fn,fs,ft,fd));
  if(!fr && ((fs|fd|(fn<4?ft:0))&1))require((s.cp0[13]&124)==40 && s.fcr31==0x83f07c && s.fpr[fd]==one,"undefined odd FR=0 convention");
  else require(s.fpr[fd]==(fn==0?two:fn==1?0:fn==7?one|sign:one) && !m.cpu.exception_taken && s.fcr31==(fn==6?0x83f07c:0x80007c),"all FPR triples, aliases, FR modes and S/D functions");
  ++registers;
 }
 struct Profile{unsigned fn;uint64_t a,b;};
 // Literal raw-bit golden results independent of the numeric implementation.
 for(unsigned fmt:{16u,17u})for(unsigned profile=0;profile<10;++profile)for(unsigned rm=0;rm<4;++rm)for(unsigned enables=0;enables<32;++enables)for(unsigned oldflags=0;oldflags<32;++oldflags)for(unsigned flush=0;flush<2;++flush)for(unsigned cf=0;cf<2;++cf){
  const bool single=fmt==16;const uint64_t one=single?0x3f800000:0x3ff0000000000000ull,two=single?0x40000000:0x4000000000000000ull,ten=single?0x41200000:0x4024000000000000ull,inf=single?0x7f800000:0x7ff0000000000000ull,min=single?0x800000:0x10000000000000ull,sign=single?0x80000000:0x8000000000000000ull,canon=single?0x7fbfffff:0x7ff7ffffffffffffull;
  const Profile p[]={{3,one,ten},{4,two,0},{3,one,0},{3,0,0},{2,inf,0},{0,1,one},{3,min,two},{2,inf-1,two},{7,one,0},{6,inf+1,0}};
  prepare(m);s.fpr[2]=p[profile].a;s.fpr[6]=p[profile].b;s.fpr[4]=sentinel;s.fcr31=rm+enables*128+oldflags*4+flush*0x1000000+cf*0x800000+0x3f000;
  uint64_t result=0;unsigned causes=0;
  switch(profile){
   case 0:result=single?(rm==0 || rm==2?0x3dcccccd:0x3dcccccc):(rm==0 || rm==2?0x3fb999999999999aull:0x3fb9999999999999ull);causes=1;break;
   case 1:result=single?(rm==2?0x3fb504f4:0x3fb504f3):(rm==0 || rm==2?0x3ff6a09e667f3bcdull:0x3ff6a09e667f3bccull);causes=1;break;
   case 2:result=inf;causes=8;break;
   case 3:case 4:result=canon;causes=16;break;
   case 5:causes=32;break;
   case 6:if(!flush || (enables&3))causes=32;else{causes=3;result=rm==2?min:0;}break;
   case 7:causes=5;result=rm==0 || rm==2?inf:inf-1;break;
   case 8:result=one|sign;break;
   case 9:result=inf+1;break;
  }
  const bool trap=profile!=9 && ((causes&32) || (causes&enables));const auto original=s.fcr31;execute(m,operation(fmt,p[profile].fn));
  const auto expected=profile==9?original:(original&~0x3f000u)|(causes<<12)|(trap?0:causes*4);
  require(s.fcr31==expected && (s.cp0[13]&124)==(trap?60u:0u),"all FCSR control/sticky/condition products; MOV preserves enabled old causes");
  require(s.fpr[4]==(trap?sentinel:single?(sentinel&0xffffffff00000000ull)|uint32_t(result):result),"trap suppresses destination and sticky flags");++controls;
 }
 // Exact halfway below minimum normal: ties-to-even can rescue the tiny result.
 for(unsigned fmt:{16u,17u})for(unsigned negative=0;negative<2;++negative)for(unsigned rm=0;rm<4;++rm)for(unsigned enables=0;enables<32;++enables)for(unsigned flush=0;flush<2;++flush){
  const uint64_t min=fmt==16?0x800000:0x10000000000000ull,one=fmt==16?0x3f800000:0x3ff0000000000000ull,sign=negative?(fmt==16?0x80000000:0x8000000000000000ull):0;
  const bool rescued=rm==0 || (rm==2 && !negative) || (rm==3 && negative);
  const unsigned causes=rescued?1:(!flush || (enables&3))?32:3;
  const bool trap=(causes&32) || (causes&enables);const uint64_t result=sign|(rescued?min:0);
  prepare(m);s.fpr[2]=min|sign;s.fpr[6]=one-1;s.fpr[4]=sentinel;s.fcr31=rm+enables*128+flush*0x1000000;
  const auto original=s.fcr31;execute(m,operation(fmt,2));
  require(s.fcr31==((original&~0x3f000u)|(causes<<12)|(trap?0:causes*4)) && m.cpu.exception_taken==trap,"minimum-normal rescue versus tiny assist/flush");
  require(s.fpr[4]==(trap?sentinel:fmt==16?(sentinel&0xffffffff00000000ull)|uint32_t(result):result),"rounded rescued tiny destination");
 }
 for(unsigned fmt=16;fmt<32;++fmt)for(unsigned fn=0;fn<8;++fn){
  prepare(m,0);s.fpr.fill(77);s.fcr31=0x180007c;execute(m,operation(fmt,fn));require((s.cp0[13]&0x3000007c)==0x1000002c && s.fcr31==0x180007c && s.fpr[4]==77,"CU1 precedes numeric decode");
  if(fmt>=18){prepare(m);s.fpr.fill(77);execute(m,operation(fmt,fn));require((s.cp0[13]&124)==60 && s.fcr31==0x20000 && s.fpr[4]==77,"reserved format assist");}
 }
 for(bool bev:{false,true})for(bool nested:{false,true}){
  prepare(m,0x24000080|(bev?0x400000:0)|(nested?2:0));s.fpr[2]=0x3f800000;s.fpr[6]=0;s.fpr[4]=sentinel;s.delay_slot=true;s.cp0[14]=123;s.cp0[13]=nested?0x80000000:0;s.fcr31=8*128;
  require(m.write(pc,4,operation(16,3)),"delay operation");m.cpu.step();require(s.cp0[14]==(nested?123:pc-4) && (s.cp0[13]&0x8000007c)==0x8000003c && s.fpr[4]==sentinel && s.fcr31==0x8400,"nested/delay FPE commit");require(s.pc==(bev?0xffffffffbfc00380ull:0xffffffff80000180ull),"FPE vectors");
 }
 prepare(m);s.fpr[2]=0x40000000;s.fpr[4]=sentinel;s.fcr31=128;require(m.write(pc,4,operation(16,4)),"pending sqrt");std::vector<uint8_t> saved,after,replay;require(m.save(saved),"save");m.cpu.step();m.save(after);require(m.restore(saved),"restore");m.cpu.step();m.save(replay);require(after==replay,"pending arithmetic exact replay");
 std::cout<<registers<<" register/mode/function cases and "<<controls<<" FCSR combinations passed; CU1, reserved formats, BD/BEV and state replay passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
