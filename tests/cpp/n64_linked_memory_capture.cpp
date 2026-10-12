#include "nes_plugin_fixture.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
struct Sink final:emu::ITestSink{
    bool done=false;
    void write(const char*,size_t)override{throw std::runtime_error("unexpected text");}
    void finish(int32_t status)override{require(status==-1,"unexpected guest verdict");done=true;}
    void note(const char*,const char*)override{throw std::runtime_error("unexpected note");}
    void blob(const char*,const uint8_t*,size_t)override{throw std::runtime_error("unexpected blob");}
    void request_reset()override{throw std::runtime_error("unexpected reset");}
};
static void word(std::ofstream& out,uint64_t value){for(unsigned n=0;n<8;++n)out.put(char(value>>(56-n*8)));}
static std::array<char,80> observe(emu::IEmulatorPlugin& core){std::array<char,80> bytes;for(unsigned n=0;n<80;++n)bytes[n]=char(core.read_memory(uint16_t(0x2000+n)));return bytes;}
static uint64_t count(const std::array<char,80>& bytes){uint64_t value=0;for(unsigned n=0;n<8;++n)value=value*256+uint8_t(bytes[n]);return value;}
int main(int argc,char** argv){try{
    require(argc==5,"core ROM output cases");uint64_t cases=0;const std::string arg=argv[4];const auto parsed=std::from_chars(arg.data(),arg.data()+arg.size(),cases);require(parsed.ec==std::errc{} && parsed.ptr==arg.data()+arg.size() && cases>=256 && cases<=1048576,"case count");
    std::ifstream file(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};emulator_test::LoadedCore core(argv[1],rom);Sink sink;require(core.plugin->set_test_sink(&sink),"sink");
    core.plugin->run_frame({});require(!count(observe(*core.plugin)) && !sink.done,"live linked checkpoint boundary");
    auto& net=core.netplay();const auto saved=emulator_test::snapshot(net);require(saved.size()==9528128 && uint8_t(saved[637])==1,"LLbit serialized in S032");
    std::vector<std::array<char,80>> rows;for(unsigned n=0;n<12;++n){core.plugin->run_frame({});rows.push_back(observe(*core.plugin));}const auto after=emulator_test::snapshot(net);
    require(net.load_state_fast(saved.data(),saved.size()),"live linked fast restore");for(unsigned n=0;n<12;++n){net.run_frame_netplay_n({0,0,0,0});require(observe(*core.plugin)==rows[n],"linked RAM rollback");}require(emulator_test::snapshot(net)==after,"complete linked rollback state");
    auto old=saved;old.resize(old.size()-512);old[7]='0';old[637]=0;require(net.load_state_fast(old.data(),old.size()),"S030 import");core.plugin->run_frame({});require(uint8_t(observe(*core.plugin)[63])==0,"legacy import retained nonexistent LLbit");
    require(net.load_state_fast(saved.data(),saved.size()),"restore before rejection");for(unsigned bit=0;bit<64;++bit)if(bit!=0 && bit!=8 && bit!=16){auto bad=saved;bad[639-bit/8]|=uint8_t(1u<<(bit%8));require(!net.load_state_fast(bad.data(),bad.size()) && emulator_test::snapshot(net)==saved,"atomic reserved flag rejection");}
    core.plugin->reset();sink.done=false;core.plugin->run_frame({});require(!count(observe(*core.plugin)),"initial LL yield");
    std::ofstream out(argv[3],std::ios::binary);require(bool(out),"register output");out.write("V64LINK1",8);word(out,cases);word(out,10);word(out,core.plugin->get_rom_crc32());
    for(uint64_t n=0;n<cases;++n){core.plugin->run_frame({});require(!sink.done,"early finish");const auto row=observe(*core.plugin);require(count(row)==n+1,"fresh guest RAM sequence");out.write(row.data(),row.size());}
    core.plugin->run_frame({});require(sink.done,"missing guest completion");out.write("LINKDONE",8);word(out,core.plugin->get_frame_count());word(out,core.plugin->get_cycle_count());out.close();require(bool(out),"output close");core.plugin->set_test_sink(nullptr);
    std::cout<<cases<<" real linked-memory records; live LLbit/full rollback, S030 import, 61 atomic flag rejections PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
