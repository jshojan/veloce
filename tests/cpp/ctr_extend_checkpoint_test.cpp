#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
struct Sink final:emu::ITestSink {
 std::string text;bool done=false;
 void write(const char* bytes,size_t count)override{text.append(bytes,count);}
 void finish(int32_t status)override{require(status==-1,"guest verdict unsupported");done=true;}
 void note(const char*,const char*)override{throw std::runtime_error("unexpected note");}
 void blob(const char*,const uint8_t*,size_t)override{throw std::runtime_error("unexpected blob");}
 void request_reset()override{throw std::runtime_error("unexpected reset");}
};
int main(int argc,char** argv){try{
 require(argc==3,"core and original extension image required");std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
 emulator_test::LoadedCore core(argv[1],rom);Sink sink;require(core.plugin->set_test_sink(&sink),"original register sink");auto& net=core.netplay();
 unsigned rows=0;
 for(unsigned boundary:{0u,1u,3u}) {
  core.plugin->reset();for(unsigned n=0;n<boundary;++n)core.plugin->run_frame({0});
  const auto before=emulator_test::snapshot(net);require(before.size()==135791096,"complete 3DS snapshot size");
  sink.text.clear();sink.done=false;for(unsigned n=0;n<2;++n)core.plugin->run_frame({0});
  require(!sink.done && sink.text.find("REG r=")!=std::string::npos,"actual extension continuation output");
  const auto text=sink.text;const auto after=emulator_test::snapshot(net);
  require(net.load_state_fast(before.data(),before.size()),"extension fast restore");sink.text.clear();sink.done=false;
  for(unsigned n=0;n<2;++n)net.run_frame_netplay(0,0);
  require(sink.text==text && !sink.done && emulator_test::snapshot(net)==after,"actual register output/full native rollback mismatch");
  for(size_t at=0;(at=text.find("REG r=",at))!=std::string::npos;at+=6)++rows;
 }
 core.plugin->set_test_sink(nullptr);std::cout<<rows<<" actual extension register rows; three checkpoints and two-frame native/fast continuations match complete states PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
