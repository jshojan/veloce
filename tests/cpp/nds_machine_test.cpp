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
        require(cpu.state.unsupported && cpu.state.r[15]==start,"unimplemented Thumb transfer silently executed");
        instruction(m,nine,0xe10f4000); cpu.state.cpsr=0xf00000df; cpu.step(); require(cpu.state.r[4]==0xf00000df,"MRS wrong");
        instruction(m,nine,0xe128f004); cpu.state.r[4]=0xa0000013; cpu.step(); require(cpu.state.cpsr==0xa00000df,"MSR flags changed mode");
        instruction(m,nine,0xe128f004); cpu.state.r[4]=0xf8000000; cpu.step();
        require(cpu.state.cpsr==(nine ? 0xf80000dfu:0xf00000dfu),"ARM9 Q/ARM7 reserved flag mask wrong");
        instruction(m,nine,0xee000010); cpu.step(); require(cpu.state.unsupported,"unimplemented coprocessor silent");
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
    core.unload_rom(); require(!core.is_rom_loaded() && !core.save_state(replay),"unloaded plugin retained cartridge");
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"usage: nds_machine_test plugin original.nds"); std::ifstream in(argv[2],std::ios::binary);
        std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),{}); nds::Machine m; require(m.load(rom.data(),rom.size()),"original DS ROM rejected");
        arithmetic(m); shifts(m); control(m); memory(m); states(m,rom,argv[1]);
        std::cout<<"DS ALU/shifter/control/memory/state/plugin checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
