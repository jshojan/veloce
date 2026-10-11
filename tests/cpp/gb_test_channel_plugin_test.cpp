#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using emulator_test::require;
struct Sink final:emu::ITestSink{
 std::string output,adapter;
 void write(const char* p,size_t n)override{output.append(p,n);}
 void finish(int32_t)override{throw std::runtime_error("unexpected direct finish");}
 void note(const char* key,const char* value)override{require(std::string(key)=="adapter","unexpected note");adapter=value;}
 void blob(const char*,const uint8_t*,size_t)override{throw std::runtime_error("unexpected blob");}
 void request_reset()override{throw std::runtime_error("unexpected reset");}
};
struct Program{
 std::vector<uint8_t> rom=std::vector<uint8_t>(32768,0);size_t at=0x150;
 Program(bool micro=false,bool ram=false){rom[0x100]=0xc3;rom[0x101]=0x50;rom[0x102]=1;const std::string title=micro?"microtest":"owned_fixture";std::copy(title.begin(),title.end(),rom.begin()+0x134);rom[0x147]=ram?8:0;rom[0x149]=ram?2:0;byte(0xf3);}
 void byte(uint8_t b){rom[at++]=b;}
 void store(uint16_t a,uint8_t b){byte(0x3e);byte(b);byte(0xea);byte(uint8_t(a));byte(uint8_t(a>>8));}
 void serial(const std::string& s){for(char c:s){store(0xff01,uint8_t(c));store(0xff02,0x81);}}
 void done(){byte(0x18);byte(0xfe);uint8_t check=0;for(unsigned i=0x134;i<=0x14c;++i)check=uint8_t(check-rom[i]-1);rom[0x14d]=check;}
};
int main(int argc,char** argv){try{
 require(argc==3,"GB production plugin and observation directory required");std::filesystem::create_directories(argv[2]);unsigned count=0;
 auto execute=[&](Program& p,const std::string& adapter,const std::string& expected,unsigned frames=1){p.done();emulator_test::LoadedCore core(argv[1],p.rom);Sink sink;require(core.plugin->set_test_sink(&sink),"attach sink");require(std::string(core.plugin->test_channels())=="serial,mooneye,a000,hram,blarggtext","actual channel metadata");for(unsigned f=0;f<frames;++f)core.plugin->run_frame({});if(sink.adapter!=adapter || sink.output!=expected){std::cerr<<"case "<<count<<" adapter="<<sink.adapter<<" expected="<<adapter<<"\noutput: "<<sink.output<<"\nexpected: "<<expected<<"\n";throw std::runtime_error("actual guest adapter observations");}std::ofstream out(std::filesystem::path(argv[2])/(std::to_string(count)+".capture"));out<<"# original GB fixture adapter="<<sink.adapter<<"\n"<<sink.output;require(bool(out),"write actual observations");core.plugin->set_test_sink(nullptr);++count;};
 for(unsigned pattern=0;pattern<3;++pattern){Program p;const uint8_t op[]={6,14,22,30,38,46},pass[]={3,5,8,13,21,34};for(unsigned n=0;n<6;++n){p.byte(op[n]);p.byte(pattern==0?pass[n]:pattern==1?0x42:0);}p.byte(0x40);execute(p,pattern==2?"":"mooneye",pattern==0?"REG b=03 c=05 d=08 e=0d h=15 l=22\nEND 1/1\n":pattern==1?"REG b=42 c=42 d=42 e=42 h=42 l=42\nEND 0/1 code=66\n":"");}
 for(bool armed:{false,true})for(bool pass:{false,true}){Program p(armed);p.store(0xff80,pass?0x2a:0x15);p.store(0xff81,0x2a);p.store(0xff82,pass?1:255);execute(p,armed?"hram":"",armed?(pass?"CHECK 1 PASS hram exp=2a got=2a\nEND 1/1\n":"CHECK 1 FAIL hram exp=2a got=15\nEND 0/1\n"):"");}
 for(bool pass:{false,true}){Program p;p.serial(pass?"Passed\n":"Failed #7\n");execute(p,"blarggtext",pass?"Passed\nEND 1/1\n":"Failed #7\nEND 0/1 code=7\n");}
 // An explicit serial stream owns its data, including words resembling legacy verdicts.
 {Program p(true);p.serial("VELOCE 1 gb owned_fixture\nLOG Passed\nEND observations_complete\n");p.store(0xff82,1);execute(p,"","VELOCE 1 gb owned_fixture\nLOG Passed\nEND observations_complete\n");}
 for(bool pass:{false,true}){Program p(false,true);p.store(0xa000,0x80);p.store(0xa001,0xde);p.store(0xa002,0xb0);p.store(0xa003,0x61);p.store(0xa004,'x');p.store(0xa005,0);p.byte(1);p.byte(0xff);p.byte(0xff);const auto loop=p.at;p.byte(0x0b);p.byte(0x78);p.byte(0xb1);p.byte(0x20);p.byte(uint8_t(int(loop)-int(p.at)-1));p.store(0xa000,pass?0:7);execute(p,"a000",pass?"LOG x\nEND 1/1\n":"LOG x\nEND 0/1 code=7\n",40);}
 std::cout<<count<<" original production GB adapter captures PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
