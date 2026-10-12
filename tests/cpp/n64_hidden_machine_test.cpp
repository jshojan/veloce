#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void verify(bool ok,unsigned line){if(!ok)throw std::runtime_error("hidden machine mismatch at line "+std::to_string(line));}
#define check(...) verify((__VA_ARGS__),__LINE__)
static uint64_t addr(unsigned a){return n64::sign32(0xa0000000u+a);}
static unsigned metadata(uint64_t value,unsigned bytes){unsigned result=0;for(unsigned n=0;n<bytes;n+=2)result=(result<<2)|(((value>>(8*(bytes-n-2)))&1)?3:0);return result;}
int main(int argc,char** argv){try{
 check(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;check(m.load(rom.data(),rom.size()));uint64_t got=0,stores=0;
 for(unsigned raw=0;raw<65536;++raw)for(unsigned width:{2u,4u,8u}){
  const uint64_t value=uint64_t(raw)|(uint64_t(raw/2)<<16)|(uint64_t(raw/4)<<32)|(uint64_t(raw/8)<<48);
  check(m.write(addr(0x1000),width,value));unsigned expected=metadata(value,width);
  if(width==8)check(m.hidden.ebus_read(0x1000,8,got) && got==((uint64_t(expected>>4)<<32)|(expected&15)));
  else if(width==4)check(m.hidden.ebus_read(0x1000,4,got) && got==expected);
  else check(m.hidden.halfword(0x1000)==expected);
  ++stores;
 }
 // All enabled-byte masks: independently apply the largest aligned bus pieces.
 for(unsigned width:{4u,8u})for(unsigned mask=1;mask<(1u<<width);++mask)for(unsigned raw=0;raw<16;++raw)for(bool ebus:{false,true}){
  const uint64_t value=uint64_t(raw)|(uint64_t(raw/2)<<16)|(uint64_t(raw/4)<<32)|(uint64_t(raw/8)<<48);
  m.mi_mode=0;check(m.write(addr(0x1000),8,~0ull));m.mi_mode=ebus?256:0;
  std::array<unsigned,8> bits{1,1,1,1,1,1,1,1};
  for(unsigned n=0;n<width;){if(!(mask&(1u<<n))){++n;continue;}unsigned count=1;
   for(unsigned candidate:{2u,4u,8u})if(n%candidate==0 && n+candidate<=width && (mask>>n)% (1u<<candidate)==(1u<<candidate)-1)count=candidate;
   const uint64_t v=value>>(8*(width-n-count));
   if(!ebus){if(count==1)bits[n]=n%2?v%2:0;else for(unsigned i=0;i<count;i+=2)bits[n+i]=bits[n+i+1]=(v>>(8*(count-i-2)))%2;}
   else for(unsigned i=0;i<count;++i){const unsigned a=n+i;if(count==1)bits[a]=a%4==3?v%2:0;else if(count==2)bits[a]=n%4==2?(v>>(1-i))%2:0;else bits[a]=((count==8 && i<4?v>>32:v)>>(3-a%4))%2;}
   n+=count;
  }
  check(m.write_masked(addr(0x1000),width,value,uint8_t(mask)));
  for(unsigned i=0;i<8;i+=2)check(m.hidden.halfword(0x1000+i)==bits[i]*2+bits[i+1]);
  ++stores;
 }
 m.mi_mode=0;check(m.write(addr(0x1000),8,0x0001000000010000ull));
 check(m.cpu.caches.read(m,addr(0x1000),8,got,false,3) && got==0x0001000000010000ull);
 check(m.cpu.caches.read(m,n64::sign32(0x80001000),8,got,false,3));m.mi_mode=256;
 check(m.read(addr(0x1000),8,got) && got==0xc0000000cull);
 check(m.cpu.caches.read(m,n64::sign32(0x80001000),8,got,false,3) && got==0x0001000000010000ull);
 check(!m.cpu.caches.read(m,n64::sign32(0x80002000),8,got,false,3));m.mi_mode=0;
 // Settled SP, SI and PI destinations all replace ninth bits by halfword LSBs.
 check(m.write(addr(0x04000000),8,0x0001000000010000ull));
 check(m.write(addr(0x04040000),4,0) && m.write(addr(0x04040004),4,0x1000) && m.write(addr(0x0404000c),4,7));
 check(m.hidden.halfword(0x1000)==3 && m.hidden.halfword(0x1002)==0);
 m.pif.fill(0);m.pif[1]=1;m.pif[3]=1;
 check(m.write(addr(0x04800000),4,0x1000) && m.write(addr(0x04800004),4,0));check(m.hidden.halfword(0x1000)==3 && m.hidden.halfword(0x1002)==3 && m.hidden.halfword(0x1004)==0);
 check(m.write(addr(0x04600000),4,0x1000) && m.write(addr(0x04600004),4,0x10001000) && m.write(addr(0x0460000c),4,7));for(unsigned n=0;n<8;n+=2)check(m.hidden.halfword(0x1000+n)==(rom[0x1001+n]%2)*3);
 m.mi_mode=256;check(m.hidden.rdp_halfword(0x1000,2,true) && m.hidden.rdp_byte(0x1000,0,true));
 std::vector<uint8_t> saved,after,replay;check(m.save(saved) && saved.size()==n64::Machine::current_state_size && saved[6]=='3' && saved[7]=='2');
 check(m.write(addr(0x1000),8,0x50000000aull) && m.read(addr(0x1000),8,got) && got==0x50000000aull);m.hidden.finish_rdp_bytes();check(m.save(after));
 check(m.restore(saved) && m.write(addr(0x1000),8,0x50000000aull));m.hidden.finish_rdp_bytes();check(m.save(replay) && replay==after);
 for(unsigned fault=0;fault<4;++fault){auto bad=after;const size_t at=bad.size()-576;
  if(fault==0)bad[at]=1;
  if(fault==1)bad[bad.size()-561]=0;
  if(fault==2){for(unsigned n=0;n<8;++n)bad[at+8+n]=0;bad[bad.size()-561]=2;}
  if(fault==3)bad.pop_back();
  check(!m.restore(bad) && m.save(replay) && replay==after);
 }
 auto old=after;old.resize(old.size()-n64::Machine::hidden_state_size-n64::Machine::primitive_depth_state_size-n64::Machine::joybus_state_size-n64::PifJoybus::Eeprom{}.size());old[6]='2';old[7]='6';check(m.restore(old) && m.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8));
 for(unsigned n=0;n<8;n+=2)check(m.hidden.halfword(0x1000+n)==(m.ram[0x1001+n]%2)*3);
 check(m.hidden.snapshot().history==0 && m.hidden.snapshot().delayed==-1);
 std::cout<<stores<<" CPU/masked store captures; EBUS/cache distinction, PI/SI/SP DMA, S030 atomic replay/rejection and S026 import PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
