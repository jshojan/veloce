#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iomanip>
#include <iostream>
#include <algorithm>
using emulator_test::require;
int main(int argc,char** argv){try{
 require(argc==6,"core, ROM, output, cases and domain required");const unsigned cases=std::stoul(argv[4]);require(cases && cases<=16384,"case domain");
 std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};emulator_test::LoadedCore core(argv[1],rom);
 const auto* layout=core.plugin->get_controller_layout();require(layout && layout->button_count==14 && layout->controller_count==1,"native dual-D-pad layout required");
 std::ofstream output(argv[3]);output<<"VELOCE VB CONTROLLER 1 domain="<<argv[5]<<" cases="<<cases<<" words=8\n";
 constexpr unsigned positions[]={0,1,4,5,6,7,8,9,10,11,21,22,23,24};
 for(unsigned sample=0;sample<cases;++sample){
  uint32_t input=0;for(unsigned n=0;n<14;++n)if(sample&(1u<<n))input|=1u<<positions[n];
  core.plugin->reset();core.plugin->run_frame({input});require(core.plugin->get_frame_count()==1 && core.plugin->get_cycle_count()<1000,"guest did not reach bounded HALT");
  output<<"REG case="<<std::dec<<sample<<" r="<<std::hex<<std::setfill('0');
  for(unsigned address=0x1000;address<0x1020;address+=4){uint32_t word=0;for(unsigned n=0;n<4;++n)word|=uint32_t(core.plugin->read_memory(address+n))<<(8*n);output<<std::setw(8)<<word;}
  output<<'\n';
  if(sample==0 || sample==1 || sample==cases-1){std::vector<uint8_t> state,again;require(core.plugin->save_state(state) && core.plugin->load_state(state),"complete controller state roundtrip");core.plugin->run_frame({input});require(core.plugin->get_frame_count()==2,"HALT continuation frame count");require(core.plugin->save_state(again) && core.plugin->load_state(state),"HALT replay checkpoint");core.plugin->run_frame({input});std::vector<uint8_t> replay;require(core.plugin->save_state(replay) && replay==again,"HALT complete-state replay");}
 }
 output.close();require(output.good(),"register output persistence");std::cout<<cases<<" actual native VB controller snapshots captured\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
