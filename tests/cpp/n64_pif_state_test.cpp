#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <cstdlib>
static void check(bool ok){if(!ok){std::cerr<<"PIF state observation mismatch\n";std::exit(1);}}
static uint64_t read(n64::Machine& m,uint32_t at,unsigned bytes=4){uint64_t value=0;check(m.read(n64::sign32(at),bytes,value));return value;}
static void write(n64::Machine& m,uint32_t at,uint64_t value,unsigned bytes=4){check(m.write(n64::sign32(at),bytes,value));}
int main(int argc,char** argv){check(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine m;check(m.load(rom.data(),rom.size()));
 write(m,0xa4800000,0x20000);write(m,0xa430000c,8);m.pif.fill(0xaa);m.pif[0]=255;m.pif[1]=1;m.pif[2]=4;m.pif[3]=1;m.pif[8]=254;
 m.input_buttons=64;write(m,0xbfc007ff,1,1);check(read(m,0xbfc007c4,4)==0xaaaaaaaa && m.joybus.snapshot()==n64::PifJoybus::State{1,64,64,64,64});
 write(m,0xa4800004,0);check(read(m,0xa0020000,8)==0xff01040110000000ull && read(m,0xa4800018)==4096 && read(m,0xa4300008)==2);
 std::vector<uint8_t> saved,after,replay;check(m.save(saved) && saved.size()==9527616 && saved[6]=='3' && saved[7]=='1');
 m.input_buttons=emu::n64_input::pack(1,-128,127);write(m,0xa4800004,0);check(read(m,0xa0020000,8)==0xff0104018000807full && m.save(after));
 check(m.restore(saved));m.input_buttons=emu::n64_input::pack(1,-128,127);write(m,0xa4800004,0);check(m.save(replay) && replay==after);
 // A flag-zero RAM write preserves the latched offsets and does not sample input.
 const auto layout=m.joybus.snapshot();m.ram[0x20000]=254; // A new parser would see end-of-packets.
 write(m,0xa4800010,0);check(m.joybus.snapshot()==layout);m.input_buttons=64;write(m,0xa4800004,0);check(read(m,0xa0020004)==0x10000000);
 check(m.restore(after));unsigned rejections=0;
 for(unsigned channel=0;channel<5;++channel)for(unsigned bit=7;bit<64;++bit){auto bad=after;bad[bad.size()-40+channel*8+7-bit/8]|=uint8_t(1u<<(bit%8));check(!m.restore(bad) && m.save(replay) && replay==after);++rejections;}
 for(const auto state:{n64::PifJoybus::State{62,64,64,64,64},n64::PifJoybus::State{1,1,64,64,64}}){auto bad=after;for(unsigned field=0;field<5;++field)for(unsigned n=0;n<8;++n)bad[bad.size()-40+field*8+n]=uint8_t(state[field]>>(56-n*8));check(!m.restore(bad) && m.save(replay) && replay==after);++rejections;}
 auto old=after;old.resize(old.size()-40);old[6]='2';old[7]='8';check(m.restore(old) && m.joybus.snapshot()==n64::PifJoybus::State{64,64,64,64,64} && m.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8));
 const auto packet=m.pif;m.input_buttons=64;write(m,0xa4800004,0);check(m.pif==packet);write(m,0xbfc007ff,1,1);write(m,0xa4800004,0);check(read(m,0xa0020004)==0x10000000);
 m.reset();check(m.joybus.snapshot()==n64::PifJoybus::State{64,64,64,64,64});std::cout<<"S030 cached SI input/RAM/IRQ replay, direct formatting, unformatted replacement, "<<rejections<<" atomic state rejections and neutral S028 import PASS\n";
}
