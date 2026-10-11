#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static void word(std::vector<uint8_t>& ram,unsigned at,uint64_t value){for(unsigned i=0;i<8;++i)ram[at+i]=uint8_t(value>>(56-i*8));}
int main(int argc,char** argv){try{
 require(argc==2,"original ROM fixture required");
 const unsigned native[64]={1,1,1,1,1,1,1,1,4,6,12,14,12,14,20,22,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
 uint64_t headers=0,streams=0,words=0;
 for(unsigned byte=0;byte<256;++byte){n64::RdpPacket packet;
  require(n64::RdpPacket::length(byte)==native[byte&63],"all opcode aliases/native lengths");
  for(unsigned i=0;i<native[byte&63];++i){const uint64_t value=i?(0x2900000000000000ULL|(uint64_t(i)*0x01010101010101ULL)):(uint64_t(byte)<<56)|0x123456789abcdeULL;
   packet.push(0x100+i*8,value);require(packet.count==i+1 && packet.needed==native[byte&63] && packet.words[i]==value && packet.ready()==(i+1==native[byte&63]),"word order/ready boundary");
   n64::RdpPacket replay;require(replay.restore(packet.snapshot()) && replay.snapshot()==packet.snapshot(),"partial packet standalone replay");++headers;
  }
 }
 uint64_t payloads=0;
 for(unsigned raw=0;raw<65536;++raw){n64::RdpPacket packet;
  for(unsigned i=0;i<22;++i){const uint64_t value=(i?0x2900000000000000ULL:0x0f00000000000000ULL)|(uint64_t(raw)*0x0001000100010001ULL&0x00ffffffffffffffULL);
   packet.push(0x100+i*8,value);require(packet.words[i]==value && packet.count==i+1,"all repeated-halfword coefficient retention");++payloads;
  }
  require(packet.ready(),"full payload packet ready");
 }
 for(unsigned op:{8u,9u,10u,11u,12u,13u,14u,15u,36u,37u})for(unsigned source=0;source<2;++source)for(unsigned address=0;address<4096;address+=8){
  n64::DpInterface dp;std::vector<uint8_t> ram(8192,0x5a);std::array<uint8_t,8192> sp{};bool irq=false;dp.flags=source;
  for(unsigned i=0;i<native[op];++i){const auto value=i?0x2900000000000000ULL|uint64_t(i)*0x01010101010101ULL:(uint64_t(op)<<56)|0xabcdef12345678ULL;word(ram,address+i*8,value);for(unsigned j=0;j<8;++j)sp[(address+i*8+j)&4095]=ram[address+i*8+j];}
  const auto memory=ram;dp.write(0,address);
  for(unsigned i=0;i<native[op];++i){const unsigned end=address+(i+1)*8;dp.write(1,end);dp.tick(ram,sp,irq);
   require(dp.current==end && !irq && ram==memory && dp.packet.count==i+1 && dp.packet.needed==native[op],"partial END fetch/current or coefficient FullSync escape");
   require(dp.blocked==(i+1==native[op]),"unsupported draw only diagnoses complete native packet");
   const auto saved=dp.packet.snapshot();dp.flags=source|2;dp.tick(ram,sp,irq);require(dp.packet.snapshot()==saved && !irq,"frozen partial packet");dp.flags=source|4;dp.tick(ram,sp,irq);require(dp.packet.snapshot()==saved,"flushed partial packet convention");dp.flags=source;
   ++words;
  }
  require(dp.blocked_word==dp.packet.words[0] && dp.blocked_reason==1,"complete unsupported packet diagnostic");++streams;
 }
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),{});n64::Machine m;require(m.load(rom.data(),rom.size()),"fixture load");
 auto& dp=m.dp_interface;word(m.ram,0x100,0xcf000ffa0ffaf006ULL);for(unsigned i=1;i<22;++i)word(m.ram,0x100+i*8,0x2900000000000000ULL|i);
 dp.write(0,0x100);dp.write(1,0x158);for(unsigned i=0;i<11;++i)dp.tick(m.ram,m.sp,m.dp_interrupt);require(dp.packet.count==11 && !dp.blocked,"halfway Mario-shaped packet");
 std::vector<uint8_t> saved,after,replay;require(m.save(saved) && saved.size()==n64::Machine::current_state_size && saved[7]=='5',"S025 size/version");
 dp.write(1,0x1b0);for(unsigned i=0;i<11;++i)dp.tick(m.ram,m.sp,m.dp_interrupt);require(m.save(after) && dp.blocked && dp.current==0x1b0 && !m.dp_interrupt,"complete retained triangle");
 require(m.restore(saved),"partial machine restore");dp.write(1,0x1b0);for(unsigned i=0;i<11;++i)dp.tick(m.ram,m.sp,m.dp_interrupt);require(m.save(replay) && replay==after,"exact partial FIFO replay");
 for(unsigned field:{0u,1u,2u,14u,24u}){auto bad=saved;const uint64_t invalid=field==0?1:field==1?23:field==2?21:1;const size_t at=bad.size()-208+field*8;for(unsigned i=0;i<8;++i)bad[at+i]=uint8_t(invalid>>(56-i*8));require(m.restore(saved) && !m.restore(bad) && m.save(replay) && replay==saved,"invalid FIFO state rejects atomically");}
 auto old=saved;old.resize(old.size()-208);old[7]='3';require(m.restore(old) && !dp.packet.count && m.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8),"S023 neutral FIFO conversion preserves previous bytes");
 dp.reset();word(m.ram,0x100,0x0f00000000000000ULL);dp.write(0,0x100);dp.write(1,0x108);dp.tick(m.ram,m.sp,m.dp_interrupt);const auto pending=dp.packet.snapshot();dp.write(1,0x900008);dp.current=0x900000;dp.tick(m.ram,m.sp,m.dp_interrupt);require(dp.blocked && dp.blocked_reason==2 && dp.current==0x900000 && dp.packet.snapshot()==pending,"bad fetch preserves previously buffered words");
 dp.reset();m.dp_interrupt=false;word(m.ram,0x100,0x0800000000000000ULL);
 dp.write(0,0x100);dp.write(1,0x108);dp.tick(m.ram,m.sp,m.dp_interrupt);
 for(unsigned i=0;i<3;++i){const auto value=0x2900000000000000ULL|i;for(unsigned n=0;n<8;++n)m.sp[0x500+i*8+n]=uint8_t(value>>(56-n*8));}
 dp.flags=1;dp.write(0,0x500);dp.write(1,0x518);for(unsigned i=0;i<3;++i)dp.tick(m.ram,m.sp,m.dp_interrupt);
 require(dp.packet.start==0x100 && dp.packet.count==4 && dp.current==0x518 && dp.blocked && !m.dp_interrupt && dp.packet.words[1]==0x2900000000000000ULL,"partial packet survives queued noncontiguous START and source switch");
 std::cout<<payloads<<" coefficient words, "<<headers<<" header/partial states, "<<streams<<" DRAM/XBUS streams / "<<words<<" word-boundary captures; payload FullSync isolation, freeze/flush, S025 replay/rejection/S023 conversion PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
