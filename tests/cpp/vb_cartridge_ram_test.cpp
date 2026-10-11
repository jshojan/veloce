#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 require(argc==2,"original cartridge required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};uint64_t checks=0;
 for(unsigned size=1024;size<=65536;size*=2){
  auto image=rom;for(unsigned n=0;n<4;++n)image[image.size()-496+n]=uint8_t(size>>(24-n*8));vb::Machine m;require(m.load(image.data(),image.size()) && m.cartridge_ram.size()==size,"declared capacity");
  for(uint32_t base:{0x06000000u,0x06010000u,0x06ff0000u,0x86000000u})for(unsigned at=0;at<size;++at)for(unsigned value=0;value<256;++value){
   uint32_t got=999;require(m.write(base+at,1,value) && m.read(0x06000000+at,1,got) && got==value,"all SRAM bytes/values/mirrors");++checks;
  }
  for(unsigned width:{2u,4u})for(unsigned raw=0;raw<65536;++raw)for(unsigned at:{0u,size-1,size-2,size-4}){
   const uint32_t value=raw*65537u;require(m.write(0x06000000+at,width,value),"wide SRAM write");uint32_t got=0;require(m.read(0x86000000+at,width,got) && got==(width==2?value&65535:value),"wide SRAM endian/wrap");
   for(unsigned n=0;n<width;++n)require(m.cartridge_ram[(at+n)%size]==uint8_t(value>>(n*8)),"byte ownership");++checks;
  }
  const auto persistent=m.cartridge_ram;m.reset();require(m.cartridge_ram==persistent,"reset must retain battery data");std::vector<uint8_t> saved,replay;require(m.save(saved) && saved.size()==66216+size && saved[7]=='4',"persistent complete state size");m.cartridge_ram.assign(size,0);require(m.restore(saved) && m.cartridge_ram==persistent && m.save(replay) && replay==saved,"whole SRAM state replay");
  for(unsigned mutation=0;mutation<3;++mutation){auto bad=saved;if(mutation==0)bad.pop_back();if(mutation==1)bad[66208]=1;if(mutation==2)bad.push_back(0);require(!m.restore(bad) && m.save(replay) && replay==saved,"bad state atomicity");}
  auto old=saved;old.resize(66208);old[7]='3';require(m.restore(old) && m.cartridge_ram==std::vector<uint8_t>(size,0),"legacy state neutral SRAM");
  m.restore(saved);auto invalid=image;invalid[invalid.size()-495]^=0x80;require(!m.load(invalid.data(),invalid.size()) && m.save(replay) && replay==saved,"invalid capacity atomicity");
  uint32_t guard=99;require(!m.read(0x06ffffff,2,guard) && guard==99 && !m.write(0x06ffffff,2,0),"board region crossing rejection");require(m.save(replay) && replay==saved,"rejected crossing mutation");
 }
 std::cout<<checks<<" original cartridge RAM bus observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
