#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void check(bool v){if(!v)throw std::runtime_error("primitive state mismatch");}
static void put(std::vector<uint8_t>& ram,unsigned at,uint64_t word){for(unsigned n=0;n<8;++n)ram[at+n]=uint8_t(word>>(56-n*8));}
int main(int argc,char** argv){try{
 check(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};n64::Machine machine;check(machine.load(rom.data(),rom.size()));auto& dp=machine.dp_interface;
 dp.primitive_depth.command(0x4567abcd);dp.depth_image=0x100000;check(machine.hidden.rdp_halfword(0x1000,2,true));
 std::vector<uint8_t> saved,after,replay;check(machine.save(saved) && saved.size()==9527616 && saved.size()==n64::Machine::current_state_size && saved[7]=='9');
 put(machine.ram,0x100,0xeea5c33cfedc9876);put(machine.ram,0x108,0xe900000000000000);dp.flags=2;check(dp.write(0,0x100) && dp.write(1,0x110) && machine.save(saved));
 dp.flags=0;unsigned clocks=0;while(!machine.dp_interrupt && clocks<10){machine.clock();++clocks;}
 check(machine.dp_interrupt && dp.primitive_depth.snapshot()==0x7edc9876 && machine.save(after));
 check(machine.restore(saved));dp.flags=0;for(unsigned n=0;n<clocks;++n)machine.clock();check(machine.save(replay) && replay==after);
 for(unsigned bit=31;bit<64;++bit){auto bad=after;bad[bad.size()-41-bit/8]|=uint8_t(1u<<(bit%8));check(!machine.restore(bad) && machine.save(replay) && replay==after);}
 auto old=after;old.resize(old.size()-48);old[7]='7';check(machine.restore(old) && dp.primitive_depth.snapshot()==0 && dp.depth_image==0x100000 && machine.hidden.halfword(0x1000)==2 && machine.save(replay));
 check(std::equal(old.begin()+8,old.end(),replay.begin()+8) && std::all_of(replay.end()-48,replay.end()-40,[](uint8_t b){return !b;}));
 machine.reset();check(dp.primitive_depth.snapshot()==0);
 std::cout<<"S029 primitive state capacity, FullSync replay, 33 reserved-bit rejections and byte-exact S027 neutral import/reset PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
