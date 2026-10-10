#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
using emulator_test::require;
constexpr uint32_t start=0x02002000;
static int64_t signed32(uint32_t v) { return v&0x80000000 ? int64_t(v)-0x100000000LL:v; }
static const uint32_t patterns[]={0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef};
static void instruction(nds::Machine& m,bool nine,uint32_t word) {
    require(m.write(nine,start,4,word),"test instruction write failed"); (nine ? m.cpu9:m.cpu7).reset(start);
}
static void arithmetic(nds::Machine& m) {
    for(bool nine:{false,true}) for(unsigned op=0;op<16;++op) for(auto a:patterns) for(auto b:patterns)
        for(unsigned flags=0;flags<16;++flags) for(unsigned rd=0;rd<3;++rd) for(bool set:{false,true}) {
            const bool test=op>=8 && op<=11; if(test && !set) continue;
            instruction(m,nine,0xe0000001|(op<<21)|(uint32_t(set)<<20)|(rd<<12));
            auto& cpu=nine ? m.cpu9:m.cpu7; auto& s=cpu.state;
            s.r[0]=a; s.r[1]=b; s.r[2]=0xdeadbeef; s.cpsr=0xdf|(flags<<28)|(nine ? 0x08000000:0); const auto original=s.r;
            bool carry=flags&2,overflow=flags&1; uint32_t result=0;
            if((op>=2 && op<=7) || op==10 || op==11) {
                const bool subtract=op==2 || op==3 || op==6 || op==7 || op==10;
                const bool reverse=op==3 || op==7; const uint32_t x=reverse ? b:a,y=reverse ? a:b;
                const unsigned extra=(op==6 || op==7) ? unsigned(!carry):op==5 ? unsigned(carry):0;
                const int64_t unbounded=subtract ? int64_t(x)-y-extra:int64_t(x)+y+extra;
                const int64_t math=subtract ? signed32(x)-signed32(y)-extra:signed32(x)+signed32(y)+extra;
                result=uint32_t(unbounded); carry=subtract ? unbounded>=0:uint64_t(unbounded)>0xffffffff;
                overflow=math< -0x80000000LL || math>0x7fffffffLL;
            } else switch(op) {
                case 0:case 8:result=a&b;break; case 1:case 9:result=a^b;break;
                case 12:result=a|b;break; case 13:result=b;break; case 14:result=a&~b;break; case 15:result=~b;break;
            }
            const uint32_t psw=(nine ? 0x08000000:0)|(set ? 0xdf|(result&0x80000000)|(result==0 ? 0x40000000:0)|(carry ? 0x20000000:0)|(overflow ? 0x10000000:0):0xdf|(flags<<28));
            cpu.step(); require(!s.unsupported && s.cpsr==psw && s.r[15]==start+4,"ALU flags/retirement wrong");
            for(unsigned reg=0;reg<15;++reg) require(s.r[reg]==(!test && reg==rd ? result:original[reg]),"ALU destination/source alias wrong");
        }
}
static uint64_t product_reference(uint32_t a,uint32_t b,bool sign) {
    uint64_t result=0;
    for(unsigned bit=0;bit<32;++bit) if((b>>bit)&1) result+=uint64_t(a)<<bit;
    if(sign) {
        if(a>>31) result-=uint64_t(b)<<32;
        if(b>>31) result-=uint64_t(a)<<32;
    }
    return result;
}
static void multiplies(nds::Machine& m) {
    const uint64_t seeds[]={0,~uint64_t(0),uint64_t(1)<<63,0x55555555aaaaaaaaULL};
    for(bool nine:{false,true}) for(unsigned op=0;op<6;++op) for(auto a:patterns) for(auto b:patterns)
        for(auto seed:seeds) for(unsigned flags=0;flags<16;++flags) for(bool set:{false,true}) for(unsigned alias=0;alias<3;++alias) {
            const bool wide=op>=2,accumulate=op==1 || op==3 || op==5,sign=op>=4;
            const unsigned lo=wide ? (alias==1 ? 1:4):0,hi=wide ? (alias==2 ? 1:5):(alias==1 ? 1:alias==2 ? 3:4);
            const uint32_t word=0xe0000190|(uint32_t(set)<<20)|(uint32_t(accumulate)<<21)|(hi<<16)|
                (wide ? 0x00800000|(uint32_t(sign)<<22)|(lo<<12):op==1 ? 3<<12:0);
            instruction(m,nine,word); auto& cpu=nine ? m.cpu9:m.cpu7; auto& s=cpu.state;
            s.r[3]=uint32_t(seed); s.r[4]=uint32_t(seed); s.r[5]=uint32_t(seed>>32); s.r[0]=a; s.r[1]=b;
            s.cpsr=0xdf|(flags<<28)|(nine ? 0x08000000:0); const auto original=s.r;
            uint64_t result=product_reference(a,b,sign);
            if(accumulate) result+=wide ? (uint64_t(original[hi])<<32)|original[lo]:original[3];
            if(!wide) result=uint32_t(result);
            const uint32_t want=set ? (s.cpsr&0x3fffffff)|uint32_t((wide ? result>>32:result)&0x80000000)|(result==0 ? 0x40000000:0):s.cpsr;
            cpu.step(); require(!s.unsupported && s.r[15]==start+4,"multiply retirement wrong");
            const uint32_t mask=!nine && set ? (wide ? 0xcfffffff:0xdfffffff):0xffffffff;
            require(((s.cpsr^want)&mask)==0,"multiply defined flags wrong");
            for(unsigned reg=0;reg<15;++reg) {
                const uint32_t expected=reg==hi ? uint32_t(wide ? result>>32:result):wide && reg==lo ? uint32_t(result):original[reg];
                require(s.r[reg]==expected,"multiply result/source/accumulator alias wrong");
            }
        }
    for(bool nine:{false,true}) for(uint32_t word:{0xe00f0190u,0xe004f190u,0xe0040f90u,0xe004019fu,0xe085f190u,0xe0844190u,0xe02f3190u}) {
        instruction(m,nine,word); auto& cpu=nine ? m.cpu9:m.cpu7; const auto original=cpu.state.r; const auto psw=cpu.state.cpsr;
        cpu.step(); require(cpu.state.unsupported && cpu.state.r==original && cpu.state.cpsr==psw,"unpredictable multiply changed registers");
    }
    for(bool nine:{false,true}) {
        instruction(m,nine,0x00040190); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.r[0]=3; cpu.state.r[1]=7; cpu.step();
        require(cpu.state.r[4]==0 && cpu.state.r[15]==start+4 && !cpu.state.unsupported,"failed multiply condition executed");
    }
}
static void shifts(nds::Machine& m) {
    for(bool nine:{false,true}) for(auto value:patterns) for(unsigned type=0;type<4;++type)
        for(unsigned amount=0;amount<256;++amount) for(unsigned flags=0;flags<4;++flags) for(bool reg:{false,true}) {
            if(!reg && amount>=32) continue;
            instruction(m,nine,0xe1b04001|(type<<5)|(reg ? 0x210:amount<<7));
            auto& cpu=nine ? m.cpu9:m.cpu7; auto& s=cpu.state;
            s.r[1]=value; s.r[2]=amount; s.cpsr=0xdf|((flags&2) ? 0x20000000:0)|((flags&1) ? 0x10000000:0);
            uint32_t result=value; bool carry=flags&2;
            unsigned count=amount;
            if(!reg && !amount && (type==1 || type==2)) count=32;
            if(!reg && !amount && type==3) { result=(value/2)|(carry ? 0x80000000:0); carry=value&1; }
            else for(unsigned n=0;n<count;++n) {
                if(type==0) { carry=result>>31; result*=2; }
                else { const bool last=result&1; const bool top=type==2 ? result>>31:type==3 ? last:false;
                    result=result/2|(top ? 0x80000000:0); carry=last; }
            }
            cpu.step(); const uint32_t psw=0xdf|(result&0x80000000)|(result==0 ? 0x40000000:0)|(carry ? 0x20000000:0)|((flags&1) ? 0x10000000:0);
            require(!s.unsupported && s.r[4]==result && s.cpsr==psw,"register/immediate barrel shifter wrong");
        }
    for(bool nine:{false,true}) for(unsigned encoded=0;encoded<4096;++encoded) for(unsigned flags=0;flags<4;++flags) {
        instruction(m,nine,0xe3b04000|encoded); auto& cpu=nine ? m.cpu9:m.cpu7;
        cpu.state.cpsr=0xdf|((flags&2) ? 0x20000000:0)|((flags&1) ? 0x10000000:0);
        uint32_t result=encoded&255; bool carry=flags&2;
        for(unsigned n=0;n<2*(encoded>>8);++n) { const bool last=result&1; result=result/2|(last ? 0x80000000:0); carry=last; }
        cpu.step(); const uint32_t psw=0xdf|(result&0x80000000)|(result==0 ? 0x40000000:0)|(carry ? 0x20000000:0)|((flags&1) ? 0x10000000:0);
        require(cpu.state.r[4]==result && cpu.state.cpsr==psw,"rotated immediate flags wrong");
    }
}
static void control(nds::Machine& m) {
    for(bool nine:{false,true}) for(unsigned flags=0;flags<16;++flags) for(unsigned cond=0;cond<16;++cond) {
        const bool n=flags&8,z=flags&4,c=flags&2,v=flags&1;
        const bool expected[]={z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,n==v,n!=v,!z&&n==v,z||n!=v,true,false};
        instruction(m,nine,(cond<<28)|0x03a00009); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.cpsr=0xdf|(flags<<28);
        cpu.step(); require(cpu.state.r[0]==(expected[cond] ? 9u:0u),"condition table wrong");
        require(cpu.state.unsupported==(nine && cond==15),"ARMv5 unconditional extension misdecoded");
    }
    for(bool nine:{false,true}) for(uint32_t delta:{0xfe000000u,0xfffffffcu,0u,4u,0x01fffffcu}) {
        instruction(m,nine,0xeb000000|((delta>>2)&0xffffff)); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.step();
        require(cpu.state.r[15]==start+8+delta && cpu.state.r[14]==start+4,"relative branch/link wrong");
    }
    for(bool nine:{false,true}) {
        instruction(m,nine,0xe12fff10); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.r[0]=start+0x102; cpu.step();
        require(cpu.state.r[15]==start+0x100,"BX ARM alignment wrong");
        instruction(m,nine,0xe12fff10); cpu.state.r[0]=start+1; cpu.step();
        require(!cpu.state.unsupported && cpu.state.r[15]==start && (cpu.state.cpsr&0x20),"ARM BX did not enter Thumb");
        instruction(m,nine,0xe10f4000); cpu.state.cpsr=0xf00000df; cpu.step(); require(cpu.state.r[4]==0xf00000df,"MRS wrong");
        instruction(m,nine,0xe128f004); cpu.state.r[4]=0xa0000013; cpu.step(); require(cpu.state.cpsr==0xa00000df,"MSR flags changed mode");
        instruction(m,nine,0xe128f004); cpu.state.r[4]=0xf8000000; cpu.step();
        require(cpu.state.cpsr==(nine ? 0xf80000dfu:0xf00000dfu),"ARM9 Q/ARM7 reserved flag mask wrong");
        instruction(m,nine,0xee000010); cpu.step(); require(cpu.state.unsupported,"unimplemented coprocessor silent");
    }
}
static void thumb_instruction(nds::Machine& m,bool nine,uint16_t word) {
    instruction(m,nine,0); auto& cpu=nine ? m.cpu9:m.cpu7;
    cpu.state.cpsr|=0x20; cpu.state.r[15]=start+2;
    require(m.write(nine,start+2,2,word),"Thumb instruction write failed");
}
static void thumb_alu(nds::Machine& m) {
    for(bool nine:{false,true}) for(unsigned op=0;op<16;++op) for(auto a:patterns) for(auto b:patterns)
        for(unsigned incoming=0;incoming<16;++incoming) for(unsigned rd:{0u,1u,4u}) {
            thumb_instruction(m,nine,uint16_t(0x4000|(op<<6)|(1<<3)|rd)); auto& cpu=nine ? m.cpu9:m.cpu7; auto& s=cpu.state;
            s.r[0]=a; s.r[1]=b; s.r[4]=a; s.cpsr=0xff|(incoming<<28)|(nine ? 0x08000000:0); const auto original=s.r;
            if(op==13 && rd==1) { cpu.step(); require(s.unsupported && s.r==original,"unpredictable Thumb MUL alias executed"); continue; }
            const auto left=original[rd],right=b; bool carry=incoming&2,overflow=incoming&1; uint32_t result=0;
            if(op==5 || op==6 || op==9 || op==10 || op==11) {
                const bool sub=op==6 || op==9 || op==10; const auto x=op==9 ? 0:left;
                const unsigned extra=op==5 ? unsigned(carry):op==6 ? unsigned(!carry):0;
                const int64_t wide=sub ? int64_t(x)-right-extra:int64_t(x)+right+extra;
                const int64_t math=sub ? signed32(x)-signed32(right)-extra:signed32(x)+signed32(right)+extra;
                result=uint32_t(wide); carry=sub ? wide>=0:wide>0xffffffffLL; overflow=math< -0x80000000LL || math>0x7fffffffLL;
            } else if(op==2 || op==3 || op==4 || op==7) {
                result=left;
                for(unsigned count=0;count<(right&255);++count) {
                    if(op==2) { carry=result>>31; result*=2; }
                    else { const bool bit=result&1,top=op==4 ? result>>31:op==7 ? bit:false; result=result/2|(top ? 0x80000000:0); carry=bit; }
                }
            } else switch(op) {
            case 0:case 8:result=left&right;break; case 1:result=left^right;break;
            case 12:result=left|right;break; case 13:result=uint32_t(product_reference(left,right,false));break;
            case 14:result=left&~right;break; case 15:result=~right;break;
            }
            const auto expected_psw=0xff|(nine ? 0x08000000:0)|(result&0x80000000)|(result==0 ? 0x40000000:0)|(carry ? 0x20000000:0)|(overflow ? 0x10000000:0);
            cpu.step(); const auto mask=!nine && op==13 ? 0xdfffffff:0xffffffff;
            require(!s.unsupported && s.r[15]==start+4 && ((s.cpsr^expected_psw)&mask)==0,"Thumb ALU flags/retirement wrong");
            for(unsigned reg=0;reg<15;++reg) require(s.r[reg]==(reg==rd && op!=8 && op!=10 && op!=11 ? result:original[reg]),"Thumb ALU alias/source wrong");
        }
}
static void thumb_high_registers(nds::Machine& m) {
    for(bool nine:{false,true}) for(unsigned op=0;op<3;++op) for(unsigned rd=0;rd<16;++rd) for(unsigned rm=0;rm<16;++rm)
        for(auto a:patterns) for(auto b:patterns) for(unsigned incoming=0;incoming<16;++incoming) {
            thumb_instruction(m,nine,uint16_t(0x4400|(op<<8)|((rd&8)<<4)|(rm<<3)|(rd&7)));
            auto& cpu=nine ? m.cpu9:m.cpu7; auto& s=cpu.state;
            if(rd<15) s.r[rd]=a;
            if(rm<15) s.r[rm]=b;
            s.cpsr=0xff|(incoming<<28)|(nine ? 0x08000000:0); const auto original=s.r; const auto pc=s.r[15],psw=s.cpsr;
            const bool invalid=(rd<8 && rm<8) || (op==1 && rd==15);
            const uint32_t left=rd==15 ? pc+4:original[rd],right=rm==15 ? pc+4:original[rm];
            const uint32_t result=op==2 ? right:op==0 ? left+right:left-right;
            const int64_t math=signed32(left)-signed32(right);
            const uint32_t want=op!=1 ? psw:(psw&0x0fffffff)|(result&0x80000000)|(result==0 ? 0x40000000:0)|(left>=right ? 0x20000000:0)|((math< -0x80000000LL || math>0x7fffffffLL) ? 0x10000000:0);
            cpu.step(); require(s.unsupported==invalid,"Thumb high-register restriction wrong");
            if(invalid) { require(s.r==original && s.cpsr==psw,"unpredictable high-register encoding changed state"); continue; }
            require(s.cpsr==want && s.r[15]==(rd==15 && op!=1 ? result&~1u:pc+2),"Thumb high-register flags/PC wrong");
            for(unsigned reg=0;reg<15;++reg) require(s.r[reg]==(reg==rd && op!=1 ? result:original[reg]),"Thumb high-register source/alias wrong");
        }
    for(bool nine:{false,true}) for(unsigned rd=0;rd<8;++rd) for(unsigned immediate=0;immediate<256;++immediate) for(bool sp:{false,true}) for(auto value:patterns) {
        thumb_instruction(m,nine,uint16_t(0xa000|(sp ? 0x800:0)|(rd<<8)|immediate)); auto& cpu=nine ? m.cpu9:m.cpu7;
        cpu.state.r[13]=value; cpu.state.cpsr=0xf80000ff & (nine ? 0xffffffff:0xf7ffffff); const auto psw=cpu.state.cpsr;
        cpu.step(); require(cpu.state.r[rd]==(sp ? value:start+4)+immediate*4 && cpu.state.cpsr==psw,"Thumb PC/SP-relative address flags/alignment wrong");
    }
    for(bool nine:{false,true}) for(bool subtract:{false,true}) for(unsigned immediate=0;immediate<128;++immediate) for(auto value:patterns) {
        thumb_instruction(m,nine,uint16_t(0xb000|(subtract ? 0x80:0)|immediate)); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.r[13]=value; const auto psw=cpu.state.cpsr;
        cpu.step(); require(cpu.state.r[13]==(subtract ? value-immediate*4:value+immediate*4) && cpu.state.cpsr==psw,"Thumb SP adjustment flags/wrap wrong");
    }
}
static void thumb_control(nds::Machine& m) {
    for(bool nine:{false,true}) for(unsigned cond=0;cond<14;++cond) for(unsigned flags=0;flags<16;++flags) for(unsigned encoded=0;encoded<256;++encoded) {
        thumb_instruction(m,nine,uint16_t(0xd000|(cond<<8)|encoded)); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.cpsr=0xff|(flags<<28);
        const bool n=flags&8,z=flags&4,c=flags&2,v=flags&1;
        const bool table[]={z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,n==v,n!=v,!z&&n==v,z||n!=v};
        const int offset=encoded<128 ? int(encoded)*2:(int(encoded)-256)*2;
        cpu.step(); require(cpu.state.r[15]==(table[cond] ? start+6+offset:start+4) && !cpu.state.unsupported,"Thumb conditional branch wrong");
    }
    for(bool nine:{false,true}) for(unsigned encoded=0;encoded<2048;++encoded) {
        thumb_instruction(m,nine,uint16_t(0xe000|encoded)); auto& cpu=nine ? m.cpu9:m.cpu7;
        const int offset=encoded<1024 ? int(encoded)*2:(int(encoded)-2048)*2;
        cpu.step(); require(cpu.state.r[15]==start+6+offset,"Thumb unconditional branch wrong");
    }
    for(bool nine:{false,true}) for(bool from_thumb:{false,true}) for(bool link:{false,true}) for(unsigned lane=0;lane<4;++lane) {
        if(from_thumb) thumb_instruction(m,nine,uint16_t(0x4700|(link ? 0x80:0)));
        else instruction(m,nine,0xe12fff10|(link ? 0x20:0));
        auto& cpu=nine ? m.cpu9:m.cpu7; auto& s=cpu.state; const auto pc=s.r[15],psw=s.cpsr;
        s.r[0]=start+0x100+lane; const bool stop=(link && !nine) || (lane==2 && (from_thumb || link));
        cpu.step(); require(s.unsupported==stop,"BX/BLX architecture/alignment wrong");
        if(stop) require(s.r[15]==pc && s.cpsr==psw && s.r[14]==0,"rejected interworking changed state");
        else require(s.r[15]==(lane&1 ? start+0x100+(lane&2):start+0x100) && s.cpsr==((psw&~0x20u)|((lane&1)<<5)) && s.r[14]==(link ? (from_thumb ? (pc+2)|1:pc+4):0),"BX/BLX PC/T/link wrong");
    }
    for(bool nine:{false,true}) for(uint16_t word:{0x4400,0x4500,0x4600,0x45ff,0x4701,0x47f8,0xde00,0xdf00,0xf000,0xc000,0xb400}) {
        thumb_instruction(m,nine,word); auto& cpu=nine ? m.cpu9:m.cpu7; const auto original=cpu.state.r; cpu.step();
        require(cpu.state.unsupported && cpu.state.r==original,"unsupported/unpredictable Thumb encoding retired");
    }
    for(bool nine:{false,true}) {
        instruction(m,nine,0xe591f000); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.r[1]=start+0x100; m.write(nine,start+0x100,4,start+0x203); cpu.step();
        require(cpu.state.r[15]==(nine ? start+0x202:start+0x200) && bool(cpu.state.cpsr&0x20)==nine,"LDR PC architecture interworking wrong");
        thumb_instruction(m,nine,0x30ff); cpu.state.r[0]=0x7fffffff;
        std::vector<uint8_t> before,after,replay; m.save(before); cpu.step(); m.save(after);
        require(m.restore(before),"halfword-aligned Thumb state rejected"); (nine ? m.cpu9:m.cpu7).step(); m.save(replay);
        require(replay==after,"saved Thumb continuation diverged");
        auto bad=before; bad[nine ? 175:327]|=1; require(!m.restore(bad),"odd Thumb PC state accepted"); m.save(replay); require(replay==after,"bad Thumb state changed machine");
    }
}
static void memory(nds::Machine& m) {
    for(unsigned n=0;n<m.shared.size();++n) m.shared[n]=uint8_t(n^(n>>8));
    for(unsigned n=0;n<m.private7.size();++n) m.private7[n]=uint8_t(n+(n>>8)+9);
    for(unsigned mode=0;mode<4;++mode) {
        require(m.write(true,0x04000247,1,mode|0xfc),"WRAMCNT write failed"); uint32_t value=0;
        require(m.read(false,0x04000241,1,value) && value==mode,"WRAMSTAT wrong");
        require(!m.write(false,0x04000241,1,3),"ARM7 changed WRAMSTAT");
        for(bool nine:{false,true}) for(unsigned n=0;n<65536;++n) {
            value=0x12345678; const bool mapped=m.read(nine,0x03000000+n,1,value);
            if(nine && mode==3) require(!mapped && value==0x12345678,"unallocated ARM9 WRAM fabricated data");
            else {
                // Allocation table from the WRAM register definition; index by
                // region size, independent of the bus decoder's address masks.
                const unsigned base9[]={0,16384,0,0},base7[]={0,0,16384,0};
                const unsigned size9[]={32768,16384,16384,0},size7[]={65536,16384,16384,32768};
                const auto& storage=!nine && mode==0 ? m.private7:m.shared;
                const unsigned base=nine ? base9[mode]:base7[mode],span=nine ? size9[mode]:size7[mode];
                const auto expected=storage[base+n%span];
                require(mapped && value==expected,"shared/private WRAM bank/mirror wrong");
            }
        }
    }
    m.wramcnt=0; const auto guard=m.shared.back(); require(!m.write(true,0x03ffffff,2,0x1234) && m.shared.back()==guard,"failed cross-region write changed RAM");
    for(unsigned lane=0;lane<4;++lane) for(bool nine:{false,true}) {
        instruction(m,nine,0xe5914000); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.r[1]=start+0x100+lane;
        m.write(nine,start+0x100,4,0x89abcdef); cpu.step(); uint32_t expected=0x89abcdef;
        for(unsigned n=0;n<lane*8;++n) expected=expected/2|((expected&1) ? 0x80000000:0);
        require(cpu.state.r[4]==expected,"unaligned LDR rotation wrong");
        instruction(m,nine,0xe5d14000); cpu.state.r[1]=start+0x100+lane; cpu.step(); require(cpu.state.r[4]==((0x89abcdef>>(lane*8))&255),"LDRB wrong");
    }
    m.write(true,0x02000000,4,0x12345678);
    for(bool nine:{false,true}) for(uint32_t mirror:{0x02000000u,0x02400000u,0x02800000u,0x02c00000u}) { uint32_t v=0; require(m.read(nine,mirror,4,v) && v==0x12345678,"main RAM mirror/coherence wrong"); }
}
static uint32_t byte_load(const std::array<uint8_t,12>& bytes,unsigned lane,unsigned width,bool sign) {
    uint32_t result=0;
    for(unsigned n=0;n<width;++n) result+=uint32_t(bytes[lane+n])<<(8*n);
    if(sign && bytes[lane+width-1]>=128) result|=uint32_t(0xffffffffULL<<(width*8));
    return result;
}
static void transfer_case(nds::Machine& m,bool nine,unsigned rd,unsigned rn,bool update,
                          uint32_t adjusted,uint32_t target,unsigned width,bool load,bool sign,bool thumb) {
    auto& cpu=nine ? m.cpu9:m.cpu7; auto& s=cpu.state;
    const auto original=s.r; const auto psw=s.cpsr; const unsigned lane=target&3;
    const uint32_t word=target-lane,seed=load ? 0x89abcdefu:~original[rd];
    std::array<uint8_t,12> bytes{};
    for(unsigned n=0;n<12;++n) bytes[n]=uint8_t((n<4 ? seed:n<8 ? 0xdecafbadu:0x13579bdfu)>>(8*(n%4)));
    for(unsigned n=0;n<12;++n) require(m.write(nine,word+n,1,bytes[n]),"memory oracle setup failed");
    const bool odd=width==2 && (target&1);
    const auto result=load ? byte_load(bytes,lane,width,sign):original[rd];
    if(!odd && !load) for(unsigned n=0;n<width;++n) bytes[lane+n]=uint8_t(result>>(8*n));
    cpu.step();
    require(s.unsupported==odd && s.cpsr==psw,"memory flags/odd-half diagnostic wrong");
    for(unsigned reg=0;reg<16;++reg) {
        const auto want=odd ? original[reg]:reg==15 ? original[15]+(thumb ? 2:4):load && reg==rd ? result:update && reg==rn ? adjusted:original[reg];
        require(s.r[reg]==want,"memory destination/base/source/PC wrong");
    }
    for(unsigned n=0;n<12;++n) { uint32_t got=0; require(m.read(nine,word+n,1,got) && got==bytes[n],"memory store width/neighbor/atomicity wrong"); }
}
static void memory_instructions(nds::Machine& m) {
    constexpr uint32_t target=0x02003000;
    // Every ARM half/signed offset field and six address forms, including odd
    // halfword diagnostics. Byte arrays supply the load/store reference.
    for(bool nine:{false,true}) for(unsigned op=0;op<4;++op) for(bool imm:{false,true})
        for(unsigned index=0;index<3;++index) for(bool up:{false,true}) for(unsigned offset=0;offset<256;++offset)
        for(unsigned lane=0;lane<4;++lane) for(unsigned flags:{0u,5u,10u,15u}) {
            const unsigned type=op<2 ? 1:op==2 ? 2:3; const bool load=op!=0;
            const uint32_t field=imm ? ((offset&240)<<4)|(offset&15):2;
            instruction(m,nine,0xe0014090|(uint32_t(index!=2)<<24)|(uint32_t(up)<<23)|(uint32_t(imm)<<22)|
                (uint32_t(index==1)<<21)|(uint32_t(load)<<20)|(type<<5)|field);
            auto& s=(nine ? m.cpu9:m.cpu7).state;
            const auto address=target+lane,base=index==2 ? address:up ? address-offset:address+offset;
            s.r[1]=base; s.r[2]=imm ? offset^0xdead0000u:offset; s.r[4]=patterns[offset%16]; s.cpsr=0xdf|(flags<<28)|(nine ? 0x08000000:0);
            transfer_case(m,nine,4,1,index!=0,up ? base+offset:base-offset,address,op==2 ? 1:2,load,op>=2,false);
        }
    for(bool nine:{false,true}) for(unsigned op=0;op<4;++op) for(unsigned alias=0;alias<4;++alias) for(unsigned flags:{0u,5u,10u,15u}) {
        const unsigned rd=alias==1 ? 1:alias==2 ? 2:4,rm=alias==3 ? 1:2,type=op<2 ? 1:op==2 ? 2:3;
        instruction(m,nine,0xe1910090|(rd<<12)|(type<<5)|rm); auto& s=(nine ? m.cpu9:m.cpu7).state;
        if(op==0) { require(m.write(nine,start,4,0xe1810090|(rd<<12)|(type<<5)|rm),"alias instruction setup failed"); }
        s.r[4]=0x12348000; s.r[1]=alias==3 ? target/2:target-4; s.r[2]=4;
        s.cpsr=0xdf|(flags<<28)|(nine ? 0x08000000:0);
        transfer_case(m,nine,rd,1,false,0,target,op==2 ? 1:2,op!=0,op>=2,false);
    }
    // All 512 low-register triples, including destination/base/index aliases.
    for(bool nine:{false,true}) for(unsigned op=0;op<8;++op) for(unsigned rd=0;rd<8;++rd)
        for(unsigned rn=0;rn<8;++rn) for(unsigned rm=0;rm<8;++rm) for(auto value:patterns) for(unsigned flags:{0u,5u,10u,15u}) {
            thumb_instruction(m,nine,uint16_t(0x5000|(op<<9)|(rm<<6)|(rn<<3)|rd)); auto& s=(nine ? m.cpu9:m.cpu7).state;
            s.r[rd]=value; s.r[rn]=rn==rm ? target/2:target-4; s.r[rm]=rn==rm ? target/2:4;
            s.cpsr=0xff|(flags<<28)|(nine ? 0x08000000:0);
            transfer_case(m,nine,rd,rn,false,0,target,op==1 || op==5 || op==7 ? 2:op==2 || op==3 || op==6 ? 1:4,op>=3,op==3 || op==7,true);
        }
    // Immediate, SP and PC forms cover every field and destination, including
    // the halfword PC position used by thumb_instruction.
    for(bool nine:{false,true}) for(unsigned form=0;form<9;++form) for(unsigned rd=0;rd<8;++rd)
        for(unsigned offset=0;offset<(form<6 ? 32u:256u);++offset) for(unsigned flags:{0u,15u}) {
            const unsigned widths[]={4,4,1,1,2,2,4,4,4}; const uint16_t codes[]={0x6000,0x6800,0x7000,0x7800,0x8000,0x8800,0x9000,0x9800,0x4800};
            const bool load=form%2 || form==8; const unsigned width=widths[form];
            thumb_instruction(m,nine,uint16_t(codes[form]|(form<6 ? (offset<<6)|(1<<3)|rd:(rd<<8)|offset)));
            auto& s=(nine ? m.cpu9:m.cpu7).state; s.r[rd]=patterns[offset%16]; s.cpsr=0xff|(flags<<28)|(nine ? 0x08000000:0);
            const unsigned rn=form<6 ? 1:13; if(form<8) s.r[rn]=target-offset*width;
            const auto address=form==8 ? ((start+6)&~3u)+offset*4:target;
            transfer_case(m,nine,rd,rn,false,0,address,width,load,false,true);
        }
    // Invalid encodings and unmapped transactions must not write back or
    // partially update registers. Retired failed conditions skip diagnostics.
    for(bool nine:{false,true}) for(uint32_t word:{0xe1d1f0b0u,0xe19140bfu,0xe19141b2u,0xe0f140b0u,0xe1f110b0u,0xe1b140b1u,0xe1ff40b0u,0xe1d140d0u,0xe1f140b0u,0xe1e140b0u}) {
        instruction(m,nine,word); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.state.r[1]=0x05000000; cpu.state.r[2]=2; const auto original=cpu.state.r; const auto psw=cpu.state.cpsr;
        cpu.step(); require(cpu.state.unsupported && cpu.state.r==original && cpu.state.cpsr==psw,"invalid/unmapped transfer mutated registers");
    }
    for(bool nine:{false,true}) {
        instruction(m,nine,0x01f1f0b0); auto& cpu=nine ? m.cpu9:m.cpu7; cpu.step();
        require(!cpu.state.unsupported && cpu.state.r[15]==start+4,"failed memory condition diagnosed/executed");
        instruction(m,nine,0xe1df40b0); require(m.write(nine,start+8,2,0x8001),"PC literal setup failed"); cpu.step();
        require(!cpu.state.unsupported && cpu.state.r[4]==0x8001,"ARM halfword PC+8 base wrong");
    }
}
static void states(nds::Machine& m,const std::vector<uint8_t>& rom,const char* plugin) {
    require(m.load(rom.data(),rom.size()),"original reload failed"); m.frame(); require(!m.cpu9.state.unsupported && !m.cpu7.state.unsupported,"original execution stopped");
    std::vector<uint8_t> before,after,replay; require(m.save(before),"save failed"); m.frame(); m.save(after);
    require(m.restore(before),"restore failed"); m.frame(); m.save(replay); require(replay==after,"dual CPU/RAM state replay wrong");
    for(unsigned n=0;n<9;++n) {
        auto bad=before;
        if(n==0) bad.pop_back();
        if(n==1) bad[7]='2';
        if(n==2) bad[15]^=1;
        if(n==3) bad[23]^=1;
        if(n==4) bad[39]=4;
        if(n==5) bad[47]=4;
        if(n==6) bad[175]|=1;
        if(n==7) bad[183]=0x13;
        if(n==8) bad[199]=2;
        require(!m.restore(bad),"bad state accepted"); m.save(replay); require(replay==after,"bad state changed machine");
    }
    for(unsigned n=0;n<8;++n) {
        auto bad=rom;
        if(n==0) bad[0x180]=0;
        if(n==1) bad[0x12]=2;
        if(n==2) bad[0x20]=1;
        if(n==3) bad[0x24]|=1;
        if(n==4) bad[0x2f]=0xff;
        if(n==5) bad[0x3b]=0xff;
        if(n==6) bad[0x3f]=0xff;
        if(n==7) bad.resize(400);
        require(!m.load(bad.data(),bad.size()),"bad cartridge accepted"); m.save(replay); require(replay==after,"rejected cartridge changed active machine");
    }
    emulator_test::LoadedCore loaded(plugin,rom); auto& core=*loaded.plugin;
    core.run_frame({0}); require(core.save_state(before),"plugin save failed"); core.run_frame({0}); core.save_state(after);
    require(core.load_state(before),"plugin restore failed"); core.run_frame({0}); core.save_state(replay); require(after==replay,"shared-plugin state replay wrong");
    require(core.get_info().screen_width==256 && core.get_info().screen_height==384 && core.get_framebuffer().width==256,"DS plugin dimensions wrong");
    // Continue both CPUs at a halfword-aligned Thumb self-loop through the
    // actual plugin ABI; RAM and execution state are legitimate mutable state.
    const auto scalar=[&](unsigned offset,uint64_t value) { for(unsigned n=0;n<8;++n) before[offset+n]=uint8_t(value>>(56-n*8)); };
    scalar(168,start+2); scalar(320,start+2); before[183]|=0x20; before[335]|=0x20;
    before[352+0x2002]=0xfe; before[352+0x2003]=0xe7;
    require(core.load_state(before),"plugin Thumb state rejected"); core.run_frame({0}); core.save_state(after);
    require((after[183]&0x20) && (after[335]&0x20),"plugin lost Thumb state");
    require(core.load_state(before),"plugin Thumb replay restore failed"); core.run_frame({0}); core.save_state(replay);
    require(replay==after,"actual plugin Thumb continuation diverged");
    core.unload_rom(); require(!core.is_rom_loaded() && !core.save_state(replay),"unloaded plugin retained cartridge");
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"usage: nds_machine_test plugin original.nds"); std::ifstream in(argv[2],std::ios::binary);
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),{}); nds::Machine m; require(m.load(rom.data(),rom.size()),"original DS ROM rejected");
        arithmetic(m); multiplies(m); shifts(m); control(m); thumb_alu(m); thumb_high_registers(m); thumb_control(m); memory_instructions(m); memory(m); states(m,rom,argv[1]);
        std::cout<<"DS ARM/Thumb ALU/multiply/shifter/control/memory/state/plugin checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
