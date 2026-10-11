#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{require(argc==2,"original Switch probe required");std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};nx::Machine machine;require(machine.load(rom.data(),rom.size()),"load original probe");uint64_t checks=0;
 for(unsigned low=0;low<65536;++low){machine.inputs={low+((65535-low)<<16),(65535-low)+(low<<16)};
  for(unsigned port=0;port<2;++port){uint64_t value=0x123456789abcdef0;const uint64_t address=port?0xffff0014:0xffff000c;
   require(machine.read(address,4,value) && value==machine.inputs[port],"synthetic input word/port independence");++checks;
   for(unsigned width:{1u,2u,8u}){value=0x123456789abcdef0;require(!machine.read(address,width,value) && value==0x123456789abcdef0,"input width rejection preserves read destination");++checks;}
   const auto before=machine.inputs;require(!machine.write(address,4,0) && machine.inputs==before,"input port is read-only");value=0;require(!machine.read(address,4,value,true),"input port is not executable");checks+=2;
  }
 }
 machine.reset();require(machine.inputs==std::array<uint32_t,2>{0,0},"input reset neutrality");machine.rom.clear();uint64_t value=99;require(!machine.read(0xffff000c,4,value) && value==99 && !machine.save_into(nullptr,0) && !machine.state_hash(),"unloaded synthetic input/state contracts");
 std::cout<<checks<<" synthetic Switch input/width/permission observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
