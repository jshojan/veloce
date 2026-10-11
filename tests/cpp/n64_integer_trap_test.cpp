#include "vr4300.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok){if(!ok)throw std::runtime_error("integer trap observation mismatch");}
struct Memory final:n64::Memory{
    uint32_t word=0;unsigned writes=0;
    bool read(uint64_t,unsigned bytes,uint64_t& value)override{if(bytes!=4)return false;value=word;return true;}
    bool write(uint64_t,unsigned,uint64_t)override{++writes;return false;}
    bool write_masked(uint64_t,unsigned,uint64_t,uint8_t)override{++writes;return false;}
};
static constexpr uint64_t values[]={0,1,2,0x7fff,0x8000,0x7fffffff,0x80000000,0xffffffff,0x100000000,0x7fffffffffffffffull,0x8000000000000000ull,0xffffffffffffffffull,0xfffffffffffffffeull,0x5555555555555555ull,0xaaaaaaaaaaaaaaaaull,0xfedcba9876543210ull};
// Separate sign/magnitude model rather than the core's sign-bit order transform.
static bool less(uint64_t a,uint64_t b){const bool an=a>>63,bn=b>>63;if(an!=bn)return an;return an?(0-a)>(0-b):a<b;}
static bool condition(unsigned kind,uint64_t a,uint64_t b,bool word=false){
 if(word){const uint32_t x=uint32_t(a),y=uint32_t(b);const int64_t sx=int64_t(x)-(x>=0x80000000u?0x100000000ll:0),sy=int64_t(y)-(y>=0x80000000u?0x100000000ll:0);
  switch(kind){case 0:return sx>=sy;case 1:return x>=y;case 2:return sx<sy;case 3:return x<y;case 4:return x==y;case 5:return x!=y;}throw std::runtime_error("invalid word model");}
 switch(kind){case 0:return !less(a,b);case 1:return a>=b;case 2:return less(a,b);case 3:return a<b;case 4:return a==b;case 5:return a!=b;}throw std::runtime_error("invalid model kind");}
