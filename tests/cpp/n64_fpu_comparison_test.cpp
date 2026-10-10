#include "machine.hpp"
#include "fpu_comparisons.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
constexpr uint64_t pc=0xffffffffa0400000ull;
static uint32_t op(unsigned fmt,unsigned predicate,unsigned fs=2,unsigned ft=6,unsigned unused=0){return 0x44000030|(fmt<<21)|(ft<<16)|(fs<<11)|(unused<<6)|predicate;}
static void prepare(n64::Machine& m,uint32_t status=0x24000000){m.cpu.state={};m.cpu.state.pc=pc;m.cpu.state.next_pc=pc+4;m.cpu.state.cp0[12]=status;m.cpu.state.cp0[16]=2;}
static void execute(n64::Machine& m,uint32_t ins){m.cpu.state.pc=pc;m.cpu.state.next_pc=pc+4;require(m.write(pc,4,ins),"instruction write");m.cpu.step();}
// Independent monotone signed key, literal predicate masks and NaN classes.
static n64::FpuComparisons::Result reference(unsigned fmt,unsigned pred,uint64_t a,uint64_t b){
 if(fmt!=16 && fmt!=17)return {false,32};
 const unsigned width=fmt==16?32:64,f=fmt==16?23:52;const uint64_t sign=1ull<<(width-1),fraction=(1ull<<f)-1,exp=(sign-1)-fraction;
 if(width==32){a%=1ull<<32;b%=1ull<<32;}
 const auto nan=[&](uint64_t v){return v%sign>=exp && v% (fraction+1)!=0;};
 unsigned relation=0,causes=0;
 if(nan(a) || nan(b)){relation=1;causes=(pred>=8 || (nan(a) && a/(1ull<<(f-1))%2) || (nan(b) && b/(1ull<<(f-1))%2))?16:0;}
 else if(a==b || (!(a%(sign)) && !(b%(sign))))relation=2;
 else {const auto key=[&](uint64_t x){return (x/sign)?(sign*2-1)-x:x^sign;};relation=key(a)<key(b)?4:0;}
 constexpr unsigned masks[]={0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7};return {bool(masks[pred]&relation),causes};
}
int main(int argc,char** argv){try{
 require(argc==2,"original ROM required");std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& s=m.cpu.state;
 uint64_t pairs=0,encodings=0,controls=0;
 // Every repeated-halfword fraction seed, both signs of both operands and all predicates.
 for(unsigned fmt:{16u,17u})for(unsigned value=0;value<65536;++value)for(unsigned signs=0;signs<4;++signs)for(unsigned pred=0;pred<16;++pred){
  const unsigned f=fmt==16?23:52,eb=fmt==16?8:11;const uint64_t sign=1ull<<(f+eb),expmask=((1ull<<eb)-1)<<f,fraction=(1ull<<f)-1;
  const auto a=(uint64_t(value)*0x1000100010001ull&fraction)|((uint64_t(value)%((1ull<<eb))<<f)&expmask)|((signs&1)?sign:0);
  const auto b=(uint64_t(65535-value)*0x1000100010001ull&fraction)|((uint64_t(value*257u+1)%((1ull<<eb))<<f)&expmask)|((signs&2)?sign:0);
  const auto want=reference(fmt,pred,a,b),got=n64::FpuComparisons::calculate(fmt,pred,a,b);require(got.condition==want.condition && got.causes==want.causes,"raw fraction/exponent/sign predicate reference");++pairs;
 }
 // All exponent pairs on signed zero-fraction encodings, every S/D predicate.
 for(unsigned fmt:{16u,17u}){const unsigned f=fmt==16?23:52,eb=fmt==16?8:11;for(unsigned a=0;a<(1u<<eb);++a)for(unsigned b=0;b<(1u<<eb);++b)for(unsigned pred=0;pred<16;++pred){
  const auto left=uint64_t(a)<<f,right=uint64_t(b)<<f;const auto want=reference(fmt,pred,left,right),got=n64::FpuComparisons::calculate(fmt,pred,left,right);require(want.condition==got.condition && want.causes==got.causes,"all exponent pairs");++pairs;
 }}
 for(unsigned fmt:{16u,17u})for(unsigned fr=0;fr<2;++fr)for(unsigned fs=0;fs<32;++fs)for(unsigned ft=0;ft<32;++ft)for(unsigned pred=0;pred<16;++pred){
  prepare(m,0x20000000|fr*0x4000000);for(unsigned r=0;r<32;++r)s.fpr[r]=fmt==16?0xc000000000000000ull+0x3f800000+r:0x3ff0000000000000ull+r;s.fcr31=0x183f07c;const auto bank=s.fpr,gpr=s.gpr;const auto want=reference(fmt,pred,fmt==16?uint32_t(bank[fs]):bank[fs],fmt==16?uint32_t(bank[ft]):bank[ft]);execute(m,op(fmt,pred,fs,ft));
  if(!fr && ((fs|ft)&1))require((s.cp0[13]&124)==40 && s.fcr31==0x183f07c,"odd FR=0 defined RI convention");
  else require(!m.cpu.exception_taken && s.fcr31==((0x183f07cu&~0x83f000u)|(unsigned(want.condition)<<23)),"all source register encodings/aliases and FR modes");
  require(s.fpr==bank && s.gpr==gpr,"compare never writes any FPR/GPR");++encodings;
 }
 // Literal operand classes: less/equal/greater, signed zero, tiny, infinities,
 // hardware quiet/signaling NaN encodings. Predicate truth masks are independent.
 for(unsigned fmt:{16u,17u})for(unsigned profile=0;profile<10;++profile)for(unsigned pred=0;pred<16;++pred)for(unsigned rm=0;rm<4;++rm)for(unsigned enables=0;enables<32;++enables)for(unsigned flags=0;flags<32;++flags)for(unsigned fs=0;fs<2;++fs)for(unsigned cc=0;cc<2;++cc){
  const unsigned f=fmt==16?23:52,eb=fmt==16?8:11,bias=fmt==16?127:1023;const uint64_t one=uint64_t(bias)<<f,inf=((1ull<<eb)-1)<<f,sign=1ull<<(f+eb);
  const std::array<uint64_t,10> a{0,one,one+1,sign,1,inf,inf+1,inf+(1ull<<(f-1)),inf+1,inf+(1ull<<(f-1))},b{one,one,one,0,0,inf-1,1,1,inf+(1ull<<(f-1)),1};
  constexpr unsigned relations[]={4,2,0,2,0,0,1,1,1,1};const bool condition=bool(pred&relations[profile]);const unsigned causes=profile>=6 && (profile>=7 || pred>=8)?16:0;const bool trap=bool(causes&enables);
  prepare(m);s.fpr[2]=a[profile];s.fpr[6]=b[profile];s.fcr31=rm+enables*128+flags*4+fs*0x1000000+cc*0x800000+0x3f000;s.cp0[13]=0x20000000;const auto bank=s.fpr;const unsigned original=s.fcr31;execute(m,op(fmt,pred));
  const unsigned expected=(original&~0x3f000u)|(causes<<12);require(s.fcr31==(trap?expected:(expected&~0x800000u)|(unsigned(condition)<<23)|(causes<<2)) && m.cpu.exception_taken==trap && s.fpr==bank,"full FCSR product: condition/sticky/causes and trap suppression");
  require((s.cp0[13]&0x3000007c)==(trap?60u:0x20000000u),"FPE clears stale coprocessor error");++controls;
 }
 for(unsigned fmt=16;fmt<32;++fmt)for(unsigned pred=0;pred<16;++pred)for(unsigned unused=0;unused<32;++unused){
  prepare(m,0);s.fcr31=0x180007c;s.fpr.fill(77);execute(m,op(fmt,pred,3,7,unused));require((s.cp0[13]&0x3000007c)==0x1000002c && s.fcr31==0x180007c && s.fpr[3]==77,"CU1 precedes all numeric/unused/odd decode");
  if(fmt>17 || unused){prepare(m);s.fcr31=0x80007c;s.fpr.fill(77);execute(m,op(fmt,pred,2,6,unused));require((s.cp0[13]&124)==60 && s.fcr31==0x82007c && s.fpr[2]==77,"reserved formats/unused field assist convention");}
 }
 for(bool bev:{false,true})for(bool nested:{false,true}){
  prepare(m,0x24000000|(bev?0x400000:0)|(nested?2:0));s.fpr[2]=0x7fc00000;s.fpr[6]=0;s.fcr31=0x800800;s.cp0[13]=0xa0000000;s.cp0[14]=123;s.delay_slot=true;require(m.write(pc,4,op(16,0)),"delay compare");m.cpu.step();require(s.cp0[14]==(nested?123:pc-4) && s.cp0[13]==0x8000003c && s.fcr31==0x810800 && s.pc==(bev?0xffffffffbfc00380ull:0xffffffff80000180ull),"FPE BD/BEV/nested context and CC suppression");
 }
 prepare(m);s.fpr[2]=0x7fc00000;s.fcr31=0x800800;require(m.write(pc,4,op(16,0)),"pending compare");std::vector<uint8_t> saved,after,replay;require(m.save(saved),"comparison save");m.cpu.step();m.save(after);require(m.restore(saved),"comparison restore");m.cpu.step();m.save(replay);require(after==replay,"pending compare exact replay");
 std::cout<<pairs<<" raw/exponent predicate cases; "<<encodings<<" FR/source encodings; "<<controls<<" FCSR products; NaN/denormal, CU1/assist, FPE context and state replay passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
