#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
struct Sink final : emu::ITestSink {
 std::vector<char> output;
 void write(const char* bytes,size_t count) override{output.insert(output.end(),bytes,bytes+count);}
 void finish(int32_t) override{throw std::runtime_error("unexpected early completion");}
 void note(const char*,const char*) override{throw std::runtime_error("unexpected note");}
 void blob(const char*,const uint8_t*,size_t) override{throw std::runtime_error("unexpected blob");}
 void request_reset() override{throw std::runtime_error("unexpected reset");}
};
int main(int argc,char** argv){try{
 require(argc==3,"production core and original NSO required");std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
 emulator_test::LoadedCore core(argv[1],rom);require(std::string(core.plugin->get_info().name)=="Switch" && std::string(core.plugin->test_channels())=="vnxdebug","core/channel identity");
 Sink sink;require(core.plugin->set_test_sink(&sink),"test sink attachment");
 for(unsigned n=0;n<3;++n)core.plugin->run_frame({});std::vector<uint8_t> checkpoint,after,replay;require(core.plugin->save_state(checkpoint),"checkpoint save");
 sink.output.clear();for(unsigned n=0;n<8;++n)core.plugin->run_frame({});const auto captures=sink.output;const auto cycles=core.plugin->get_cycle_count();
 require(!captures.empty() && core.plugin->get_frame_count()==11 && core.plugin->save_state(after),"real guest capture/snapshot");
 require(core.plugin->load_state(checkpoint) && core.plugin->get_frame_count()==3,"production restore");sink.output.clear();
 for(unsigned n=0;n<8;++n)core.plugin->run_frame({});
 require(core.plugin->save_state(replay) && after==replay && captures==sink.output && cycles==core.plugin->get_cycle_count(),"full production state and actual register stream replay");
 auto bad=after;bad[328]=1;require(!core.plugin->load_state(bad) && core.plugin->save_state(replay) && replay==after,"production invalid state atomicity");
 const auto code=core.plugin->read_memory(0);core.plugin->write_memory(0,code^1);require(core.plugin->read_memory(0)==code,"legacy UI write preserves executable protection");
 core.plugin->set_test_sink(nullptr);core.plugin->reset();require(!core.plugin->get_frame_count() && !core.plugin->get_cycle_count(),"production reset counters");core.plugin->unload_rom();require(!core.plugin->is_rom_loaded() && !core.plugin->save_state(replay) && !core.plugin->load_state(after),"unloaded state contracts");
 std::cout<<"production ABI identity, original register-stream/full-state replay, guards, reset/unload PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
