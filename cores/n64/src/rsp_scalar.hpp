#pragma once
#include "sp_interface.hpp"
#include "dp_interface.hpp"
#include "rsp_vector.hpp"
#include <array>
#include <cstdint>
namespace n64 {
// Scalar RSP execution and vector transfers, sharing SP controls and 8 KiB memory.
// Unimplemented vector arithmetic reports an explicit emulator block, never a fake BREAK.
class RspScalar {
public:
    std::array<uint32_t,32> gpr{};
    RspVector vectors;
    uint32_t next_pc=4,blocked_word=0;
    uint64_t instructions=0;
    bool delay_slot=false,blocked=false;
    std::array<uint32_t,1024> imem_words{};
    std::array<uint64_t,1024> imem_tags{};
    using State=std::array<uint64_t,37>;
    void reset(){*this=RspScalar{};}
    void redirect(uint32_t pc){next_pc=(pc+4)&4092;delay_slot=blocked=false;blocked_word=0;}
    State snapshot() const {State s{};for(unsigned n=0;n<32;++n)s[n]=gpr[n];s[32]=next_pc;s[33]=instructions;s[34]=delay_slot;s[35]=blocked;s[36]=blocked_word;return s;}
    bool restore(const State& s){
        if(s[0] || (s[32]&~4092ull) || s[34]>1 || s[35]>1 || s[36]>0xffffffffu)return false;
        for(unsigned n=0;n<32;++n)if(s[n]>0xffffffffu)return false;
        for(unsigned n=0;n<32;++n)gpr[n]=uint32_t(s[n]);
        next_pc=uint32_t(s[32]);instructions=s[33];delay_slot=bool(s[34]);blocked=bool(s[35]);blocked_word=uint32_t(s[36]);imem_tags.fill(0);return true;
    }
    bool step(SpInterface& control,DpInterface& dp,std::array<uint8_t,8192>& memory,std::vector<uint8_t>& ram){
        if(control.status&1)return false;
        const uint32_t pc=control.pc,old_next=next_pc;
        const unsigned slot=pc>>2;
        if(imem_tags[slot]!=control.imem_generation){
            uint32_t word=0;for(unsigned n=0;n<4;++n)word=(word<<8)|memory[4096+((pc+n)&4095)];
            imem_words[slot]=word;imem_tags[slot]=control.imem_generation;
        }
        const uint32_t ins=imem_words[slot];
        if(!ins){
            // A literal NOP is common in microcode delay slots. It still
            // advances the RSP PC, instruction count and single-step HALT.
            control.pc=old_next;next_pc=(old_next+4)&4092;delay_slot=false;
            gpr[0]=0;blocked=false;blocked_word=0;++instructions;
            if(control.status&32)control.status|=1;
            return true;
        }
        const unsigned op=ins>>26,rs=(ins>>21)&31,rt=(ins>>16)&31,rd=(ins>>11)&31,sa=(ins>>6)&31,fn=ins&63;
        const uint32_t a=gpr[rs],b=gpr[rt],imm=(ins&0xffff)|((ins&0x8000)?0xffff0000u:0);
        control.pc=old_next;next_pc=(old_next+4)&4092;const bool old_delay=delay_slot;delay_slot=false;
        auto put=[&](unsigned reg,uint32_t value){if(reg)gpr[reg]=value;};
        auto signed_value=[](uint32_t x)->int64_t{return x<0x80000000u?int64_t(x):int64_t(x)-0x100000000ll;};
        auto asr=[](uint32_t value,unsigned amount){return amount?(value>>amount)|((value&0x80000000u)?(~uint32_t(0)<<(32-amount)):0):value;};
        auto branch=[&](bool taken,uint32_t target){delay_slot=true;if(taken)next_pc=target&4092;};
        auto load=[&](unsigned bytes,bool sign){uint32_t v=0;const uint32_t address=a+imm;for(unsigned n=0;n<bytes;++n)v=(v<<8)|memory[(address+n)&4095];if(sign && bytes<4 && (v&(1u<<(bytes*8-1))))v|=~uint32_t(0)<<(bytes*8);put(rt,v);};
        auto store=[&](unsigned bytes){const uint32_t address=a+imm;for(unsigned n=0;n<bytes;++n)memory[(address+n)&4095]=uint8_t(b>>(8*(bytes-1-n)));};
        bool supported=true;
        switch(op){
        case 0:
            switch(fn){
            case 0:put(rd,b<<sa);break;case 2:put(rd,b>>sa);break;case 3:put(rd,asr(b,sa));break;
            case 4:put(rd,b<<(a&31));break;case 6:put(rd,b>>(a&31));break;case 7:put(rd,asr(b,a&31));break;
            case 8:branch(true,a);break;case 9:put(rd,(pc+8)&4095);branch(true,a);break;
            case 13:control.status|=3;if(control.status&64)control.interrupt=1;break;
            case 32:case 33:put(rd,a+b);break;case 34:case 35:put(rd,a-b);break;
            case 36:put(rd,a&b);break;case 37:put(rd,a|b);break;case 38:put(rd,a^b);break;case 39:put(rd,~(a|b));break;
            case 42:put(rd,signed_value(a)<signed_value(b));break;case 43:put(rd,a<b);break;
            default:supported=false;break;
            }break;
        case 1:
            if(rt!=0 && rt!=1 && rt!=16 && rt!=17){supported=false;break;}
            if(rt&16)put(31,(pc+8)&4095);
            branch(bool(a&0x80000000u)!=bool(rt&1),pc+4+imm*4);break;
        case 2:case 3:if(op==3)put(31,(pc+8)&4095);branch(true,ins<<2);break;
        case 4:branch(a==b,pc+4+imm*4);break;case 5:branch(a!=b,pc+4+imm*4);break;
        case 6:branch(signed_value(a)<=0,pc+4+imm*4);break;case 7:branch(signed_value(a)>0,pc+4+imm*4);break;
        case 8:case 9:put(rt,a+imm);break;case 10:put(rt,signed_value(a)<signed_value(imm));break;case 11:put(rt,a<imm);break;
        case 12:put(rt,a&(ins&65535));break;case 13:put(rt,a|(ins&65535));break;case 14:put(rt,a^(ins&65535));break;case 15:put(rt,ins<<16);break;
        case 16:
            if(rd>=16 || (ins&0x7ff) || (rs!=0 && rs!=4)){supported=false;break;}
            if(rs==0){uint64_t value=0;if(rd>=8)put(rt,dp.read(rd-8));else if(!control.read(rd,value))supported=false;else put(rt,uint32_t(value));}
            else supported=rd>=8?dp.write(rd-8,b):control.write(rd,b,ram,memory);
            break;
        case 18:case 50:case 58:supported=vectors.execute(ins,gpr,memory);break;
        case 32:load(1,true);break;case 33:load(2,true);break;case 35:case 39:load(4,false);break;
        case 36:load(1,false);break;case 37:load(2,false);break;
        case 40:store(1);break;case 41:store(2);break;case 43:store(4);break;
        default:supported=false;break;
        }
        gpr[0]=0;
        if(!supported){control.pc=pc;next_pc=old_next;delay_slot=old_delay;blocked=true;blocked_word=ins;control.status|=1;return false;}
        blocked=false;blocked_word=0;++instructions;
        if(control.status&32)control.status|=1;
        return true;
    }
};
}
