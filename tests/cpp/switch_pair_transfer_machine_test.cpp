#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static uint32_t word(const std::vector<uint8_t>& b,unsigned at){return b[at]|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
static void patch(std::vector<uint8_t>& b,unsigned at,uint32_t value){for(unsigned n=0;n<4;++n)b[at+n]=value>>(8*n);}
int main(int argc,char** argv){try{
 require(argc==2,"original pair image required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> original{std::istreambuf_iterator<char>(file),{}};const unsigned text=word(original,16);
 nx::Machine machine;auto image=original;patch(image,text,0x910007ff);patch(image,text+4,0xa90007e0);
 require(machine.load(image.data(),image.size()) && machine.cpu.step() && !machine.cpu.step() && machine.cpu.state.fault==6 && machine.cpu.state.sp==nx::Machine::ram_size+1,"SP diagnostic from actual original instructions");
 std::vector<uint8_t> saved,after;require(machine.save(saved) && saved.size()==nx::Machine::state_size,"complete fault state save");machine.reset();require(machine.restore(saved) && machine.cpu.state.fault==6 && machine.save(after) && saved==after,"new diagnostic round trip");
 const auto steps=machine.cpu.state.steps;require(!machine.cpu.step() && steps==machine.cpu.state.steps,"restored terminal diagnostic is stable");auto bad=saved;bad[343]=7;require(!machine.restore(bad) && machine.save(after) && saved==after,"invalid diagnostic state rejection is atomic");
 for(bool load:{false,true}){
  image=original;patch(image,text,load?0x29c00500:0x29800500);require(machine.load(image.data(),image.size()),"pair fault image load");
  const unsigned end=word(image,56)+word(image,60)+word(image,52); // data address + file extent + BSS.
  machine.cpu.state.x[8]=end-4;machine.cpu.state.x[0]=0x12345678;machine.cpu.state.x[1]=0x89abcdef;const auto before=machine.cpu.state.x;
  uint64_t first=0;require(!machine.cpu.step() && machine.cpu.state.fault==(load?4u:5u) && machine.cpu.state.fault_address==end && machine.cpu.state.x==before && machine.cpu.state.pc==0,"second mapped access fault preserves registers/base/PC");
  require(machine.read(end-4,4,first) && first==(load?0u:0x12345678u),"first store is committed before second fault; loads do not mutate RAM");
  require(machine.save(saved) && machine.restore(saved) && machine.save(after) && saved==after,"partial store/fault complete state round trip");
 }
 std::cout<<"SP diagnostic and partial pair access fault complete-state replay PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
