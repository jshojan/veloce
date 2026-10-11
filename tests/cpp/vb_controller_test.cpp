#include "machine.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <iterator>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static uint32_t read(vb::Controller& c,unsigned at){uint32_t v=0;require(c.read(at,v),"register read");return v;}
int main(int argc,char** argv){try{
 require(argc==2,"original pad ROM required");uint64_t checks=0;
 constexpr unsigned inputs[]={0,1,4,5,6,7,8,9,10,11,21,22,23,24},wires[]={2,3,5,4,12,13,11,10,9,8,6,15,14,7};
 for(unsigned sample=0;sample<16384;++sample){uint32_t input=0,want=2;
  for(unsigned n=0;n<14;++n)if(sample&(1u<<n)){input|=1u<<inputs[n];want|=1u<<wires[n];}
  vb::Controller pad;pad.input(input);require(pad.write(0x28,0xb1),"latch");pad.input(~input);
  for(unsigned count=1;count<=16;++count){require(pad.write(0x28,0x91) && pad.write(0x28,0x81),"clock");
   if(count==16)require((read(pad,0x14)*256+read(pad,0x10))==want,"latched complete native wire result");
   else require(!read(pad,0x10) && !read(pad,0x14),"chosen partial-data retention policy");
   require(read(pad,0x28)==0xcd,"software control readback");
   auto state=pad.save();vb::Controller restored;require(restored.restore(state.data(),state.size()) && restored.save()==state,"every partial clock state replay");
   require(pad.write(0x28,0x81) && pad.save()==state,"repeated clock level does not advance");++checks;
  }
  require(pad.write(0x10,0) && pad.write(0x14,0) && (read(pad,0x14)*256+read(pad,0x10))==want,"read-only data preservation");
  const auto before=pad.save();require(!pad.write(0x28,4) && pad.save()==before,"unsupported hardware poll rejects atomically");
  pad.write(0x28,0xb1);pad.write(0x28,0xb1);for(unsigned n=0;n<16;++n){pad.write(0x28,0x91);pad.write(0x28,0x81);}
  uint32_t changed=2;for(unsigned n=0;n<14;++n)if(!(sample&(1u<<n)))changed|=1u<<wires[n];
  require((read(pad,0x14)*256+read(pad,0x10))==changed,"relatch consumes new input");checks+=3;
 }
 for(unsigned value=0;value<256;++value){vb::Controller pad;const auto before=pad.save();const bool accepted=pad.write(0x28,value);require(accepted==(!(value&4)||bool(value&1)),"HW request/abort decoding");if(!accepted)require(pad.save()==before,"failed control mutation");else require(read(pad,0x28)==((value&0xb1)|0x4c),"writable/reserved SCR fields");++checks;}
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};vb::Machine m;require(m.load(rom.data(),rom.size()),"native pad image load");m.controller.input(0x12345678);m.controller.write(0x28,0xb1);m.controller.write(0x28,0x91);m.controller.write(0x28,0x81);
 std::vector<uint8_t> state,replay;require(m.save(state),"partial controller machine state");const auto peripheral=m.controller.save();m.reset();require(m.restore(state) && m.controller.save()==peripheral && m.save(replay) && replay==state,"partial controller complete machine replay");
 for(unsigned field=0;field<5;++field){auto bad=state;bad[bad.size()-40+field*8]=1;require(!m.restore(bad) && m.save(replay) && replay==state,"invalid peripheral state rejects atomically");}
 auto legacy=state;legacy.resize(legacy.size()-40);legacy[7]='2';require(m.restore(legacy) && m.controller.save()==vb::Controller{}.save(),"old state initializes neutral pad");
 uint32_t output=99;require(!m.read(0x02000000,1,output) && output==99 && !m.write(0x02000000,1,0),"unimplemented NVC register rejection");
 require(!m.controller.restore(nullptr,40) && !m.controller.restore(peripheral.data(),39),"null/truncated peripheral states");
 std::cout<<checks<<" native VB button/partial-clock/control observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
