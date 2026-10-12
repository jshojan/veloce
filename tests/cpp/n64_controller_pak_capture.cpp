#include "nes_plugin_fixture.hpp"
#include "emu/controller_pak_storage.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
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
    require(argc==5,"plugin, ROM, register file and Pak image required");
    std::ifstream input(argv[2],std::ios::binary);
    const std::vector<uint8_t> rom{std::istreambuf_iterator<char>(input),{}};
    require(rom.size()>0x1004,"original ROM missing");
    emulator_test::LoadedCore core(argv[1],rom);
    const auto config=std::filesystem::path(argv[3]).parent_path()/"controller-pak-config.json";
    std::filesystem::create_directories(config.parent_path());
    {std::ofstream file(config);file<<"{\"controller_paks\":15}\n";require(bool(file),"config write");}
    core.plugin->unload_rom();
    require(core.plugin->load_config(config.c_str()) && core.plugin->load_rom(rom.data(),rom.size()),"inserted Pak ROM load");
    auto* storage=dynamic_cast<emu::IControllerPakStorage*>(core.plugin);
    require(storage && storage->controller_pak_ports()==4,"Pak storage interface");
    std::ofstream results(argv[3],std::ios::binary|std::ios::trunc);
    require(bool(results),"register file open");
    Sink sink(results);
    require(core.plugin->set_test_sink(&sink),"guest debug sink");
    for(unsigned frame=0;frame<1000 && !sink.done;++frame)core.plugin->run_frame({});
    require(sink.done && bool(results),"guest did not finish register corpus");
    core.plugin->set_test_sink(nullptr);
    results.close();require(bool(results),"register file close");
    std::ofstream paks(argv[4],std::ios::binary|std::ios::trunc);
    require(bool(paks),"Pak image open");
    for(unsigned port=0;port<4;++port){
        require(storage->controller_pak_inserted(port),"guest Pak was removed");
        const auto bytes=storage->get_controller_pak_data(port);
        require(bytes.size()==32768,"Pak image extent");
        paks.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    }
    paks.close();require(bool(paks),"Pak image close");
    std::cout<<"N64 guest Controller Pak register and media captures complete\n";
}
