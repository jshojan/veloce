#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
 require(argc==2,"original ROM required");std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
 n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");
 // All ports query the ordinary native controller ID/status packet.
 m.pif.fill(0);for(unsigned port=0;port<4;++port){const unsigned at=port*6;m.pif[at]=1;m.pif[at+1]=3;m.pif[at+2]=0;}
 m.pif[24]=0xfe;m.pif[63]=1;m.process_pif();require(m.write(0xffffffffa4800004ull,4,0),"SI poll");
 for(unsigned port=0;port<4;++port){const unsigned at=port*6;require(m.pif[at+1]==3 && m.pif[at+3]==5 && m.pif[at+4]==0,"native controller query routing");}
 // Every 14-button combination, independently scattered into frontend bits,
 // on each active port. Other ports remain neutral and must not alias it.
 const unsigned virtual_bits[]={0,1,20,6,8,9,10,11,4,5,21,22,23,24};
 const unsigned native_bits[]={15,14,13,12,11,10,9,8,5,4,3,2,1,0};
 uint64_t cases=0;
 for(unsigned active=0;active<4;++active)for(unsigned raw=0;raw<16384;++raw){
  uint32_t frontend=0;uint16_t want=0;
  for(unsigned bit=0;bit<14;++bit)if(raw&(1u<<bit)){frontend+=1u<<virtual_bits[bit];want=uint16_t(want+(1u<<native_bits[bit]));}
  m.input_buttons=active==0?frontend:0;m.other_input_buttons.fill(0);if(active)m.other_input_buttons[active-1]=frontend;
  m.pif.fill(0);for(unsigned port=0;port<4;++port){const unsigned at=port*7;m.pif[at]=1;m.pif[at+1]=4;m.pif[at+2]=1;}
  m.pif[28]=0xfe;m.pif[63]=1;m.process_pif();require(m.write(0xffffffffa4800004ull,4,0),"SI poll");
  for(unsigned port=0;port<4;++port){const unsigned at=port*7;const unsigned got=unsigned(m.pif[at+3])*256+m.pif[at+4];require(m.pif[at+1]==4 && got==(port==active?want:0) && !m.pif[at+5] && !m.pif[at+6],"digital read port isolation");}
  require(!(m.pif[63]&1),"PIF control settles");++cases;
 }
 m.input_buttons=0x12345678;m.other_input_buttons={0x87654321,0x01234567,0xfedcba98};m.dp_interface.tmem.bytes.fill(0x5c);
 std::vector<uint8_t> state,after;require(m.save(state) && state.size()==n64::Machine::current_state_size,"exact current state extent");
 m.input_buttons=0;m.other_input_buttons.fill(0);require(m.restore(state) && m.save(after) && after==state,"all controller inputs replay");
 for(unsigned port=0;port<3;++port){auto bad=state;bad[bad.size()-1048896+port*8]=1;require(!m.restore(bad) && m.save(after) && after==state,"reserved controller state rejects atomically");}
 auto old=state;old.resize(old.size()-1048896);old[6]='2';old[7]='2';require(m.restore(old) && m.input_buttons==0x12345678 && m.other_input_buttons==std::array<uint32_t,3>{} && m.dp_interface.tmem.bytes[4095]==0x5c,"S022 neutral additional controllers preserves prefix");
 require(m.save(after) && std::equal(old.begin()+8,old.end(),after.begin()+8),"S022 preceding fields unchanged");
 for(bool extra:{false,true}){auto bad=state;if(extra)bad.push_back(0);else bad.pop_back();require(m.restore(state) && !m.restore(bad) && m.save(after) && after==state,"invalid controller state length atomicity");}
 m.reset();require(!m.input_buttons && m.other_input_buttons==std::array<uint32_t,3>{},"reset neutral inputs");
 std::cout<<cases<<" digital port combinations; query, isolation and S023 replay/legacy/invalid-state controls PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
