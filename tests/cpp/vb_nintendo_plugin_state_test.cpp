#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
struct Sink:emu::ITestSink{
 std::string text;int finishes=0;
 void write(const char* data,size_t size)override{text.append(data,size);}
 void note(const char*,const char*)override{}
 void blob(const char*,const uint8_t*,size_t)override{}
 void request_reset()override{throw std::runtime_error("unexpected reset");}
 void finish(int)override{++finishes;}
};
int main(int argc,char** argv){try{
 require(argc==3,"core and original ROM required");std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
 emulator_test::LoadedCore core(argv[1],rom);Sink sink;require(core.plugin->set_test_sink(&sink),"native sink required");
 core.plugin->run_frame({0});require(!sink.text.empty() && !sink.finishes,"checkpoint must be during active observations");
 std::vector<uint8_t> before,after,replay;require(core.plugin->save_state(before) && before.size()==66208,"native complete state format");sink.text.clear();
 core.plugin->run_frame({0});auto output=sink.text;auto cycles=core.plugin->get_cycle_count();require(!output.empty() && !sink.finishes,"active continuation required");require(core.plugin->save_state(after),"native continuation save");
 require(core.plugin->load_state(before),"native checkpoint restore");sink.text.clear();core.plugin->run_frame({0});require(core.plugin->save_state(replay),"native replay save");
 require(replay==after && sink.text==output && core.plugin->get_cycle_count()==cycles,"native state/output continuation differs");
 core.plugin->set_test_sink(nullptr);std::cout<<"Native active instruction/output checkpoint PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
