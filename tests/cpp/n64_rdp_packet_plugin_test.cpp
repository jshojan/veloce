#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
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
 require(argc==3,"production core and original packet cartridge required");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
 emulator_test::LoadedCore core(argv[1],rom);auto& net=core.netplay();Sink sink;require(core.plugin->set_test_sink(&sink),"actual guest sink");
 std::vector<uint8_t> saved;unsigned warm=0;bool partial=false;
 for(;warm<128;++warm){net.run_frame_netplay(0,0);saved=emulator_test::snapshot(net);const size_t at=saved.size()-200;
  const auto count=be(saved,at+8),needed=be(saved,at+16);if(count && count<needed){partial=true;++warm;break;}}
 require(partial && saved[7]=='4' && saved.size()==8478944 && net.get_max_state_size()==saved.size(),"real partial S024 state and exact query");
 sink.bytes.clear();for(unsigned i=0;i<3;++i)net.run_frame_netplay(0,0);const auto output=sink.bytes;const auto after=emulator_test::snapshot(net);
 require(!output.empty() && core.plugin->get_frame_count()==warm+3,"real packed stream after partial checkpoint");
 require(net.load_state_fast(saved.data(),saved.size()),"fast partial restore");sink.bytes.clear();for(unsigned i=0;i<3;++i)net.run_frame_netplay(0,0);
 require(output==sink.bytes && after==emulator_test::snapshot(net),"production actual register output/full-state partial FIFO replay");
 auto bad=after;bad[bad.size()-200]=1;require(!net.load_state_fast(bad.data(),bad.size()) && after==emulator_test::snapshot(net),"invalid parser state fast restore atomicity");
 core.plugin->set_test_sink(nullptr);std::cout<<"production netplay ABI partial-command checkpoint, actual output/full-state replay, S024 capacity and rejection PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
