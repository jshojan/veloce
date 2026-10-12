#include "nes_plugin_fixture.hpp"
#include "emu/netplay_plugin.hpp"
#include <fstream>
#include <iterator>
#include <vector>
using emulator_test::require;
struct Sink final:emu::ITestSink {
    std::ofstream& output;
    bool done=false;
    explicit Sink(std::ofstream& file):output(file){}
    void write(const char* bytes,size_t size) override {output.write(bytes,size);}
    void finish(int32_t code) override {require(code==-1,"unexpected guest finish");done=true;}
    void note(const char*,const char*) override {require(false,"unexpected guest note");}
    void blob(const char*,const uint8_t*,size_t) override {require(false,"unexpected guest blob");}
    void request_reset() override {require(false,"unexpected guest reset");}
};
int main(int argc,char** argv){
    require(argc==5,"plugin, ROM, register file and SRAM file required");
    std::ifstream input(argv[2],std::ios::binary);
    const std::vector<uint8_t> rom{std::istreambuf_iterator<char>(input),{}};
    require(rom.size()>0x1004,"original ROM missing");
    emulator_test::LoadedCore core(argv[1],rom);
    require(core.plugin->has_battery_save(),"cartridge SRAM not selected");
    std::ofstream results(argv[3],std::ios::binary|std::ios::trunc);
    require(bool(results),"register file open");
    Sink sink(results);
    require(core.plugin->set_test_sink(&sink),"guest debug sink");
    for(unsigned frame=0;frame<1000 && !sink.done;++frame)core.plugin->run_frame({});
    require(sink.done && bool(results),"guest did not finish SRAM corpus");
    core.plugin->set_test_sink(nullptr);
    results.close();require(bool(results),"register file close");
    auto* netplay=dynamic_cast<emu::INetplayCapable*>(core.plugin);
    require(netplay,"SRAM netplay interface");
    std::vector<uint8_t> snapshot(netplay->save_state_fast(nullptr,0));
    require(snapshot.size()==netplay->get_max_state_size() && snapshot.size()>8 &&
            netplay->save_state_fast(snapshot.data(),snapshot.size())==snapshot.size() &&
            snapshot[7]=='6' && netplay->load_state_fast(snapshot.data(),snapshot.size()),
            "S036 frontend rollback");
    const auto bank=core.plugin->get_battery_save_data();
    require(bank.size()==32768 || bank.size()==98304,"SRAM bank length");
    std::ofstream save(argv[4],std::ios::binary|std::ios::trunc);
    save.write(reinterpret_cast<const char*>(bank.data()),bank.size());
    save.close();require(bool(save),"SRAM file close");
}
