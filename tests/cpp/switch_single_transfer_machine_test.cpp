#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static uint32_t word(const std::vector<uint8_t>& b,unsigned at){return b[at]|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
static void patch(std::vector<uint8_t>& b,unsigned at,uint32_t value){for(unsigned n=0;n<4;++n)b[at+n]=value>>(8*n);}
int main(int argc,char** argv){try{
 require(argc==2,"original single image required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> original{std::istreambuf_iterator<char>(file),{}};const unsigned text=word(original,16);
 nx::Machine machine;auto image=original;patch(image,text,0x910007ff);patch(image,text+4,0x388003e0);
 require(machine.load(image.data(),image.size()) && machine.cpu.step() && !machine.cpu.step() && machine.cpu.state.fault==6,"signed single SP alignment diagnostic");
 std::vector<uint8_t> saved,after;require(machine.save(saved) && machine.restore(saved) && machine.save(after) && saved==after,"SP diagnostic full state replay");
 for(bool load:{false,true}){
  image=original;patch(image,text,load?0xb8800d00:0xb8000d00);require(machine.load(image.data(),image.size()),"single fault image load");
  const unsigned end=word(image,52)+word(image,56)+word(image,60);
  machine.cpu.state.x[8]=end-2;machine.cpu.state.x[0]=0x12345678;const auto before=machine.cpu.state.x;const auto ram=machine.ram;
  require(!machine.cpu.step() && machine.cpu.state.fault==(load?4u:5u) && machine.cpu.state.fault_address==end-2 && machine.cpu.state.x==before && machine.cpu.state.pc==0 && machine.ram==ram,"cross-boundary single access fault before destination/base/byte mutation");
  require(machine.save(saved) && machine.restore(saved) && machine.save(after) && saved==after,"single fault full state round trip");
 }
 // Unprivileged transfers use the same supplied process permissions, including RO protection.
 image=original;patch(image,text,0xb8000900);require(machine.load(image.data(),image.size()),"unprivileged store fixture");machine.cpu.state.x[8]=0;const auto ram=machine.ram;
 require(!machine.cpu.step() && machine.cpu.state.fault==5 && machine.ram==ram,"unprivileged store cannot write process text");
 std::cout<<"single SP, crossing-map and unprivileged permission faults/full-state replay PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
