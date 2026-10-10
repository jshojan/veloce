#include "machine.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* m) {if(!ok)throw std::runtime_error(m);}
static constexpr uint64_t pc=0xffffffffa0400000ull,ram=0xffffffffa0600000ull;
static void prepare(n64::Machine& m,uint32_t status) {
 m.cpu.state={};auto& s=m.cpu.state;s.pc=pc;s.next_pc=pc+4;s.cp0[12]=status;s.cp0[16]=2;s.gpr[1]=ram;
}
static void execute(n64::Machine& m,uint32_t ins) {
 auto& s=m.cpu.state;s.pc=pc;s.next_pc=pc+4;s.delay_slot=false;
 require(m.write(pc,4,ins),"instruction write");m.cpu.step();
}
static uint32_t move(unsigned op,unsigned g,unsigned f) {return 0x44000000|(op<<21)|(g<<16)|(f<<11);}
int main(int argc,char** argv) {
 try {
 require(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
 n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& s=m.cpu.state;uint64_t cases=0;
 for(unsigned fr=0;fr<2;++fr)for(unsigned reg=0;reg<32;++reg)for(unsigned raw=0;raw<65536;++raw) {
  prepare(m,0x20000000|fr*0x4000000);s.fpr.fill(0x89abcdef01234567ull);
  const uint32_t value=raw*65537u;const unsigned index=fr?reg:reg&~1u,shift=!fr && (reg&1)?32:0;
  s.gpr[2]=0xfedcba9800000000ull|value;execute(m,move(4,2,reg));
  const uint64_t expected=(0x89abcdef01234567ull&~(uint64_t(0xffffffff)<<shift))|(uint64_t(value)<<shift);
  require(s.fpr[index]==expected && !m.cpu.exception_taken,"all word register lanes and ignored high GPR");
  for(unsigned other=0;other<32;++other)if(other!=index)require(s.fpr[other]==0x89abcdef01234567ull,"neighbor FPR changed");
  execute(m,move(0,3,reg));require(s.gpr[3]==n64::sign32(value),"all MFC1 signed payloads");
  execute(m,(57u<<26)|(1<<21)|(reg<<16));uint64_t observed=0;require(m.read(ram,4,observed) && observed==value,"all SWC1 words");
  const uint32_t inverse=~value;require(m.write(ram,4,inverse),"word source");execute(m,(49u<<26)|(1<<21)|(reg<<16));execute(m,move(0,3,reg));require(s.gpr[3]==n64::sign32(inverse),"all LWC1 words");
  if(fr || !(reg&1)) {
   const uint64_t wide=(uint64_t(value)<<32)|inverse;s.gpr[2]=wide;execute(m,move(5,2,reg));execute(m,move(1,3,reg));require(s.gpr[3]==wide,"all doubleword moves");
   execute(m,(61u<<26)|(1<<21)|(reg<<16));require(m.read(ram,8,observed) && observed==wide,"all SDC1 payloads");
   require(m.write(ram,8,~wide),"doubleword source");execute(m,(53u<<26)|(1<<21)|(reg<<16));execute(m,move(1,3,reg));require(s.gpr[3]==~wide,"all LDC1 payloads");
  }
  ++cases;
 }
 // Every integer register, including the architecturally hardwired zero register.
 for(unsigned fr=0;fr<2;++fr)for(unsigned g=0;g<32;++g)for(unsigned kind:{0u,1u,4u,5u}) {
  prepare(m,0x20000000|fr*0x4000000);s.fpr[2]=0x1234567889abcdefull;s.gpr[g]=g?0xfedcba9876543210ull:0;
  execute(m,move(kind,g,2));
  if(kind==0 || kind==1)require(s.gpr[g]==(g?(kind==0?n64::sign32(0x89abcdef):0x1234567889abcdefull):0),"all GPR destinations/zero");
  else if(kind==5)require(s.fpr[2]==(g?0xfedcba9876543210ull:0),"all GPR wide sources/zero");
  else require(uint32_t(s.fpr[2])==(g?0x76543210u:0u),"all GPR word sources/zero");
 }
 // Transfers are bit operations: FCR31 exception masks/NaN payloads are not interpreted.
 for(unsigned bit=0;bit<64;++bit)for(unsigned reg=0;reg<32;++reg) {
  prepare(m,0x24000000);s.fcr31=0x183ffff;s.gpr[2]=uint64_t(1)<<bit;
  execute(m,move(5,2,reg));execute(m,move(1,3,reg));
  require(s.gpr[3]==s.gpr[2] && s.fcr31==0x183ffff && !m.cpu.exception_taken,"all data bit positions/FCR31 preservation");
 }
 for(unsigned bits=1;bits<2048;++bits)for(unsigned kind:{0u,1u,4u,5u}) {
  prepare(m,0x24000000);s.fpr.fill(77);s.gpr[2]=123;execute(m,move(kind,2,0)|bits);
  require((s.cp0[13]&124)==40 && s.fpr[0]==77 && s.gpr[2]==123,"reserved transfer encodings atomicity");
 }
 // Every signed immediate with actual address arithmetic, both widths and directions.
 for(unsigned raw=0;raw<65536;++raw)for(unsigned op:{49u,53u,57u,61u}) {
  prepare(m,0x24000000);const int offset=raw<32768?int(raw):int(raw)-65536;const unsigned bytes=(op==49||op==57)?4:8;
  s.gpr[1]=ram-uint64_t(offset);s.fpr[2]=0xa5a5f00d12345678ull;
  require(m.write(ram,8,0x9876543212345678ull),"offset source");execute(m,(op<<26)|(1<<21)|(2<<16)|raw);
  require(!m.cpu.exception_taken && m.cpu.access_address==ram && m.cpu.access_bytes==bytes,"all signed immediates/base sums");
 }
 // CU1 takes priority over register/width/address decoding, without destination or bus mutation.
 for(unsigned op:{49u,53u,57u,61u})for(unsigned reg=0;reg<32;++reg)for(unsigned lane=0;lane<8;++lane) {
  prepare(m,0);s.gpr[1]=ram+lane;s.fpr.fill(77);auto before=s.fpr;uint64_t old=0;m.read(ram,8,old);
  execute(m,(op<<26)|(1<<21)|(reg<<16));uint64_t after=0;m.read(ram,8,after);
  require((s.cp0[13]&0x3000007c)==0x1000002c && before==s.fpr && old==after && m.cpu.access_address==pc,"CU1 memory precedence");
 }
 for(unsigned kind:{0u,1u,4u,5u})for(unsigned reg=0;reg<32;++reg) {
  prepare(m,0);s.fpr.fill(77);s.gpr[2]=123;execute(m,move(kind,2,reg));require((s.cp0[13]&0x3000007c)==0x1000002c && s.gpr[2]==123 && s.fpr[0]==77,"CU1 move precedence");
 }
 for(unsigned mode=0;mode<3;++mode)for(unsigned wide=0;wide<2;++wide)for(unsigned kind:{1u,5u}) {
  // Code remains in KSEG only for kernel; user/supervisor execute through a mapped low page.
  prepare(m,0x24000000|(mode<<3)|(wide?(mode==1?64:32):0));
  m.cpu.tlb.reset();require(m.cpu.tlb.write(0,0,0,(0x400000>>6)|23,(0x401000>>6)|23),"privilege code mapping");
  require(m.write(pc,4,move(kind,2,0)),"mapped instruction");s.pc=0;s.next_pc=4;s.gpr[2]=123;s.fpr[0]=456;m.cpu.step();
  const bool legal=mode==0 || wide;
  require((s.cp0[13]&124)==(legal?0u:40u),"DMFC1/DMTC1 32/64-bit mode legality");
 }
 for(unsigned op:{49u,53u,57u,61u})for(unsigned lane=1;lane<8;++lane) {
  const unsigned bytes=(op==49||op==57)?4:8;if(!(lane&(bytes-1)))continue;
  prepare(m,0x24000000);s.gpr[1]=ram+lane;s.fpr[2]=123;execute(m,(op<<26)|(1<<21)|(2<<16));
  require((s.cp0[13]&124)==(op>=57?20u:16u) && s.cp0[8]==ram+lane && s.fpr[2]==123,"alignment fault atomicity");
 }
 for(unsigned op:{49u,53u,57u,61u})for(unsigned fault=0;fault<4;++fault) {
  prepare(m,0x24000000);m.cpu.tlb.reset();s.gpr[1]=fault==3?0xffffffffa5000000ull:0x20000000;s.fpr[2]=123;
  if(fault==1 || fault==2)require(m.cpu.tlb.write(0,0x20000000,0,(0x600000>>6)|(fault==1?0:2),(0x601000>>6)|7),"fault mapping");
  require(m.write(ram,8,0x8877665544332211ull),"fault backing sentinel");
  execute(m,(op<<26)|(1<<21)|(2<<16));const unsigned want=fault==3?7:fault==2 && op>=57?1:op>=57?3:2;
  if(fault==2 && op<57)require(!m.cpu.exception_taken,"valid clean page load");
  else {require((s.cp0[13]&124)==want*4 && s.fpr[2]==123,"TLB/refill/invalid/modified/DBE atomic load");
   uint64_t backing=0;require(m.read(ram,8,backing) && backing==0x8877665544332211ull,"faulted store changed backing");}

 }
 for(unsigned reg=1;reg<32;reg+=2)for(unsigned kind:{1u,5u}) {prepare(m,0x20000000);s.fpr.fill(77);execute(m,move(kind,2,reg));require((s.cp0[13]&124)==40 && s.fpr[0]==77,"undefined odd-double RI convention");}
 for(unsigned reg=1;reg<32;reg+=2)for(unsigned op:{53u,61u}) {
  prepare(m,0x20000000);s.fpr.fill(77);execute(m,(op<<26)|(1<<21)|(reg<<16));
  require((s.cp0[13]&124)==40 && s.fpr[0]==77,"undefined odd-double memory RI convention");
 }
 for(bool unusable:{false,true})for(bool store:{false,true})for(bool bev:{false,true})for(bool nested:{false,true}) {
  prepare(m,(unusable?0u:0x24000000u)|(bev?0x400000u:0u)|(nested?2u:0u));s.gpr[1]=ram+1;s.fpr[2]=123;
  s.delay_slot=true;s.cp0[14]=0x1234;s.cp0[13]=nested?0x80000000:0;
  require(m.write(pc,4,((store?61u:53u)<<26)|(1<<21)|(2<<16)),"delay-slot operation");m.cpu.step();
  require((s.cp0[13]&124)==(unusable?44u:store?20u:16u) && s.fpr[2]==123,"delay-slot CU1/alignment priority");
  require(s.cp0[14]==(nested?0x1234:pc-4) && (s.cp0[13]&0x80000000),"delay/nested EPC/BD");
  require(s.pc==(bev?0xffffffffbfc00380ull:0xffffffff80000180ull),"BEV exception vector");
 }
 prepare(m,0x24000000);s.fpr.fill(0x123456789abcdef0ull);s.fpr[31]=~uint64_t(0);s.gpr[2]=0xdeadbeef;
 require(m.write(pc,4,move(4,2,31)),"pending move");std::vector<uint8_t> saved,after,replay;require(m.save(saved),"FPR save");m.cpu.step();m.save(after);
 require(m.restore(saved),"FPR restore");m.cpu.step();m.save(replay);require(after==replay,"FPR exact continuation");
 auto old=saved;old.resize(old.size()-1344);old[7]='2';require(m.restore(old) && std::all_of(s.fpr.begin(),s.fpr.end(),[](uint64_t v){return v==0;}),"S012 neutral FPR conversion");
 require(m.restore(saved),"FPR reset fixture");m.reset();require(std::all_of(s.fpr.begin(),s.fpr.end(),[](uint64_t v){return v==0;}),"FPR reset");
 std::cout<<cases<<" word-register/payload cases; 3145728 doubleword payload cases, 262144 immediate cases, CU1/privilege/alignment/TLB/bus faults and FPR state replay passed\n";return 0;
 }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
