#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <algorithm>
using emulator_test::require;
int main(int argc,char** argv){try{
 require(argc==3,"core and original controller ROM required");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
 emulator_test::LoadedCore muted(argv[1],rom),streamed(argv[1],rom);auto& a=muted.netplay();auto& b=streamed.netplay();require(a.get_max_players()==4 && a.is_deterministic(),"netplay capability");
 uint64_t chunks=0;streamed.plugin->set_audio_callback([&](const float*,size_t count,int rate){require(count && rate==48000,"actual AI output");++chunks;});
 for(unsigned frame=0;frame<20;++frame){std::vector<uint32_t> inputs={frame*1234567u+3,frame*9876543u+7,~frame,frame<<20};a.run_frame_netplay_n(inputs);b.run_frame_netplay_n(inputs);}
 auto saved=emulator_test::snapshot(a);const auto streamed_state=emulator_test::snapshot(b);require(chunks,"actual guest AI callback missing");require(saved==streamed_state,"muted and streamed complete states differ");require(saved.size()==a.get_max_state_size(),"current state query exactness");
 uint64_t hash=0xcbf29ce484222325ULL;for(auto byte:saved)hash=(hash^byte)*0x100000001b3ULL;require(a.get_state_hash()==hash,"complete state hash");
 std::vector<uint8_t> guard(saved.size(),0xa5);require(!a.save_state_fast(guard.data(),guard.size()-1) && std::all_of(guard.begin(),guard.end(),[](uint8_t x){return x==0xa5;}),"undersized save writes");
 require(a.save_state_fast(nullptr,0)==saved.size() && emulator_test::snapshot(a)==saved,"query mutates state");
 a.run_frame_netplay(0xabcdef01,0x87654321);require(a.load_state_fast(saved.data(),saved.size()) && emulator_test::snapshot(a)==saved,"fast exact replay restore");
 auto bad=saved;bad.back()^=1;bad[bad.size()-1049408]=1;require(!a.load_state_fast(bad.data(),bad.size()) && emulator_test::snapshot(a)==saved,"invalid fast restore atomicity");require(!a.load_state_fast(nullptr,0),"null restore");
 muted.plugin->unload_rom();require(a.get_max_state_size()==0 && a.save_state_fast(nullptr,0)==0 && a.get_state_hash()==0 && !a.load_state_fast(saved.data(),saved.size()),"unloaded state contract");
 streamed.plugin->set_audio_callback(nullptr);std::cout<<"production four-input rollback API, full hash, muted/streamed determinism and state guards PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