int main(){try{
    Memory memory;n64::VR4300 cpu(memory);cpu.reset(n64::sign32(0x80000400));
    constexpr unsigned functions[]={0x30,0x31,0x32,0x33,0x34,0x36},immediate_types[]={8,9,10,11,12,14};
    uint64_t checks=0;
    auto check=[&](uint32_t instruction,unsigned kind,unsigned rs,unsigned rt,bool immediate,bool delay=false,bool nested=false,bool bev=false,bool word_mode=false){
        auto& s=cpu.state;s={};s.pc=n64::sign32(0x80000404);s.next_pc=s.pc+4;s.delay_slot=delay;
        for(unsigned reg=1;reg<32;++reg)s.gpr[reg]=word_mode?n64::sign32(uint32_t(values[reg%16])):values[reg%16];
        const uint64_t right=immediate?n64::sign16(uint16_t(instruction)):s.gpr[rt];
        const bool trap=condition(kind,s.gpr[rs],right,word_mode);const auto registers=s.gpr;
        s.hi=0x13579bdf2468ace0;s.lo=0xfedcba9876543210;s.cp0[12]=(word_mode?0:128)|(nested?2:0)|(bev?1u<<22:0);
        s.cp0[16]=2;s.cp0[13]=0x80000300;s.cp0[14]=n64::sign32(0x80001000);s.cp0[8]=0x12345678;
        const auto before=s;memory.word=instruction;cpu.step();
        require(s.gpr==registers && s.hi==before.hi && s.lo==before.lo && s.cp0[8]==before.cp0[8] && !memory.writes);
        require(cpu.exception_taken==trap && cpu.instruction_valid && cpu.instruction_word==instruction);
        if(trap){
            if(((s.cp0[13]>>2)&31)!=13)std::cerr<<"instruction "<<std::hex<<instruction<<std::dec<<" expected Trap 13, actual "<<((s.cp0[13]>>2)&31)<<'\n';
            require(((s.cp0[13]>>2)&31)==13 && (s.cp0[13]&0x300)==0x300 && s.cp0[12]==(before.cp0[12]|2));
            require(s.cp0[14]==(nested?before.cp0[14]:before.pc-(delay?4:0)));
            require(bool(s.cp0[13]>>31&1)==(nested?true:delay));
            require(s.pc==n64::sign32(bev?0xbfc00380:0x80000180) && s.next_pc==s.pc+4 && !s.delay_slot);
        }else require(s.cp0[13]==before.cp0[13] && s.cp0[14]==before.cp0[14] && s.cp0[12]==before.cp0[12] && s.pc==before.next_pc && s.next_pc==before.next_pc+4 && !s.delay_slot);
        ++checks;
    };
    for(unsigned kind=0;kind<6;++kind)for(unsigned code=0;code<1024;++code)for(unsigned rs=0;rs<32;++rs)for(unsigned rt=0;rt<32;++rt)
        check((rs<<21)|(rt<<16)|(code<<6)|functions[kind],kind,rs,rt,false);
    for(unsigned kind=0;kind<6;++kind)for(unsigned imm=0;imm<65536;++imm)for(unsigned rs=0;rs<32;++rs)
        check((1u<<26)|(rs<<21)|(immediate_types[kind]<<16)|imm,kind,rs,0,true);
    for(unsigned kind=0;kind<6;++kind)for(bool delay:{false,true})for(bool nested:{false,true})for(bool bev:{false,true})for(unsigned rs=0;rs<32;++rs)for(unsigned rt=0;rt<32;++rt)
        check((rs<<21)|(rt<<16)|(1023u<<6)|functions[kind],kind,rs,rt,false,delay,nested,bev);
    for(unsigned kind=0;kind<6;++kind)for(unsigned code=0;code<1024;++code)for(unsigned rs=0;rs<32;++rs)for(unsigned rt=0;rt<32;++rt)
        check((rs<<21)|(rt<<16)|(code<<6)|functions[kind],kind,rs,rt,false,false,false,false,true);
    for(unsigned kind=0;kind<6;++kind)for(unsigned imm=0;imm<65536;++imm)for(unsigned rs=0;rs<32;++rs)
        check((1u<<26)|(rs<<21)|(immediate_types[kind]<<16)|imm,kind,rs,0,true,false,false,false,true);
    // Execute real taken/untaken ordinary/likely branches before the trap,
    // including annulled slots, rather than only synthesizing delay metadata.
    for(bool word_mode:{false,true})for(unsigned kind=0;kind<6;++kind)for(bool taken:{false,true})for(bool likely:{false,true})for(auto a:values)for(auto b:values){
        auto& s=cpu.state;s={};s.pc=n64::sign32(0x80000400);s.next_pc=s.pc+4;s.cp0[16]=2;s.cp0[12]=word_mode?0:128;
        s.gpr[1]=1;s.gpr[2]=taken?1:2;s.gpr[3]=word_mode?n64::sign32(uint32_t(a)):a;s.gpr[4]=word_mode?n64::sign32(uint32_t(b)):b;
        const auto before=s.gpr;const auto trap=condition(kind,s.gpr[3],s.gpr[4],word_mode);
        memory.word=((likely?20u:4u)<<26)|(1u<<21)|(2u<<16)|2;cpu.step();
        if(likely && !taken)require(s.pc==n64::sign32(0x80000408) && !s.delay_slot && !cpu.exception_taken && s.instructions==1);
        else{require(s.pc==n64::sign32(0x80000404) && s.delay_slot);memory.word=(3u<<21)|(4u<<16)|functions[kind];cpu.step();
            require(cpu.exception_taken==trap && s.gpr==before);
            if(trap)require((s.cp0[13]&0x8000007c)==0x80000034 && s.cp0[14]==n64::sign32(0x80000400) && s.pc==n64::sign32(0x80000180));
            else require(s.pc==n64::sign32(taken?0x8000040c:0x80000408) && !s.delay_slot && !s.cp0[14]);}
        ++checks;
    }
    std::cout<<checks<<" 32/64 register/code/immediate and actual branch/annul/delay/nested/BEV trap observations PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
