#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <algorithm>
using emulator_test::require;
static uint64_t hash(const std::vector<uint8_t>& data){uint64_t h=0xcbf29ce484222325ull;for(auto b:data)h=(h^b)*0x100000001b3ull;return h;}
static uint32_t word(const std::vector<uint8_t>& data,size_t at){uint32_t v=0;for(unsigned n=0;n<4;++n)v=v*256+data[at+n];return v;}
int main(int argc,char** argv){try{
 require(argc==3,"PowerPC core and original probe required");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
 emulator_test::LoadedCore core(argv[1],rom);auto& net=core.netplay();constexpr size_t size=25166340,si=360+24*1024*1024;
 require(net.is_deterministic() && net.get_max_players()==2 && net.get_max_state_size()==size,"two-player rollback capability");
 const auto* layout=core.plugin->get_controller_layout();require(layout && layout->controller_count==2 && layout->button_count==12,"two native digital controller slots exposed to frontend");
 auto initial=emulator_test::snapshot(net);require(initial.size()==size && net.get_state_hash()==hash(initial),"complete canonical hash");
 auto short_buffer=initial,before=initial;require(!net.save_state_fast(short_buffer.data(),size-1) && short_buffer==before,"short save preserves destination");
 require(!net.load_state_fast(nullptr,0) && !net.load_state_fast(nullptr,size),"null state rejects");
 for(unsigned port=0;port<2;++port)for(unsigned bit=0;bit<32;++bit){
  require(net.load_state_fast(initial.data(),size),"rewind initial");const uint32_t mask=uint32_t(1)<<bit;
  net.run_frame_netplay(port?0:mask,port?mask:0);const auto after=emulator_test::snapshot(net);
  require(word(after,si+128)==(port?0:mask) && word(after,si+132)==(port?mask:0) && word(after,si+144)==3 && !word(after,si+136) && !word(after,si+140),"input ownership and unused ports");
  require(net.get_state_hash()==hash(after),"hash includes all CPU/RAM/SI state");
  require(net.load_state_fast(initial.data(),size),"rewind frame");net.run_frame_netplay(port?0:mask,port?mask:0);require(emulator_test::snapshot(net)==after,"complete-state replay");
 }
 const auto checkpoint=emulator_test::snapshot(net);auto bad=checkpoint;bad[si+144]=1;
 require(!net.load_state_fast(bad.data(),size) && emulator_test::snapshot(net)==checkpoint,"invalid SI state rejects atomically");
 bad=checkpoint;bad[15]^=1;require(!net.load_state_fast(bad.data(),size) && emulator_test::snapshot(net)==checkpoint,"wrong profile rejects atomically");
 bad=checkpoint;bad.push_back(0);require(!net.load_state_fast(bad.data(),bad.size()) && emulator_test::snapshot(net)==checkpoint,"oversized state rejects");
 auto legacy=checkpoint;legacy.resize(si);legacy[7]='1';require(net.load_state_fast(legacy.data(),legacy.size()),"legacy CPU/RAM checkpoint migration");
 const auto migrated=emulator_test::snapshot(net);require(std::equal(checkpoint.begin()+8,checkpoint.begin()+si,migrated.begin()+8) && word(migrated,si+144)==1 && !word(migrated,si+128) && !word(migrated,si+132),"legacy restores CPU/RAM and neutral first controller");
 net.run_frame_netplay(255,127);core.plugin->run_frame({0x80000001});const auto ordinary=emulator_test::snapshot(net);
 require(word(ordinary,si+128)==0x80000001 && !word(ordinary,si+132) && word(ordinary,si+144)==1,"ordinary frame has one controller");
 net.run_frame_netplay_n({0x12345678,0x87654321});const auto both=emulator_test::snapshot(net);require(word(both,si+128)==0x12345678 && word(both,si+132)==0x87654321,"n-player entry routes both inputs");
 core.plugin->reset();require(emulator_test::snapshot(net)==initial,"reset clears CPU/RAM/SI history");core.plugin->unload_rom();require(!net.save_state_fast(nullptr,0) && !net.get_state_hash() && !net.load_state_fast(initial.data(),size),"unloaded rollback contracts");
 std::cout<<"64 input-bit replay cases; complete hash, short/invalid state guards, legacy restore, local/remote ownership, reset/unload PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
