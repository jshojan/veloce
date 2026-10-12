#include "nes_plugin_fixture.hpp"
#include "emu/rumble_output.hpp"
#include "emu/netplay_plugin.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
using emulator_test::require;
struct Sink final:emu::ITestSink {
    std::ofstream& output;
    bool done=false;
    size_t bytes_written=0;
    explicit Sink(std::ofstream& file):output(file){}
    void write(const char* bytes,size_t size) override {output.write(bytes,size);bytes_written+=size;}
    void finish(int32_t code) override {require(code==-1,"unexpected guest finish");done=true;}
    void note(const char*,const char*) override {require(false,"unexpected guest note");}
    void blob(const char*,const uint8_t*,size_t) override {require(false,"unexpected guest blob");}
    void request_reset() override {require(false,"unexpected guest reset");}
};
int main(int argc,char** argv){
    require(argc==4,"plugin, ROM and register file required");
    std::ifstream input(argv[2],std::ios::binary);
    const std::vector<uint8_t> rom{std::istreambuf_iterator<char>(input),{}};
    require(rom.size()>0x1004,"original ROM missing");
    emulator_test::LoadedCore core(argv[1],rom);
    const auto config=std::filesystem::path(argv[3]).parent_path()/"rumble-config.json";
    std::filesystem::create_directories(config.parent_path());
    {std::ofstream file(config);file<<"{\"controller_paks\":0,\"rumble_paks\":15}\n";require(bool(file),"config write");}
    core.plugin->unload_rom();
    require(core.plugin->load_config(config.c_str()) && core.plugin->load_rom(rom.data(),rom.size()),"Rumble Pak ROM load");
    auto* rumble=dynamic_cast<emu::IRumbleOutput*>(core.plugin);
    require(rumble,"Rumble output interface");
    std::ofstream results(argv[3],std::ios::binary|std::ios::trunc);
    require(bool(results),"register file open");
    Sink sink(results);
    require(core.plugin->set_test_sink(&sink),"guest debug sink");
    for(unsigned frame=0;frame<3000 && !sink.done;++frame)core.plugin->run_frame({});
    require(sink.done && bool(results),
            ("guest did not finish register corpus; bytes="+std::to_string(sink.bytes_written)).c_str());
    for(unsigned port=0;port<4;++port)require(!rumble->rumble_active(port),"guest left motor running");
    auto* netplay=dynamic_cast<emu::INetplayCapable*>(core.plugin);
    require(netplay,"Rumble netplay interface");
    std::vector<uint8_t> snapshot(netplay->save_state_fast(nullptr,0));
    require(snapshot.size()==netplay->get_max_state_size() && snapshot.size()>8 &&
            netplay->save_state_fast(snapshot.data(),snapshot.size())==snapshot.size() &&
            snapshot[7]=='5' && netplay->load_state_fast(snapshot.data(),snapshot.size()),
            "S035 frontend rollback");
    {std::ofstream file(config);file<<"{\"controller_paks\":1,\"rumble_paks\":1}\n";}
    require(!core.plugin->load_config(config.c_str()),"overlapping accessory masks accepted");
    core.plugin->set_test_sink(nullptr);
    results.close();require(bool(results),"register file close");
}
