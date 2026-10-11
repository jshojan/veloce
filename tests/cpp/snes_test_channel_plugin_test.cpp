#include "nes_plugin_fixture.hpp"
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
using emulator_test::require;
struct Sink final : emu::ITestSink {
    std::string output, notes;
    std::map<std::string,std::vector<uint8_t>> blobs;
    int finishes=0, status=0, resets=0;
    void write(const char* p,size_t n) override { output.append(p,n); }
    void finish(int32_t s) override { ++finishes; status=s; }
    void note(const char* k,const char* v) override { notes+=std::string(k)+"="+v+"\n"; }
    void blob(const char* k,const uint8_t* p,size_t n) override { blobs[k]=std::vector<uint8_t>(p,p+n); }
    void request_reset() override { ++resets; }
};
struct Program {
    std::vector<uint8_t> rom=std::vector<uint8_t>(32768,0xea);
    size_t at=0;
    Program() {
        for(size_t n=0x7fc0;n<rom.size();++n) rom[n]=0;
        std::fill(rom.begin()+0x7fc0,rom.begin()+0x7fd5,' ');
        rom[0x7fd5]=0x20; rom[0x7fd6]=2; rom[0x7fd7]=5; rom[0x7fd8]=3;
        rom[0x7fdc]=0xff; rom[0x7fdd]=0xff;
        rom[0x7ffc]=0; rom[0x7ffd]=0x80;
        byte(0x78); byte(0xd8); // reset starts with 8-bit A
    }
    void byte(uint8_t v) { rom[at++]=v; }
    void store(uint32_t a,uint8_t v) {
        byte(0xa9); byte(v); byte(0x8f); // LDA #byte; STA long
        byte(a); byte(a>>8); byte(a>>16);
    }
    void text(const std::string& s,uint32_t port=0x0021fe) { for(auto c:s) store(port,c); }
    void block(uint8_t verdict) {
        store(0x006000,0x80); store(0x006001,0xde); store(0x006002,0xb0); store(0x006003,0x61);
        store(0x006000,verdict);
    }
    void color(uint16_t v) { store(0x002121,0); store(0x002122,v); store(0x002122,v>>8); }
    void idle() { byte(0x80); byte(0xfe); }
};
int main(int argc,char** argv) { try {
    require(argc==3,"SNES plugin and capture directory required");
    std::filesystem::create_directories(argv[2]); unsigned count=0;
    auto run=[&](Program& p,const std::string& output,const std::string& notes,int finishes=0,bool stp=false) {
        if(stp) p.byte(0xdb); else p.idle();
        emulator_test::LoadedCore core(argv[1],p.rom); Sink sink;
        require(core.plugin->set_test_sink(&sink),"attach production sink");
        require(std::string(core.plugin->test_channels())=="port,blargg6000,stp,sram,spcport","channel metadata");
        core.plugin->run_frame({});
        if(sink.output!=output || sink.notes!=notes || sink.finishes!=finishes || sink.resets!=0 || (finishes && sink.status!=-1)) {
            std::cerr<<"case="<<count<<" output="<<sink.output<<" notes="<<sink.notes<<" finishes="<<sink.finishes<<'\n';
            throw std::runtime_error("production SNES capture mismatch");
        }
        if(stp && !notes.empty()) {
            require(sink.blobs.count("cgram") && sink.blobs["cgram"].size()==512,"actual CGRAM blob");
            require(sink.blobs.count("sram") && sink.blobs["sram"].size()==8192,"actual cartridge RAM blob");
            for(const auto& pair:sink.blobs) {
                std::ofstream file(std::filesystem::path(argv[2])/(std::to_string(count)+"."+pair.first),std::ios::binary);
                file.write(reinterpret_cast<const char*>(pair.second.data()),pair.second.size());
                require(bool(file),"write actual memory capture");
            }
        } else require(sink.blobs.empty(),"unexpected memory blob");
        std::ofstream file(std::filesystem::path(argv[2])/(std::to_string(count)+".capture"));
        file<<notes<<"META finishes="<<sink.finishes<<" status="<<sink.status<<'\n'<<output;
        require(bool(file),"write actual channel capture"); core.plugin->set_test_sink(nullptr); ++count;
    };
    for(uint32_t port:{0x0021feu,0x8021feu,0x3f21feu,0xbf21feu}) {
        Program p;p.text("REG original=1234\n",port);p.store(port+1,1);p.block(0);
        run(p,"REG original=1234\n","",1);
    }
    {Program p;p.text("ignored",0x7e21fe);run(p,"","");}
    for(uint8_t code:{0,7}) {Program p;p.block(code);run(p,code?"END 0/1 code=7\n":"END 1/1\n","adapter=blargg6000\n");}
    {Program p;p.store(0x006000,0);run(p,"","");}
    {Program p;p.text("END 1/1\n");p.color(0x001f);run(p,"END 1/1\n","",0,true);}
    for(uint16_t color:{0x7c00,0x001f,0x1234}) {
        Program p;p.color(color);char details[40];std::snprintf(details,sizeof(details),"00:%04x cgram0=%04x",unsigned(0x8000+p.at),color);
        run(p,color==0x7c00?"END 1/1\n":color==0x001f?"END 0/1\n":"",std::string("adapter=stp\nstp=")+details+"\n",color==0x1234?1:0,true);
    }
    {Program p;p.store(0x7e3456,0xa5);p.idle();emulator_test::LoadedCore core(argv[1],p.rom);core.plugin->run_frame({});std::vector<uint8_t> before,after;require(core.plugin->save_state(before),"snapshot before late attach");Sink sink;core.plugin->set_test_sink(&sink);require(core.plugin->save_state(after) && before==after,"late sink attachment changed hardware state");core.plugin->set_test_sink(nullptr);}
    std::cout<<count<<" original production SNES captures PASS\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
