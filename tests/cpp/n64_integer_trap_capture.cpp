#include "nes_plugin_fixture.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
struct Sink final:emu::ITestSink{
    bool done=false;
    void write(const char*,size_t)override{throw std::runtime_error("unexpected text result");}
    void finish(int32_t status)override{require(status==-1,"unexpected guest verdict");done=true;}
    void note(const char*,const char*)override{throw std::runtime_error("unexpected note");}
    void blob(const char*,const uint8_t*,size_t)override{throw std::runtime_error("unexpected blob");}
    void request_reset()override{throw std::runtime_error("unexpected reset");}
};
static void word(std::ofstream& out,uint64_t value){for(unsigned n=0;n<8;++n)out.put(char(value>>(56-n*8)));}
static std::array<char,96> observe(emu::IEmulatorPlugin& core){std::array<char,96> bytes;for(unsigned n=0;n<96;++n)bytes[n]=char(core.read_memory(uint16_t(0x2000+n)));return bytes;}
static uint64_t counter(const std::array<char,96>& bytes){uint64_t value=0;for(unsigned n=88;n<96;++n)value=value*256+uint8_t(bytes[n]);return value;}
int main(int argc,char** argv){try{
    require(argc==5,"core, original ROM, file and case count required");uint64_t cases=0;const std::string count=argv[4];const auto parsed=std::from_chars(count.data(),count.data()+count.size(),cases);
    require(parsed.ec==std::errc{} && parsed.ptr==count.data()+count.size() && cases && cases<=20000000,"case range");
    std::ifstream rom_file(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(rom_file),{}};
    emulator_test::LoadedCore core(argv[1],rom);Sink sink;require(core.plugin->set_test_sink(&sink),"native sink");
    std::ofstream out(argv[3],std::ios::binary);require(bool(out),"output file");out.write("V64TRAP1",8);word(out,cases);word(out,12);word(out,core.plugin->get_rom_crc32());
    for(uint64_t n=0;n<cases;++n){core.plugin->run_frame({});require(!sink.done,"early guest completion");const auto bytes=observe(*core.plugin);require(counter(bytes)==n+1,"guest did not yield exactly one fresh RAM record");out.write(bytes.data(),bytes.size());}
    core.plugin->run_frame({});require(sink.done,"missing actual guest completion");out.write("TRAPDONE",8);word(out,core.plugin->get_frame_count());word(out,core.plugin->get_cycle_count());out.close();require(bool(out),"complete result write");
    // The real plugin, complete native checkpoint and fast rollback path must
    // preserve the interrupted output routine and handler-produced registers.
    core.plugin->reset();sink.done=false;core.plugin->run_frame({});require(counter(observe(*core.plugin))==1,"initial trapped record");auto& net=core.netplay();const auto saved=emulator_test::snapshot(net);
    std::vector<std::array<char,96>> records;for(unsigned n=0;n<12;++n){core.plugin->run_frame({});records.push_back(observe(*core.plugin));}const auto after=emulator_test::snapshot(net);
    require(net.load_state_fast(saved.data(),saved.size()),"trap fast restore");for(unsigned n=0;n<12;++n){net.run_frame_netplay_n({0,0,0,0});require(observe(*core.plugin)==records[n],"actual trap RAM replay");}
    require(emulator_test::snapshot(net)==after,"complete trap rollback replay");core.plugin->set_test_sink(nullptr);
    std::cout<<cases<<" actual guest RAM/CP0 records captured; production native state/rollback continuation PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
