#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
using emulator_test::require;
struct Sink final : emu::ITestSink {
    std::vector<uint8_t> bytes;
    bool done=false;
    void write(const char* data,size_t size) override {
        bytes.insert(bytes.end(),data,data+size);
    }
    void finish(int32_t status) override {require(status==-1,"native Flash stub termination");done=true;}
    void note(const char*,const char*) override {require(false,"unexpected native Flash note");}
    void blob(const char*,const uint8_t*,size_t) override {require(false,"unexpected native Flash blob");}
    void request_reset() override {require(false,"unexpected native Flash reset");}
};
int main(int argc,char** argv){
    require(argc==4,"plugin, original native Flash ROM and result file required");
    std::ifstream stream(argv[2],std::ios::binary);
    const std::vector<uint8_t> image{std::istreambuf_iterator<char>(stream),{}};
    require(image.size()==0x1004,"original native Flash ROM extent");
    emulator_test::LoadedCore core(argv[1],image);
    const std::filesystem::path result(argv[3]);
    std::filesystem::create_directories(result.parent_path());
    std::ofstream registers(result,std::ios::trunc);
    require(bool(registers),"native Flash register file open");
    registers<<"VELOCE 1 n64 native_flash_identity\nBEGIN ids=22 words=2\n";
    constexpr std::array<const char*,20> ids={
        "NCC","NCV","NDA","NAF","NJF","NKJ","NZS","NM6","NCK","NMQ",
        "NPN","NPF","NPO","CP2","NP3","NRH","NSQ","NT9","NW4","NDP"};
    unsigned cases=0;
    auto observe=[&](const std::vector<uint8_t>& rom,const std::string& label){
        core.fresh_load(rom);
        require(core.plugin->has_battery_save(),"native Flash battery save absent");
        const auto battery=core.plugin->get_battery_save_data();
        require(battery.size()==128*1024 &&
                std::all_of(battery.begin(),battery.end(),[](uint8_t byte){return byte==255;}),
                "native Flash was not selected or erased");
        Sink sink;
        require(core.plugin->set_test_sink(&sink),"native Flash guest sink");
        core.plugin->run_frame({});
        require(sink.done && sink.bytes.size()==8,"native Flash guest ID read did not complete");
        core.plugin->set_test_sink(nullptr);
        uint32_t words[2]{};
        for(unsigned n=0;n<2;++n)
            for(unsigned b=0;b<4;++b)words[n]=(words[n]<<8)|sink.bytes[n*4+b];
        registers<<"REG case="<<label<<" r14="<<std::hex<<std::setw(8)<<std::setfill('0')<<words[0]
                 <<" r15="<<std::setw(8)<<words[1]
                 <<std::dec<<" battery_bytes="<<battery.size()<<'\n';
        require(bool(registers),"native Flash register file write");
        ++cases;
    };
    for(const char* id:ids){
        auto variant=image;
        std::copy_n(id,3,variant.begin()+0x3b);
        observe(variant,id);
    }
    for(unsigned kind=0;kind<2;++kind){
        auto variant=image;
        if(kind==0)for(size_t at=0;at<variant.size();at+=2)std::swap(variant[at],variant[at+1]);
        else for(size_t at=0;at<variant.size();at+=4){
            std::swap(variant[at],variant[at+3]);std::swap(variant[at+1],variant[at+2]);
        }
        observe(variant,kind==0?"NCV-v64":"NCV-n64");
    }
    registers<<"END observations_complete\n";
    registers.flush();
    require(cases==22 && bool(registers),"native Flash register file incomplete");
    std::cout<<"22 native FlashRAM guest silicon ID register rows captured\n";
}
