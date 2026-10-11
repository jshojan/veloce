#include "machine.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <cstdio>
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Bus:vb::Memory{
 uint16_t first=0,tail=0;bool fault=false;
 bool read(uint32_t address,unsigned bytes,uint32_t& value)override{
  if(bytes!=2 || (address!=0x1000 && address!=0x1002) || (fault && address==0x1002))return false;
  value=address==0x1000?first:tail;return true;
 }
 bool write(uint32_t,unsigned,uint32_t)override{return false;}
};
static uint32_t oracle(unsigned sub,uint32_t a,uint32_t b){
 if(sub==8){auto high=b/65536;auto lo=b%65536;return high*65536+(lo%256)*256+lo/256;}
 if(sub==9)return (b%65536)*65536+b/65536;
 if(sub==10){uint32_t result=0;for(unsigned n=0;n<32;++n){result=result*2+a%2;a/=2;}return result;}
 uint32_t factor=a%131072;if(factor>=65536)factor-=131072;
 uint32_t result=0;while(factor){if(factor&1)result+=b;factor>>=1;b<<=1;}return result;
}
static void prepare(vb::V810& cpu){cpu.reset();cpu.state.pc=0x1000;for(unsigned n=1;n<32;++n)cpu.state.gpr[n]=0x1234567u*n;}
struct Sink:emu::ITestSink{
 std::string text;void write(const char* data,size_t size)override{text.append(data,size);}
 void finish(int32_t)override{} void note(const char*,const char*)override{}
 void blob(const char*,const uint8_t*,size_t)override{} void request_reset()override{}
};
int main(int argc,char** argv){try{
 Bus bus;vb::V810 cpu(bus);uint64_t count=0;
 for(unsigned sub:{8u,9u,10u,12u})for(unsigned r1=0;r1<32;++r1)for(unsigned r2=0;r2<32;++r2)
 for(uint32_t pattern:{0u,1u,0x8000u,0x10000u,0x1ffffu,0x80000000u,0x89abcdefu,0xffffffffu})for(unsigned flags=0;flags<16;++flags){
  prepare(cpu);auto& s=cpu.state;if(r1)s.gpr[r1]=pattern;if(r2)s.gpr[r2]=pattern^0x89abcdef;s.sr[5]=0x8000|flags;
  auto before=s;auto want=before.gpr;if(r2)want[r2]=oracle(sub,before.gpr[r1],before.gpr[r2]);
  bus.first=uint16_t(0x3e<<10|r2<<5|r1);bus.tail=sub<<10;cpu.step();
  require(s.gpr==want && s.sr==before.sr && s.pc==0x1004 && s.steps==1 && !s.unsupported,"extension register alias/status");++count;
 }
 bus.first=uint16_t(0x3e<<10|20<<5|25);bus.tail=12<<10;
 for(uint32_t upper:{0u,0x55540000u,0xaaa80000u,0xfffe0000u})for(uint32_t b:{0u,1u,0x80000000u,0x89abcdefu})
 for(uint32_t raw=0;raw<131072;++raw)for(unsigned flags=0;flags<16;++flags){
  prepare(cpu);auto& s=cpu.state;s.gpr[25]=upper|raw;s.gpr[20]=b;s.sr[5]=0x8000|flags;
  auto before=s;auto want=before.gpr;want[20]=oracle(12,s.gpr[25],b);cpu.step();
  require(s.gpr==want && s.sr==before.sr && !s.unsupported,"signed 17-bit MPYHW");++count;
 }
 for(unsigned op:{8u,9u,10u,12u,0x16u,0x1eu})for(uint32_t raw=0;raw<262144;++raw){
  prepare(cpu);auto& s=cpu.state;s.sr[5]=(raw&1023)|((raw>>10)<<12);auto before=s;auto want=before.gpr;auto sr=before.sr;
  if(op<16){bus.first=uint16_t(0x3e<<10|20<<5|25);bus.tail=op<<10;want[20]=oracle(op,s.gpr[25],s.gpr[20]);}
  else{bus.first=op<<10;if(op==0x16)sr[5]&=~0x1000u;else sr[5]|=0x1000u;}
  cpu.step();require(s.gpr==want && s.sr==sr && !s.unsupported && s.pc==0x1000+(op<16?4:2),"whole writable PSW domain");++count;
 }
 auto rejected=[&](uint16_t first,uint16_t tail,bool fault=false){prepare(cpu);auto before=cpu.state;bus.first=first;bus.tail=tail;bus.fault=fault;cpu.step();bus.fault=false;
  require(cpu.state.unsupported && cpu.state.gpr==before.gpr && cpu.state.sr==before.sr && cpu.state.steps==1,"diagnostic mutation");++count;};
 for(unsigned sub:{8u,9u,10u,12u})for(unsigned low=1;low<1024;++low)rejected(uint16_t(0x3e<<10|20<<5|25),uint16_t(sub<<10|low));
 for(unsigned sub=0;sub<64;++sub)if(sub!=8 && sub!=9 && sub!=10 && sub!=12)rejected(0x3e<<10,sub<<10);
 for(unsigned op:{0x16u,0x1eu})for(unsigned low=1;low<1024;++low)rejected(uint16_t(op<<10|low),0);
 rejected(0x3e<<10,8<<10,true);require(cpu.state.pc==0x1000,"tail fetch fault PC");
 require(oracle(12,0x8000,1)==0x8000 && oracle(12,0x10000,1)==0xffff0000 && oracle(12,0xffff,1)==0xffff,"17-bit reference vectors");
 {
  vb::Machine m;Sink sink;m.sink=&sink;
  for(uint32_t raw=0;raw<65536;++raw){uint32_t value=raw*65537;char want[9];std::snprintf(want,sizeof(want),"%08x",value);sink.text.clear();
   require(m.write(0x04000008,4,value) && sink.text==want,"actual word transport");
   require(!m.write(0x04000008,1,value) && !m.write(0x04000008,2,value) && sink.text==want,"transport width rejection");
  }
 }
 if(argc==2){
  std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> image{std::istreambuf_iterator<char>(file),{}};vb::Machine m;require(m.load(image.data(),image.size()),"state fixture load");
  for(unsigned op:{8u,9u,10u,12u,0x16u,0x1eu}){
   m.reset();m.cpu.state.pc=0x05001000;m.cpu.state.gpr[25]=0x10000;m.cpu.state.gpr[20]=0x89abcdef;m.cpu.state.sr[5]=0xff3ff;
   m.write(0x05001000,2,op<16?uint16_t(0x3e<<10|20<<5|25):uint16_t(op<<10));m.write(0x05001002,2,op<16?op<<10:0);
   std::vector<uint8_t> checkpoint,after,replay;require(m.save(checkpoint),"save before instruction");m.cpu.step();require(!m.cpu.state.unsupported && m.save(after),"execute/save extension");
   require(m.restore(checkpoint),"restore before instruction");m.cpu.step();require(m.save(replay) && replay==after,"whole machine extension replay");
  }
 }
 std::cout<<count<<" Nintendo instruction component observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
