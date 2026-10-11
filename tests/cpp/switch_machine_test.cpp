#include "machine.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Bus final : nx::Memory {
 uint32_t word=0;
 bool read(uint64_t address,unsigned size,uint64_t& value,bool instruction) override {if(address || size!=4 || !instruction)return false;value=word;return true;}
 bool write(uint64_t,unsigned,uint64_t) override{return false;}
};
struct Answer{uint64_t value=0;unsigned flags=0;};
static Answer ripple(uint64_t a,uint64_t b,unsigned width,bool subtract,unsigned carry){
 Answer out;unsigned sign_in=0;
 for(unsigned bit=0;bit<width;++bit){const unsigned x=(a>>bit)&1,y=((b>>bit)&1)^unsigned(subtract);if(bit+1==width)sign_in=carry;out.value|=uint64_t(x^y^carry)<<bit;carry=(x&y)|(x&carry)|(y&carry);}
 out.flags=unsigned(bool(out.value&(uint64_t(1)<<(width-1))))*8+unsigned(!out.value)*4+carry*2+(sign_in^carry);return out;
}
static uint32_t u32(const std::vector<uint8_t>& bytes,size_t at){return uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;}
static void set32(std::vector<uint8_t>& bytes,size_t at,uint32_t value){for(unsigned n=0;n<4;++n)bytes[at+n]=uint8_t(value>>(n*8));}
int main(int argc,char** argv){try{
 require(argc==2,"original NSO required");Bus bus;nx::Aarch64 cpu(bus);
 const uint32_t words[]={0x2b010002,0x6b010002,0x1a010002,0x3a010002,0x5a010002,0x7a010002};
 uint64_t cases=0;
 for(unsigned width:{32u,64u})for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){
  const uint64_t left=uint64_t(a)*0x0101010101010101ULL,right=uint64_t(b)*0x0101010101010101ULL;
  for(unsigned form=0;form<6;++form){
   const bool subtract=form==1 || form==4 || form==5,flagged=form==0 || form==1 || form==3 || form==5;
   Answer answers[2];for(unsigned carry=0;carry<2;++carry)answers[carry]=ripple(left,right,width,subtract,form<2?unsigned(subtract):carry);
   bus.word=words[form]|(width==64?0x80000000u:0);
   for(unsigned flags=0;flags<16;++flags){
    cpu.state={};cpu.state.x[0]=left;cpu.state.x[1]=right;cpu.state.x[7]=0xfedcba9876543210ULL;cpu.state.sp=0xabcdef01;cpu.state.nzcv=flags;
    const auto expected=answers[(flags>>1)&1];require(cpu.step(),"defined arithmetic failed");
    require(cpu.state.x[2]==expected.value && cpu.state.nzcv==(flagged?expected.flags:flags) && cpu.state.x[0]==left && cpu.state.x[1]==right && cpu.state.x[7]==0xfedcba9876543210ULL && cpu.state.sp==0xabcdef01 && cpu.state.pc==4 && cpu.state.steps==1,"all arithmetic/NZCV/source preservation cases");++cases;
   }
  }
 }
 // Every immediate, shift choice, width and flag/subtract form at a carry boundary.
 for(unsigned width:{32u,64u})for(unsigned shift=0;shift<2;++shift)for(unsigned form=0;form<4;++form)for(unsigned imm=0;imm<4096;++imm){
  cpu.state={};cpu.state.x[0]=width==64?~uint64_t(0):0xffffffffu;cpu.state.nzcv=9;
  const bool subtract=form&2,flags=form&1;
  bus.word=0x11000002|(width==64?0x80000000u:0)|(subtract?0x40000000u:0)|(flags?0x20000000u:0)|(shift<<22)|(imm<<10);
  const auto expected=ripple(cpu.state.x[0],uint64_t(imm)<<(shift?12:0),width,subtract,subtract);
  require(cpu.step() && cpu.state.x[2]==expected.value && cpu.state.nzcv==(flags?expected.flags:9),"immediate full domain");++cases;
 }
 // Bit selection oracle for every legal shift/rotate amount and logical form.
 for(unsigned width:{32u,64u})for(unsigned pattern=0;pattern<16;++pattern)for(unsigned shift=0;shift<4;++shift)for(unsigned amount=0;amount<width;++amount)for(unsigned op=0;op<4;++op)for(unsigned invert=0;invert<2;++invert){
  const uint64_t mask=width==64?~uint64_t(0):0xffffffffu,right=uint64_t(pattern*17)*0x0101010101010101ULL,left=(~right)^0x8000000100000000ULL;
  uint64_t transformed=0;
  for(unsigned bit=0;bit<width;++bit){unsigned source=0;bool valid=true;
   if(shift==0){valid=bit>=amount;source=bit-amount;}
   else if(shift==1){source=bit+amount;valid=source<width;}
   else if(shift==2)source=std::min(bit+amount,width-1);
   else source=(bit+amount)%width;
   if(valid && ((right>>source)&1))transformed|=uint64_t(1)<<bit;
  }
  if(invert)transformed^=mask;
  const uint64_t expected=((op==0 || op==3)?left&transformed:op==1?left|transformed:left^transformed)&mask;
  cpu.state={};cpu.state.x[0]=left;cpu.state.x[1]=right;cpu.state.nzcv=15;
  bus.word=0x0a010002|(width==64?0x80000000u:0)|(op<<29)|(shift<<22)|(invert<<21)|(amount<<10);
  require(cpu.step() && cpu.state.x[2]==expected && cpu.state.nzcv==(op==3?unsigned(bool(expected&(uint64_t(1)<<(width-1))))*8+unsigned(!expected)*4:15),"all logical shifts/rotates/invert/flags");++cases;
 }
 for(unsigned width:{32u,64u})for(unsigned shift=0;shift<width;shift+=16)for(unsigned op:{0u,2u,3u})for(unsigned imm=0;imm<65536;++imm){
  const uint64_t mask=width==64?~uint64_t(0):0xffffffffu,seed=0xa5a55a5a1234fedcULL;
  const uint64_t lane=uint64_t(imm)<<shift,expected=(op==0?~lane:op==2?lane:(seed&~(uint64_t(65535)<<shift))|lane)&mask;
  cpu.state={};cpu.state.x[2]=seed;cpu.state.nzcv=15;bus.word=0x12800002|(width==64?0x80000000u:0)|(op<<29)|(shift/16<<21)|(imm<<5);
  require(cpu.step() && cpu.state.x[2]==expected && cpu.state.nzcv==15,"MOVN/Z/K full halfword domains");++cases;
 }
 for(unsigned flags=0;flags<16;++flags)for(unsigned condition=0;condition<16;++condition){
  const bool n=flags&8,z=flags&4,c=flags&2,v=flags&1;
  const bool conditions[]={z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,n==v,n!=v,!z&&n==v,z||n!=v,true,true};
  cpu.state={};cpu.state.nzcv=flags;bus.word=0x54000000|(2u<<5)|condition;
  require(cpu.step() && cpu.state.pc==(conditions[condition]?8u:4u) && cpu.state.nzcv==flags,"all B.cond/NZCV encodings");
 }
 // XZR and SP are different operands depending on the instruction family.
 cpu.state={};cpu.state.sp=0x1000;bus.word=0x910043ff;require(cpu.step() && cpu.state.sp==0x1010,"ADD SP alias");
 cpu.state={};cpu.state.sp=0x1000;bus.word=0x8b1f03e2;require(cpu.step() && !cpu.state.x[2] && cpu.state.sp==0x1000,"shifted arithmetic uses XZR");
 for(uint32_t invalid:{0x0bc10002u,0x0b018002u,0x52800000u|(2u<<21),0x12a00002u|(1u<<29),0xffffffffu,0xd5033f9fu}){
  cpu.state={};cpu.state.x[2]=0x1234;bus.word=invalid;require(!cpu.step() && cpu.state.fault==3 && cpu.state.fault_word==invalid && !cpu.state.pc && cpu.state.x[2]==0x1234,"unsupported encoding diagnostics atomicity");
 }
 cpu.state={};cpu.state.pc=1;require(!cpu.step() && cpu.state.fault==1 && cpu.state.fault_address==1,"instruction alignment diagnostic");
 cpu.state={};cpu.state.pc=4;require(!cpu.step() && cpu.state.fault==2,"instruction fetch diagnostic");
 std::ifstream file(argv[1],std::ios::binary);require(file.good(),"original archive missing");std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
 nx::Machine machine;require(machine.load(rom.data(),rom.size()),"native uncompressed NSO load");
 const auto initial_crc=machine.crc;std::vector<uint8_t> initial,after,replay;require(machine.save(initial) && initial.size()==nx::Machine::state_size,"state extent");
 auto rejected=[&](std::vector<uint8_t> bad){require(!machine.load(bad.data(),bad.size()) && machine.crc==initial_crc && machine.save(after) && after==initial,"bad executable load atomicity");};
 for(unsigned bit=0;bit<32;++bit){auto bad=rom;set32(bad,12,1u<<bit);rejected(std::move(bad));}
 for(auto mutation:std::initializer_list<std::pair<size_t,uint32_t>>{{4,1},{8,1},{16,255},{16,0xffffffffu},{20,1},{24,0xffffffffu},{36,0},{36,1},{40,0xffffffffu},{52,0x3ff000},{56,0xffffffffu},{60,0xffffffffu},{28,0xffffffffu}}){
  auto bad=rom;set32(bad,mutation.first,mutation.second);if(mutation.first==28)set32(bad,44,1);rejected(std::move(bad));
 }
 // Empty optional sections have no bytes to dereference or compare on restore.
 auto empty=rom;set32(empty,32,0);set32(empty,36,0);set32(empty,40,0);nx::Machine no_ro;
 require(no_ro.load(empty.data(),empty.size()) && no_ro.save(after) && no_ro.restore(after),"empty RO segment state roundtrip");
 auto truncated=rom;truncated.resize(255);rejected(truncated);truncated=rom;truncated.pop_back();rejected(truncated);
 uint64_t sentinel=0x12345678;require(!machine.read(0x400000,8,sentinel) && sentinel==0x12345678 && !machine.write(0,4,0) && !machine.write(0x10000,1,0),"map bounds and read-only segments");
 const auto data_address=u32(rom,52);require(machine.write(data_address,8,0x123456789abcdef0ULL) && machine.read(data_address,8,sentinel) && sentinel==0x123456789abcdef0ULL,"native data segment little endian");
 require(machine.read(data_address+16,8,sentinel) && !sentinel,"native BSS zero init");
 require(!machine.read(0x3f0000,4,sentinel,true) && machine.write(0x3f0000,8,0xabc),"stack writable not executable");
 for(unsigned n=0;n<200;++n)require(machine.cpu.step(),"original startup");
 require(machine.save(initial),"pending execution save");
 for(unsigned n=0;n<1000;++n)require(machine.cpu.step(),"original execution");
 require(machine.save(after) && machine.restore(initial),"pending restore");
 for(unsigned n=0;n<1000;++n)require(machine.cpu.step(),"original replay");
 require(machine.save(replay) && replay==after,"complete original execution replay");
 for(size_t at:{size_t(32),size_t(312),size_t(320),size_t(328),size_t(336)}){auto bad=after;bad[at]=1;require(!machine.restore(bad) && machine.save(replay) && replay==after,"invalid state fields reject atomically");}
 for(size_t at:{size_t(360),size_t(360+0x10000)}){auto bad=after;bad[at]^=1;require(!machine.restore(bad) && machine.save(replay) && replay==after,"immutable segment restore rejection");}
 auto bad=after;bad.pop_back();require(!machine.restore(bad) && machine.save(replay) && replay==after,"invalid state extent");
 machine.reset();require(machine.cpu.state.pc==0 && machine.cpu.state.sp==nx::Machine::ram_size && !machine.done && !machine.cpu.state.fault && !machine.frames,"reset architecture");
 std::cout<<cases<<" integer cases; conditions, SP/XZR, native loader/map, diagnostics and complete replay/atomic state guards PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
