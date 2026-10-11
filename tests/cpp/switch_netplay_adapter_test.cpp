#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint64_t hash(const std::vector<uint8_t>& bytes){uint64_t h=0xcbf29ce484222325ull;for(auto b:bytes)h=(h^b)*0x100000001b3ull;return h;}
static uint64_t scalar(const std::vector<uint8_t>& bytes,unsigned at){uint64_t v=0;for(unsigned n=0;n<8;++n)v=v*256+bytes[at+n];return v;}
int main(int argc,char** argv){try{
 require(argc==3,"Switch core and original netplay image required");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
 emulator_test::LoadedCore core(argv[1],rom);auto& net=core.netplay();require(net.is_deterministic() && net.get_max_players()==2 && net.get_max_state_size()==4194664,"two-input deterministic rollback capability");
 auto initial=emulator_test::snapshot(net);require(initial.size()==4194664 && net.get_state_hash()==hash(initial),"complete canonical state/hash agreement");
 std::vector<uint8_t> small(initial.size(),0xa5),unchanged=small;require(!net.save_state_fast(small.data(),small.size()-1) && small==unchanged,"short fast-save buffer remains untouched");
 require(!net.load_state_fast(nullptr,0) && !net.load_state_fast(nullptr,initial.size()),"null state input rejects");
 for(unsigned port=0;port<2;++port)for(unsigned bit=0;bit<32;++bit){
  require(net.load_state_fast(initial.data(),initial.size()),"initial fast restore");const uint32_t mask=uint32_t(1)<<bit;
  net.run_frame_netplay(port?0:mask,port?mask:0);const auto after=emulator_test::snapshot(net);
  require(scalar(after,344)==(port?0:mask) && scalar(after,352)==(port?mask:0) && net.get_state_hash()==hash(after),"all input bits are retained in the complete state/hash");
  require(net.load_state_fast(initial.data(),initial.size()),"frame rewind");net.run_frame_netplay(port?0:mask,port?mask:0);require(emulator_test::snapshot(net)==after,"input-aware complete replay");
 }
 net.run_frame_netplay(0x89abcdef,0x76543210);const auto checkpoint=emulator_test::snapshot(net);
 for(unsigned at:{344u,352u}){auto bad=checkpoint;bad[at]=1;require(!net.load_state_fast(bad.data(),bad.size()) && emulator_test::snapshot(net)==checkpoint,"reserved high input bits reject atomically");}
 auto bad=checkpoint;bad.push_back(0);require(!net.load_state_fast(bad.data(),bad.size()) && emulator_test::snapshot(net)==checkpoint,"oversized fast state rejects atomically");
 // Real v1 byte layout, excluding the new input slots and using the prior magic.
 auto legacy=checkpoint;legacy[7]='1';legacy.erase(legacy.begin()+344,legacy.begin()+360);
 require(net.load_state_fast(legacy.data(),legacy.size()),"legacy state migration");auto migrated=emulator_test::snapshot(net);
 require(scalar(migrated,344)==0 && scalar(migrated,352)==0 && std::equal(checkpoint.begin()+360,checkpoint.end(),migrated.begin()+360) && std::equal(checkpoint.begin()+8,checkpoint.begin()+344,migrated.begin()+8),"legacy migration preserves CPU/RAM and neutralizes both inputs");
 net.run_frame_netplay(255,127);core.plugin->run_frame({0x80000001});const auto ordinary=emulator_test::snapshot(net);require(scalar(ordinary,344)==0x80000001 && scalar(ordinary,352)==0,"ordinary frames neutralize absent second input");
 net.run_frame_netplay_n({0x12345678,0x87654321});const auto both=emulator_test::snapshot(net);require(scalar(both,344)==0x12345678 && scalar(both,352)==0x87654321,"n-player entry routes both supported inputs");
 core.plugin->reset();require(emulator_test::snapshot(net)==initial,"reset clears input/counters/guest RAM");core.plugin->unload_rom();require(!net.save_state_fast(nullptr,0) && !net.get_state_hash() && !net.load_state_fast(initial.data(),initial.size()),"unloaded rollback contracts");
 std::cout<<"64 input-bit/replay cases; exact hash, buffer/state guards, v1 migration, reset/unload PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
