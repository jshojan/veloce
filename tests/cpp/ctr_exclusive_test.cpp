#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Bus:nds::Memory{
 uint32_t instruction=0,address=0,width=0,status=0;uint64_t value=0,replacement=0;unsigned calls=0;bool reject=false;
 bool read(uint32_t a,unsigned w,uint32_t& v)override{if(a!=0x1000 || w!=4)return false;v=instruction;return true;}
 bool write(uint32_t,unsigned,uint32_t)override{return false;}
 bool load_exclusive(uint32_t a,unsigned w,uint64_t& v)override{++calls;address=a;width=w;if(reject)return false;v=value;return true;}
 bool store_exclusive(uint32_t a,unsigned w,uint64_t v,uint32_t& s)override{++calls;address=a;width=w;replacement=v;if(reject)return false;s=status;return true;}
 bool clear_exclusive()override{++calls;return !reject;}
};
static uint32_t instruction(bool load,unsigned kind,unsigned rn,unsigned rd,unsigned rm,unsigned cond=14){return (cond<<28)|0x01800f90|(kind<<21)|(unsigned(load)<<20)|(rn<<16)|(rd<<12)|(load?15:rm);}
int main(int argc,char** argv){try{
 uint64_t checks=0;Bus bus;nds::Arm cpu(bus,true,true);
 for(bool load:{false,true})for(unsigned kind=0;kind<4;++kind)for(unsigned rn=0;rn<16;++rn)for(unsigned rd=0;rd<16;++rd)for(unsigned rm=0;rm<(load?1u:16u);++rm)for(unsigned flags=0;flags<16;++flags){
  const unsigned width=kind==0?4:kind==1?8:kind==2?1:2;cpu.reset(0x1000);auto& s=cpu.state;for(unsigned n=0;n<15;++n)s.r[n]=0x89abcdefu+n;s.r[rn]=0x2000;s.r[15]=0x1000;s.cpsr=0xdf|(flags<<28);auto before=s;bus.instruction=instruction(load,kind,rn,rd,rm);bus.value=0xfedcba9876543210;bus.status=flags&1;bus.calls=0;
  const bool invalid=rn==15 || rd==15 || (load && width==8 && ((rd&1)||rd>=14)) || (!load && (rm==15 || rd==rn || rd==rm || (width==8 && ((rm&1)||rm>=14 || rd==rm+1))));
  auto want=before.r;const uint64_t mask=width==8?~uint64_t(0):(uint64_t(1)<<(width*8))-1;bus.value&=mask;
  cpu.step();if(invalid)require(s.unsupported && s.r==before.r && !bus.calls,"invalid register fields");
  else{if(load){want[rd]=uint32_t(bus.value);if(width==8)want[rd+1]=uint32_t(bus.value>>32);}else{want[rd]=bus.status;require(bus.replacement==(width==8?(uint64_t(before.r[rm+1])<<32)|before.r[rm]:before.r[rm]),"captured source pair/aliases");}want[15]+=4;require(!s.unsupported && s.r==want && bus.calls==1 && bus.address==0x2000 && bus.width==width,"exclusive CPU registers/bus width");}
  require(s.cpsr==before.cpsr && s.steps==1,"exclusive flags/attempt count");++checks;
 }
 for(unsigned condition=0;condition<15;++condition)for(unsigned flags=0;flags<16;++flags){cpu.reset(0x1000);cpu.state.cpsr=0xdf|(flags<<28);cpu.state.r[1]=0x2000;bus.instruction=instruction(true,0,1,2,0,condition);bus.calls=0;cpu.step();require(bus.calls==unsigned(nds::Arm::condition(condition,0xdf|(flags<<28))),"exclusive condition gates transaction");++checks;}
 for(bool v5:{false,true}){nds::Arm old(bus,v5);old.reset(0x1000);old.state.r[1]=0x2000;bus.instruction=instruction(true,0,1,2,0);bus.calls=0;old.step();require(old.state.unsupported && !bus.calls,"pre ARMv6 profile rejection");}
 cpu.reset(0x1000);bus.instruction=0xf57ff01f;bus.calls=0;cpu.step();require(bus.calls==1 && cpu.state.r[15]==0x1004,"unconditional CLREX");
 require(argc==2,"original 3DS image required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};ctr::Machine m;require(m.load(rom.data(),rom.size()),"production machine fixture");
 constexpr uint32_t address=0x20001000;
 for(unsigned width:{1u,2u,4u,8u})for(unsigned raw=0;raw<65536;++raw){uint64_t old=0,status_value=uint64_t(raw)*0x0001000100010001ull;uint32_t status=99;
  require(m.load_exclusive(0,address,width,old) && m.store_exclusive(0,address,width,status_value,status) && status==0,"successful native exclusive pair");
  require(m.store_exclusive(0,address,width,0,status) && status==1,"reservation consumed after store");
  require(m.load_exclusive(0,address,width,old) && m.load_exclusive(1,address,width,old) && m.store_exclusive(1,address,width,status_value,status) && status==0 && m.store_exclusive(0,address,width,0,status) && status==1,"peer exclusive store invalidates owner");
  require(m.load_exclusive(0,address,width,old) && m.write(2,address,1,raw) && m.store_exclusive(0,address,width,0,status) && status==1,"ARM9 ordinary writer invalidates monitor");++checks;
 }
 uint64_t old=0;uint32_t status=99;require(m.load_exclusive(0,address,8,old),"pending monitor");auto before=m.exclusive.slots;
 require(!m.store_exclusive(0,0x28000000,8,0,status) && status==99 && m.exclusive.slots[0].address==before[0].address,"rejected bus leaves status/monitor");
 std::vector<uint8_t> saved,after,replay;require(m.save(saved),"pending exclusive state save");require(m.store_exclusive(0,address,8,0x123456789abcdef0,status) && !status && m.save(after),"post-exclusive save");require(m.restore(saved) && m.store_exclusive(0,address,8,0x123456789abcdef0,status) && !status && m.save(replay) && replay==after,"pending exclusive whole-state replay");
 auto bad=saved;bad[519]=3;require(!m.restore(bad) && m.save(replay) && replay==after,"bad reservation size atomicity");
 auto legacy=saved;legacy.erase(legacy.begin()+504,legacy.begin()+536);legacy[7]='1';require(m.restore(legacy) && !m.exclusive.slots[0].width && !m.exclusive.slots[1].width,"legacy state neutral exclusive monitors");
 std::cout<<checks<<" exclusive CPU/bus observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
