#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <iomanip>
using emulator_test::require;
struct Sink:emu::ITestSink{
 std::ofstream& out;bool done=false,record=false;std::string trace;explicit Sink(std::ofstream& o):out(o){}
 void write(const char* p,size_t n)override{out.write(p,n);if(record)trace.append(p,n);}void finish(int32_t status)override{require(status==-1,"guest supplied verdict");done=true;}
 void note(const char*,const char*)override{}void blob(const char*,const uint8_t*,size_t)override{}void request_reset()override{throw std::runtime_error("unexpected reset");}
};
int main(int argc,char** argv){try{
 require(argc==4,"plugin, ROM and output required");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};emulator_test::LoadedCore core(argv[1],rom);
 require(core.plugin->has_battery_save() && core.plugin->get_battery_save_data().size()==8192,"cartridge battery contract");
 // Native active checkpoint/output continuation before the measured full run.
 std::ofstream discarded;Sink trial(discarded);trial.record=true;core.plugin->set_test_sink(&trial);core.plugin->run_frame({0});std::vector<uint8_t> before,after,replay;if(!trial.done){require(core.plugin->save_state(before),"active cartridge checkpoint");trial.trace.clear();core.plugin->run_frame({0});auto emitted=trial.trace;require(core.plugin->save_state(after) && core.plugin->load_state(before),"native SRAM restore");trial.trace.clear();core.plugin->run_frame({0});require(core.plugin->save_state(replay) && replay==after && trial.trace==emitted && !emitted.empty(),"native SRAM complete-state/output replay");}
 core.plugin->set_test_sink(nullptr);core.fresh_load(rom);std::ofstream output(argv[3],std::ios::binary);Sink sink(output);core.plugin->set_test_sink(&sink);
 output<<"#VELOCE 1 core=VB rom_crc32="<<std::hex<<std::setfill('0')<<std::setw(8)<<core.plugin->get_rom_crc32()<<" channels=v810debug\n";
 for(unsigned frame=0;frame<20000 && !sink.done;++frame)core.plugin->run_frame({0});require(sink.done,"cartridge guest did not finish");
 output<<"#VELOCE end reason=terminator frames="<<std::dec<<core.plugin->get_frame_count()<<" cycles="<<core.plugin->get_cycle_count()<<" status=-1\n";core.plugin->set_test_sink(nullptr);output.close();require(output.good(),"actual observation persistence");
 auto battery=core.plugin->get_battery_save_data();std::ofstream save(std::string(argv[3])+".sav",std::ios::binary);save.write(reinterpret_cast<const char*>(battery.data()),battery.size());save.close();require(save.good(),"battery capture persistence");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
