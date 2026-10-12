#include "nes_plugin_fixture.hpp"
#include "emu/netplay_plugin.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <vector>
using emulator_test::require;
struct Sink final : emu::ITestSink {
    std::vector<uint8_t> bytes;
    bool done=false;
    void write(const char* data,size_t size) override {bytes.insert(bytes.end(),data,data+size);}
    void finish(int32_t status) override {require(status==-1,"RTC cartridge termination");done=true;}
    void note(const char*,const char*) override {require(false,"unexpected RTC note");}
    void blob(const char*,const uint8_t*,size_t) override {require(false,"unexpected RTC blob");}
    void request_reset() override {require(false,"unexpected RTC reset");}
};
int main(int argc,char** argv){
    require(argc==4,"plugin, original RTC cartridge and register file required");
    std::ifstream stream(argv[2],std::ios::binary);
    const std::vector<uint8_t> rom{std::istreambuf_iterator<char>(stream),{}};
    require(rom.size()==0x1004,"original RTC cartridge extent");
    emulator_test::LoadedCore core(argv[1],rom);
    Sink sink;require(core.plugin->set_test_sink(&sink),"RTC test sink");
    core.plugin->run_frame({});
    require(sink.done && sink.bytes.size()==13,"RTC guest register capture length");
    core.plugin->set_test_sink(nullptr);
    const auto battery=core.plugin->get_battery_save_data();
    require(battery.size()==131112,"RTC FlashRAM and clock battery media");
    auto* netplay=dynamic_cast<emu::INetplayCapable*>(core.plugin);
    require(netplay,"RTC netplay state interface");
    std::vector<uint8_t> snapshot(netplay->save_state_fast(nullptr,0));
    require(snapshot.size()==netplay->get_max_state_size() &&
            netplay->save_state_fast(snapshot.data(),snapshot.size())==snapshot.size() &&
            snapshot[7]=='8' && netplay->load_state_fast(snapshot.data(),snapshot.size()),
            "RTC S038 state round trip");
    const std::filesystem::path path(argv[3]);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream result(path,std::ios::trunc);
    require(bool(result),"RTC register file open");
    result<<"VELOCE 1 n64 cartridge_rtc\nBEGIN joybus words=13\nREG bytes=";
    for(uint8_t value:sink.bytes)result<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(value);
    result<<"\nEND observations_complete\n";
    result.flush();require(bool(result),"RTC register file write");
}
