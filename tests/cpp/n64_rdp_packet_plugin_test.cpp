#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <algorithm>
using emulator_test::require;
struct Sink final:emu::ITestSink{
 std::vector<char> bytes;
 void write(const char* data,size_t count)override{bytes.insert(bytes.end(),data,data+count);}
 void finish(int32_t)override{throw std::runtime_error("early completion");}
 void note(const char*,const char*)override{throw std::runtime_error("unexpected note");}
 void blob(const char*,const uint8_t*,size_t)override{throw std::runtime_error("unexpected blob");}
 void request_reset()override{throw std::runtime_error("unexpected reset");}
};
static uint64_t be(const std::vector<uint8_t>& data,size_t at){uint64_t out=0;for(unsigned i=0;i<8;++i)out=(out<<8)|data[at+i];return out;}
int main(int argc,char** argv){try{
 const bool primitive_mode=argc==4 && std::string(argv[3])=="primitive";
 const bool hidden_mode=argc==4 && std::string(argv[3])=="hidden";
 const bool blender_mode=argc==4 && std::string(argv[3])=="blender";
 const bool texture_mode=argc==4 && std::string(argv[3])=="texture";
 const bool perspective_mode=argc==4 && std::string(argv[3])=="perspective";
 const bool conversion_mode=argc==4 && std::string(argv[3])=="conversion";
 require(argc==3 || primitive_mode || hidden_mode || blender_mode || texture_mode || conversion_mode || perspective_mode,"production core and original cartridge required");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
 emulator_test::LoadedCore core(argv[1],rom);auto& net=core.netplay();Sink sink;require(core.plugin->set_test_sink(&sink),"actual guest sink");
 std::vector<uint8_t> saved;unsigned warm=0;bool partial=false;
 for(;warm<128;++warm){net.run_frame_netplay(0,0);saved=emulator_test::snapshot(net);const size_t at=saved.size()-1048832;
  const auto count=be(saved,at+8),needed=be(saved,at+16);
  const bool checkpoint=primitive_mode?be(saved,saved.size()-8)!=0:hidden_mode?std::any_of(saved.begin()+saved.size()-1048600+0x700000/8,saved.begin()+saved.size()-1048600+0x700000/8+4,[](uint8_t v){return v!=0;}):blender_mode?(be(saved,saved.size()-1048624) && be(saved,saved.size()-1048616) && be(saved,saved.size()-1048608)):perspective_mode?((be(saved,saved.size()-1053760)&(1ull<<51)) && std::any_of(saved.end()-1052952,saved.end()-1048856,[](uint8_t v){return v!=0;})):conversion_mode?be(saved,saved.size()-1048632)!=0:texture_mode?std::any_of(saved.end()-1052952,saved.end()-1048856,[](uint8_t v){return v!=0;}):count && count<needed;
  if(checkpoint){partial=true;++warm;break;}}
 require(partial && saved[7]=='8' && saved.size()==9527576 && net.get_max_state_size()==saved.size(),"real guest S028 checkpoint and exact query");
 sink.bytes.clear();for(unsigned i=0;i<3;++i)net.run_frame_netplay(0,0);const auto output=sink.bytes;const auto after=emulator_test::snapshot(net);
 require(!output.empty() && core.plugin->get_frame_count()==warm+3,"real packed stream after partial checkpoint");
 require(net.load_state_fast(saved.data(),saved.size()),"fast partial restore");sink.bytes.clear();for(unsigned i=0;i<3;++i)net.run_frame_netplay(0,0);
 require(output==sink.bytes && after==emulator_test::snapshot(net),"production actual register output/full-state partial FIFO replay");
 auto bad=after;bad[bad.size()-1048832]=1;require(!net.load_state_fast(bad.data(),bad.size()) && after==emulator_test::snapshot(net),"invalid parser state fast restore atomicity");
 if(primitive_mode){bad=after;bad[bad.size()-8]=1;require(!net.load_state_fast(bad.data(),bad.size()) && after==emulator_test::snapshot(net),"invalid primitive depth fast restore atomicity");}
 if(hidden_mode){bad=after;bad[bad.size()-24]=128;require(!net.load_state_fast(bad.data(),bad.size()) && after==emulator_test::snapshot(net),"invalid hidden history fast restore atomicity");}
 if(blender_mode){bad=after;bad[bad.size()-1048624]=128;require(!net.load_state_fast(bad.data(),bad.size()) && after==emulator_test::snapshot(net),"invalid blender state fast restore atomicity");}
 if(conversion_mode){bad=after;bad[bad.size()-1048632]=128;require(!net.load_state_fast(bad.data(),bad.size()) && after==emulator_test::snapshot(net),"invalid conversion state fast restore atomicity");}
 core.plugin->set_test_sink(nullptr);std::cout<<(primitive_mode?"production primitive-depth checkpoint":hidden_mode?"production hidden CPU/EBUS checkpoint":blender_mode?"production blender/memory-latch checkpoint":perspective_mode?"production perspective/TMEM checkpoint":conversion_mode?"production conversion/TMEM checkpoint":texture_mode?"production texture/TMEM checkpoint":"production partial-command checkpoint")<<", actual output/full-state replay, netplay ABI S028 capacity and rejection PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
