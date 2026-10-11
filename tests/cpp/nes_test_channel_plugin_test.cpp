#include "nes_plugin_fixture.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
using emulator_test::require;
struct Sink final:emu::ITestSink{
 std::string output,adapter;int finishes=0,last=0;
 void write(const char* p,size_t n)override{output.append(p,n);}
 void finish(int32_t status)override{++finishes;last=status;}
 void note(const char* key,const char* value)override{require(std::string(key)=="adapter","unexpected note");adapter=value;}
 void request_reset()override{throw std::runtime_error("unexpected reset");}
 void blob(const char*,const uint8_t*,size_t)override{throw std::runtime_error("unexpected blob");}
};
// Original NROM programs execute real CPU stores. No guest pass/fail branch.
static std::vector<uint8_t> image(unsigned status,bool signature,bool port){
 std::vector<uint8_t> rom(16+16384,0xea);const uint8_t head[]={ 'N','E','S',26,1,0,0,0,1,0,0,0,0,0,0,0};std::copy(std::begin(head),std::end(head),rom.begin());size_t at=16;
 rom[at++]=0x78;rom[at++]=0xd8;
 auto store=[&](uint16_t address,uint8_t value){for(uint8_t b:{uint8_t(0xa9),value,uint8_t(0x8d),uint8_t(address),uint8_t(address>>8)})rom[at++]=b;};
 if(port)store(0x401f,0);
 store(0x6000,0x80);if(signature){store(0x6001,0xde);store(0x6002,0xb0);store(0x6003,0x61);}
 store(0x6004,'x');store(0x6005,0);store(0x6000,uint8_t(status));
 if(port){for(char c:std::string("REG observed=0"))store(0x401e,uint8_t(c));
  // Emit A after a real RAM load, then convert the bounded 0/7 low digit.
  for(uint8_t b:{uint8_t(0xad),uint8_t(0x00),uint8_t(0x60),uint8_t(0x18),uint8_t(0x69),uint8_t(0x30),uint8_t(0x8d),uint8_t(0x1e),uint8_t(0x40)})rom[at++]=b;
  store(0x401e,10);store(0x401f,1);}
 const auto pc=uint16_t(0x8000+at-16);rom[at++]=0x4c;rom[at++]=uint8_t(pc);rom[at++]=uint8_t(pc>>8);rom[16+0x3ffc]=0;rom[16+0x3ffd]=0x80;return rom;
}
int main(int argc,char** argv){try{
 require(argc==3,"production NES plugin and capture directory required");std::filesystem::create_directories(argv[2]);unsigned cases=0;
 for(unsigned status:{0u,7u})for(bool signature:{false,true})for(bool port:{false,true}){
  emulator_test::LoadedCore core(argv[1],image(status,signature,port));Sink sink;
  require(std::string(core.plugin->test_channels())=="port,blargg6000","actual driver metadata");require(core.plugin->set_test_sink(&sink),"attach result sink");core.netplay().run_frame_netplay(0,0);
  if(port)require(sink.output==std::string("REG observed=0")+(status?"7\n":"0\n") && sink.adapter.empty() && sink.finishes==1 && sink.last==-1,"port precedence and unknown completion");
  else if(signature)require(sink.adapter=="blargg6000" && sink.finishes==0 && sink.output==std::string("LOG x\n")+(status?"END 0/1 code=7\n":"END 1/1\n"),"actual guest signature/status adapter");
  else require(sink.output.empty() && sink.adapter.empty() && sink.finishes==0,"status alone cannot fabricate verdict");
  const auto ram=core.plugin->read_memory(0x6000);require(ram==status,"actual status RAM byte");
  std::ofstream capture(std::filesystem::path(argv[2])/(std::to_string(cases)+".capture"));
  capture<<"# original NROM status="<<status<<" signature="<<signature<<" port="<<port<<"\n";
  capture<<"REG ram6000="<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(ram)<<std::dec<<"\n";
  capture<<"META adapter="<<sink.adapter<<" finishes="<<sink.finishes<<" status="<<sink.last<<"\n"<<sink.output;
  require(bool(capture),"actual capture file write");
  core.plugin->set_test_sink(nullptr);++cases;
 }
 std::cout<<cases<<" original production NES adapter/port captures PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
