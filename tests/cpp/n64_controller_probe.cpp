#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <iomanip>
using emulator_test::require;
struct Sink final : emu::ITestSink {
    std::ofstream file;bool done=false;int status=0;
    explicit Sink(const char* path):file(path){require(file.good(),"result file open");}
    void write(const char* bytes,size_t count) override {file.write(bytes,count);}
    void finish(int32_t code) override {require(!done,"duplicate guest terminator");done=true;status=code;}
    void note(const char*,const char*) override {throw std::runtime_error("unexpected metadata channel");}
    void blob(const char*,const uint8_t*,size_t) override {throw std::runtime_error("unexpected blob channel");}
    void request_reset() override {throw std::runtime_error("unexpected guest reset");}
};
int main(int argc,char** argv){try{
 require(argc==5,"core ROM output count required");std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
 emulator_test::LoadedCore core(argv[1],rom);Sink sink(argv[3]);core.plugin->set_test_sink(&sink);
 sink.file<<"#VELOCE 1 core=N64 rom_crc32="<<std::hex<<std::setw(8)<<std::setfill('0')<<core.plugin->get_rom_crc32()<<" channels=v64debug\n"<<std::dec;
 const unsigned count=unsigned(std::stoul(argv[4]));require(count && count<=16384,"invalid input domain");
 const unsigned bits[]={0,1,20,6,8,9,10,11,4,5,21,22,23,24};uint32_t valid=0;for(auto bit:bits)valid|=1u<<bit;
 const unsigned multiplier[]={1,3,5,7},offset[]={0,1,7,11};
 for(unsigned raw=0;raw<count;++raw){std::vector<uint32_t> inputs(4);
  for(unsigned port=0;port<4;++port){const unsigned value=(raw*multiplier[port]+offset[port])%16384;for(unsigned bit=0;bit<14;++bit)if(value&(1u<<bit))inputs[port]|=1u<<bits[bit];if(raw&1)inputs[port]|=~(valid|0xc000u);}
  core.netplay().run_frame_netplay_n(inputs);
  require(core.plugin->get_frame_count()==raw+1,"quantum count");require(sink.done==(raw+1==count),"guest termination bound");
 }
 require(sink.done && sink.status==-1,"guest terminator");
 sink.file<<"#VELOCE end reason=terminator frames="<<core.plugin->get_frame_count()<<" cycles="<<core.plugin->get_cycle_count()<<" status="<<sink.status<<"\n";
 sink.file.close();require(sink.file.good(),"result file persistence");core.plugin->set_test_sink(nullptr);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
