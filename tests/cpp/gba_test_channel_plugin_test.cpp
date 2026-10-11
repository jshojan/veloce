#include "nes_plugin_fixture.hpp"
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
using emulator_test::require;
struct Sink final:emu::ITestSink{
 std::string output,notes;int finishes=0,last=0;
 void write(const char* p,size_t n)override{output.append(p,n);}
 void finish(int32_t n)override{++finishes;last=n;}
 void note(const char* k,const char* v)override{notes+=std::string(k)+"="+v+"\n";}
 void blob(const char*,const uint8_t*,size_t)override{throw std::runtime_error("unexpected blob");}
 void request_reset()override{throw std::runtime_error("unexpected reset");}
};
struct Program{
 std::vector<uint8_t> rom=std::vector<uint8_t>(4096,0);size_t at=192;
 Program(){put(0,0xea00002e);rom[0xb2]=0x96;}
 void put(size_t p,uint32_t w){for(unsigned n=0;n<4;++n)rom[p+n]=uint8_t(w>>(n*8));}
 void word(uint32_t w){put(at,w);at+=4;}
 void mov(unsigned r,uint32_t w){word(0xe59f0000|(r<<12));word(0xea000000);word(w);}
 void store(uint32_t a,uint32_t v,bool half=false){mov(0,a);mov(1,v);word(half?0xe1c010b0:0xe5801000);}
 void message(const std::string& text){for(unsigned n=0;n<=text.size();++n){mov(0,0x04fff600+n);mov(1,n<text.size()?uint8_t(text[n]):0);word(0xe5c01000);}store(0x04fff700,0x103,true);}
 void idle(){word(0xeafffffe);}
};
int main(int argc,char** argv){try{
 require(argc==3,"production GBA plugin and capture directory required");std::filesystem::create_directories(argv[2]);unsigned count=0;
 auto run=[&](Program& p,unsigned frames,const std::string& output,const std::string& notes,int finishes=0){p.idle();emulator_test::LoadedCore core(argv[1],p.rom);Sink sink;require(core.plugin->set_test_sink(&sink),"attach sink");require(std::string(core.plugin->test_channels())=="mgba,r12","channel metadata");for(unsigned f=0;f<frames;++f)core.plugin->run_frame({});if(sink.output!=output || sink.notes!=notes || sink.finishes!=finishes || (finishes && sink.last!=-1)){std::cerr<<"case="<<count<<" output="<<sink.output<<" notes="<<sink.notes<<" finishes="<<sink.finishes<<"\n";throw std::runtime_error("actual production channel observations");}std::ofstream file(std::filesystem::path(argv[2])/(std::to_string(count)+".capture"));file<<"# original ARM fixture\n"<<sink.notes<<"META finishes="<<sink.finishes<<" status="<<sink.last<<"\n"<<sink.output;require(bool(file),"write actual observations");core.plugin->set_test_sink(nullptr);++count;};
 for(unsigned verdict:{0u,7u}){Program p;p.mov(12,verdict);const auto pc=uint32_t(0x08000000+p.at);char address[9];std::snprintf(address,sizeof(address),"%08x",pc);std::string expected="REG pc="+std::string(address)+" r12=0000000"+std::to_string(verdict)+"\nLOG m_test_eval epilogue not found; verdict from r12\n"+(verdict?"END 0/1 code=7\n":"END 1/1\n");run(p,12,expected,"adapter=r12\n");}
 for(unsigned verdict:{0u,9u}){Program p;p.mov(13,0x03007f00);p.mov(7,verdict);p.mov(12,verdict?0:7);p.word(0xe92d1fff);p.word(0xe1b0c007);p.word(0xe8bd1fff);char address[9];std::snprintf(address,sizeof(address),"%08x",uint32_t(0x08000000+p.at));std::string expected="REG pc="+std::string(address)+" r12="+(verdict?"00000000":"00000007")+" r7="+(verdict?"00000009":"00000000")+"\nLOG verdict from m_test_eval register r7\n"+(verdict?"END 0/1 code=9\n":"END 1/1\n");run(p,12,expected,"adapter=r12\n");}
 {Program p;p.mov(12,7);p.mov(0,0x08000101);p.word(0xe12fff10);p.put(0x100,0xe7fe);run(p,12,"REG pc=08000100 r12=00000007\nLOG m_test_eval epilogue not found; verdict from r12\nEND 0/1 code=7\n","adapter=r12\n");}
 {Program p;p.message("LOG disabled");run(p,1,"","");}
 {Program p;p.store(0x04fff780,0xc0de,true);p.message("REG byte_stores=abc");p.store(0x04fff782,1,true);run(p,12,"REG byte_stores=abc\n","level=3\n",1);}
 {Program p;p.store(0x04fff780,0xc0de,true);p.message("END 1/1");run(p,12,"END 1/1\n","level=3\n");}
 {Program p;p.store(0x04fff780,0xc0de,true);p.message("END observations_complete");run(p,12,"END observations_complete\n","level=3\n");}
 std::cout<<count<<" original production GBA channel captures PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
