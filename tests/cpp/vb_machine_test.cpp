#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
using emulator_test::require;
constexpr uint32_t start=0x05000000;
static uint16_t reg(unsigned op,unsigned r1=0,unsigned r2=0) { return uint16_t(op<<10|r2<<5|r1); }
static void code(vb::Machine& m,std::initializer_list<uint16_t> words) {
    m.reset(); uint32_t address=start;
    for(auto word:words) { require(m.write(address,2,word),"code store failed"); address+=2; }
    m.cpu.state.pc=start;
}
static void control(vb::Machine& m) {
    for(unsigned flags=0;flags<16;++flags) for(unsigned cond=0;cond<16;++cond) {
        const bool z=flags&1,s=flags&2,ov=flags&4,cy=flags&8;
        const bool expected[]={ov,cy,z,cy||z,s,true,s!=ov,z||(s!=ov),!ov,!cy,!z,!(cy||z),!s,false,s==ov,!z&&(s==ov)};
        for(int displacement:{-256,-2,0,2,254}) {
            code(m,{uint16_t(0x8000|(cond<<9)|(displacement&0x1fe))}); m.cpu.state.sr[5]=flags;
            m.cpu.step(); require(m.cpu.state.pc==start+(expected[cond] ? displacement:2),"branch condition/relative base wrong");
            require(m.cpu.state.sr[5]==flags,"branch changed flags");
        }
        code(m,{reg(0x12,cond,2)}); m.cpu.state.sr[5]=flags; m.cpu.step();
        require(m.cpu.state.gpr[2]==expected[cond] && m.cpu.state.sr[5]==flags,"SETF wrong");
    }
    code(m,{reg(6,1)}); m.cpu.state.gpr[1]=start+0x21; m.cpu.step();
    require(m.cpu.state.pc==start+0x20,"JMP low bit not masked");
    for(bool link:{false,true}) for(int offset:{-0x2000000,-2,0,4,0x1fffffe}) {
        code(m,{uint16_t((link ? 0x2b:0x2a)<<10|((uint32_t(offset)>>16)&0x3ff)),uint16_t(offset)});
        m.cpu.step(); require(m.cpu.state.pc==start+offset,"JR/JAL displacement wrong");
        require(m.cpu.state.gpr[31]==(link ? start+4:0),"JAL link wrong");
    }
    code(m,{reg(0x10,31,1),reg(0x11,1,1),reg(0x11,1,0)});
    m.cpu.step(); require(m.cpu.state.gpr[1]==0xffffffff,"MOV imm5 sign wrong");
    m.cpu.step(); require(m.cpu.state.gpr[1]==0 && (m.cpu.state.sr[5]&15)==9,"ADD immediate flags wrong");
    m.cpu.step(); require(m.cpu.state.gpr[0]==0,"r0 writable");
    code(m,{reg(1,1,1)}); m.cpu.state.gpr[1]=0x80000000; m.cpu.step();
    require(m.cpu.state.gpr[1]==0 && (m.cpu.state.sr[5]&15)==13,"aliased ADD wrong");
    code(m,{reg(4,1,2)}); m.cpu.state.gpr[1]=32; m.cpu.state.gpr[2]=0x80000001; m.cpu.state.sr[5]=0x800f;
    m.cpu.step(); require(m.cpu.state.gpr[2]==0x80000001 && m.cpu.state.sr[5]==0x8002,"zero-count shift carry/high flags wrong");
    code(m,{reg(0xf,1,2)}); m.cpu.state.gpr[1]=0; m.cpu.state.sr[5]=0x800c; m.cpu.step();
    require(m.cpu.state.gpr[2]==0xffffffff && m.cpu.state.sr[5]==0x800a,"NOT source/carry wrong");
    code(m,{reg(0x1a),reg(0x10,1,2)}); m.cpu.step(); const auto steps=m.cpu.state.steps;
    m.cpu.step(); require(m.cpu.state.halted && m.cpu.state.steps==steps && m.cpu.state.gpr[2]==0,"HALT did not stop");
    code(m,{reg(0x16)}); m.cpu.step(); require(m.cpu.state.unsupported,"unsupported instruction silently executed");
}
static void muldiv(vb::Machine& m) {
    struct Vector { unsigned op; uint32_t a,b,lo,hi,flags; };
    for(auto v:{Vector{8,2,0xffffffff,0xfffffffe,0xffffffff,10},
                Vector{8,0xffffffff,0x80000000,0x80000000,0,14},
                Vector{10,0xffffffff,0xffffffff,1,0xfffffffe,12},
                Vector{9,3,0xfffffff9,0xfffffffe,0xffffffff,10},
                Vector{9,0xffffffff,0x80000000,0x80000000,0,14},
                Vector{11,2,1,0,1,9}}) {
        for(unsigned destination:{0u,2u,30u}) {
            code(m,{reg(v.op,1,destination)}); auto& s=m.cpu.state;
            s.gpr[1]=v.a; s.gpr[destination]=destination ? v.b:0; s.sr[5]=0x800f;
            m.cpu.step();
            require(s.gpr[1]==v.a && !s.unsupported,"multiply/divide source changed");
            if(destination) {
                require(s.gpr[destination]==v.lo && s.gpr[30]==(destination==30 ? v.lo:v.hi)
                        && s.sr[5]==(0x8000|v.flags),"multiply/divide result/alias/PSW wrong");
            } else require(s.gpr[0]==0 && s.gpr[30]==0 && s.sr[5]==0x8009,"r0 multiply/divide flags wrong");
        }
    }
    for(unsigned op:{8u,9u,10u,11u}) {
        const bool multiply=op==8 || op==10;
        code(m,{reg(op,30,2)}); m.cpu.state.gpr[30]=3; m.cpu.state.gpr[2]=7; m.cpu.state.sr[5]=15;
        m.cpu.step(); require(m.cpu.state.gpr[2]==(multiply ? 21u:2u) && m.cpu.state.gpr[30]==(multiply ? 0u:1u),
                             "r30 source overwritten before use");
        code(m,{reg(op,2,2)}); m.cpu.state.gpr[2]=7; m.cpu.state.sr[5]=15; m.cpu.step();
        require(m.cpu.state.gpr[2]==(multiply ? 49u:1u) && m.cpu.state.gpr[30]==0 && m.cpu.state.sr[5]==8,
                "multiply/divide same source/destination wrong");
    }
    for(unsigned op:{9u,11u}) for(unsigned bits=0;bits<16;++bits) for(bool nested:{false,true}) for(bool ae:{false,true}) {
        code(m,{reg(op,1,2),reg(0x19)}); auto& s=m.cpu.state;
        const uint32_t original=0xa0000|bits|(nested ? 0x4000:0)|(ae ? 0x2000:0);
        s.gpr[2]=0x89abcdef; s.gpr[30]=0x13579bdf; s.sr[5]=original;
        s.sr[4]=0x12345678; s.sr[0]=start+0x80; s.sr[1]=0x101;
        s.sr[2]=start+0x90; s.sr[3]=0x202;
        m.cpu.step(); const unsigned saved=nested ? 2:0;
        require(s.pc==(nested ? 0xffffffd0:0xffffff80) && s.sr[saved]==start && s.sr[saved+1]==original,
                "divide-zero saved context/vector wrong");
        require(s.sr[4]==(nested ? 0xff805678:0x1234ff80) && s.sr[5]==((original|(nested ? 0x9000:0x5000))&~0x2000u),
                "divide-zero ECR/entry PSW wrong");
        require(s.gpr[2]==0x89abcdef && s.gpr[30]==0x13579bdf && !s.unsupported,"divide-zero changed result");
        require(s.sr[nested ? 0:2]==start+(nested ? 0x80:0x90),"divide-zero destroyed other saved context");
        // Save while in the handler, then verify RETI continuation is identical.
        s.pc=start+2; s.sr[saved]=start+4;
        std::vector<uint8_t> snapshot,after,replay; require(m.save(snapshot),"exception save failed");
        m.cpu.step(); require(s.pc==start+4 && s.sr[5]==original,"RETI restore/NP priority wrong");
        m.save(after); require(m.restore(snapshot),"exception restore failed"); m.cpu.step(); m.save(replay);
        require(after==replay,"saved exception continuation diverged");
    }
    struct FatalBus : vb::Memory {
        std::vector<std::pair<uint32_t,uint32_t>> writes;
        bool read(uint32_t,unsigned bytes,uint32_t& value) override { value=reg(9,1,2); return bytes==2; }
        bool write(uint32_t address,unsigned bytes,uint32_t value) override {
            require(bytes==4,"fatal fault write width wrong"); writes.emplace_back(address,value); return false;
        }
    } bus;
    vb::V810 cpu(bus); cpu.reset(); cpu.state.pc=start; cpu.state.sr[5]=0xe00f;
    cpu.state.gpr[2]=123; cpu.state.gpr[30]=456; const auto sr=cpu.state.sr;
    cpu.step(); require(cpu.state.halted && !cpu.state.unsupported && cpu.state.pc==start && cpu.state.sr==sr,
                        "fatal exception state wrong");
    require(bus.writes==std::vector<std::pair<uint32_t,uint32_t>>{{0,0xffffff80},{4,0xe00f},{8,start}},
            "fatal exception bus order/context wrong");
    require(cpu.state.gpr[2]==123 && cpu.state.gpr[30]==456,"fatal fault changed operands");
    cpu.step(); require(cpu.state.steps==1 && bus.writes.size()==3,"fatal fault did not halt until reset");
}
static void immediates(vb::Machine& m) {
    // Decode immediate forms through a different encoding, then compare with the
    // register operations independently graded by the original ROM.
    const uint32_t values[]={0u,1u,0x7fffffffu,0x80000000u,0xffffffffu,0x89abcdefu};
    for(unsigned immediate=0;immediate<32;++immediate) for(uint32_t value:values) for(unsigned flags=0;flags<16;++flags) {
        for(auto pair:{std::pair<unsigned,unsigned>{0x11,1},{0x13,3},{0x14,4},{0x15,5},{0x17,7}}) {
            code(m,{reg(pair.first,immediate,2)}); m.cpu.state.gpr[2]=value; m.cpu.state.sr[5]=0x8000|flags; m.cpu.step();
            const auto result=m.cpu.state.gpr[2], psw=m.cpu.state.sr[5];
            code(m,{reg(pair.second,1,2)}); m.cpu.state.gpr[1]=pair.first==0x11 || pair.first==0x13 ? vb::extend(immediate,5):immediate;
            m.cpu.state.gpr[2]=value; m.cpu.state.sr[5]=0x8000|flags; m.cpu.step();
            require(result==m.cpu.state.gpr[2] && psw==m.cpu.state.sr[5],"immediate/register decoding differs");
        }
        code(m,{reg(0x10,immediate,2)}); m.cpu.state.sr[5]=0x8000|flags; m.cpu.step();
        require(m.cpu.state.gpr[2]==vb::extend(immediate,5) && m.cpu.state.sr[5]==(0x8000|flags),"MOV immediate changed flags/sign");
    }
    struct Vector {unsigned op; uint32_t source; uint16_t imm; uint32_t result,psw;};
    for(auto v:{Vector{0x28,20,0xfff0,4,0x800f},Vector{0x2f,0x1234,0x8000,0x80001234,0x800f},
                Vector{0x29,0x7fffffff,1,0x80000000,0x8006},Vector{0x29,0,0xffff,0xffffffff,0x8002},
                Vector{0x2c,0x80000000,1,0x80000001,0x800a},Vector{0x2d,0xffffffff,0x8000,0x8000,0x8008},
                Vector{0x2d,0xffffffff,0,0,0x8009},Vector{0x2e,0x80000000,0xffff,0x8000ffff,0x800a}}) {
        for(bool alias:{false,true}) {
            const unsigned source=alias ? 2:1;
            code(m,{reg(v.op,source,2),v.imm}); m.cpu.state.gpr[source]=v.source; m.cpu.state.sr[5]=0x800f;
            m.cpu.step(); require(m.cpu.state.gpr[2]==v.result && m.cpu.state.sr[5]==v.psw,"wide immediate/alias result wrong");
        }
    }
}
static void memory(vb::Machine& m) {
    code(m,{reg(0x30,1,2),0,reg(0x31,1,3),0,reg(0x33,1,4),0,reg(0x35,1,5),0});
    m.cpu.state.gpr[1]=start+0x103; m.cpu.state.gpr[5]=0x1234;
    require(m.write(start+0x100,4,0x80817fff),"memory fixture write failed");
    m.cpu.step(); require(m.cpu.state.gpr[2]==0xffffff80,"LD.B sign wrong");
    m.cpu.step(); require(m.cpu.state.gpr[3]==0xffff8081,"LD.H alignment/sign wrong");
    m.cpu.step(); require(m.cpu.state.gpr[4]==0x80817fff,"LD.W low bits not masked");
    m.cpu.step(); uint32_t v=0;
    require(m.read(start+0x100,4,v) && v==0x12347fff,"ST.H alignment/byte order wrong");
    require(m.read(0xfd000100,4,v) && v==0x12347fff && m.read(0x05010100,4,v) && v==0x12347fff,"WRAM/27-bit aliases wrong");
    require(m.write(0x0500ffff,4,0x12345678) && m.ram[0xffff]==0x78 && m.ram[0]==0x56 && m.ram[2]==0x12,"WRAM local mirror boundary wrong");
    const auto before=m.ram;
    require(!m.write(0x05ffffff,4,1) && m.ram==before,"invalid cross-region write mutated RAM");
    require(!m.write(0x07000000,4,1) && !m.read(0x02000000,4,v),"ROM protection/unmapped read wrong");
    for(unsigned id:{0u,2u,25u}) {
        code(m,{reg(0x1c,id,1),reg(0x1d,id,2)}); m.cpu.state.gpr[1]=0xffffffff;
        m.cpu.step(); m.cpu.step(); require(m.cpu.state.gpr[2]==0xfffffffe,"system PC alignment wrong");
    }
    code(m,{reg(0x1c,5,1),reg(0x1d,5,2)}); m.cpu.state.gpr[1]=0xffffffff;
    m.cpu.step(); m.cpu.step(); require(m.cpu.state.gpr[2]==0xff3ff,"PSW reserved bits writable");
    code(m,{reg(0x1c,4,1),reg(0x1d,4,2)}); m.cpu.state.gpr[1]=0xffffffff;
    m.cpu.step(); m.cpu.step(); require(m.cpu.state.gpr[2]==0xfff0,"ECR reset/read-only wrong");
}
static void states(vb::Machine& m,const std::vector<uint8_t>& rom,const char* library) {
    code(m,{reg(1,1,2),reg(7,1,2)}); m.cpu.state.gpr[1]=1; m.cpu.state.gpr[2]=0x7fffffff;
    m.cpu.step(); m.frames=123; m.ram[0xffff]=99;
    std::vector<uint8_t> saved,after,replay;
    require(m.save(saved),"state save failed"); m.cpu.step(); m.save(after);
    require(m.restore(saved),"state restore failed"); m.cpu.step(); m.save(replay);
    require(after==replay,"state continuation diverged");
    for(unsigned mutation=0;mutation<7;++mutation) {
        auto bad=after;
        if(mutation==0) bad.pop_back();
        if(mutation==1) bad[0]^=1;
        if(mutation==2) bad[8]^=1;
        if(mutation==3) bad[39]=2;
        if(mutation==4) bad[47]=1; // r0
        if(mutation==5) bad[40]=1; // GPR upper half
        if(mutation==6) bad[559]=1; // PC low bit
        require(!m.restore(bad),"invalid state accepted"); m.save(replay); require(replay==after,"rejected state mutated machine");
    }
    auto bad=rom; bad[bad.size()-0x200]=0;
    require(!m.load(bad.data(),bad.size()) && !m.load(nullptr,rom.size()) && !m.load(rom.data(),rom.size()-1),"invalid cartridge accepted");
    m.save(replay); require(after==replay,"failed load replaced active machine");
    emulator_test::LoadedCore core(library,rom);
    require(core.plugin->get_info().name && std::string(core.plugin->get_info().name)=="VB","plugin identity wrong");
    auto other_rom=rom; other_rom.back()^=1;
    vb::Machine other; require(other.load(other_rom.data(),other_rom.size()),"second marked image rejected");
    std::vector<uint8_t> other_before,other_after; other.save(other_before);
    require(!other.restore(after),"state from another full cartridge accepted"); other.save(other_after);
    require(other_before==other_after,"wrong-ROM state changed active machine");
    core.plugin->run_frame({}); require(core.plugin->save_state(saved),"plugin save failed");
    for(unsigned n=0;n<2;++n) core.plugin->run_frame({});
    core.plugin->save_state(after);
    require(core.plugin->load_state(saved),"plugin restore failed");
    for(unsigned n=0;n<2;++n) core.plugin->run_frame({});
    core.plugin->save_state(replay);
    require(after==replay,"shared plugin continuation diverged");
    core.plugin->unload_rom(); require(!core.plugin->is_rom_loaded() && !core.plugin->save_state(replay),"unload failed");
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"usage: vb_machine_test plugin ROM");
        std::ifstream file(argv[2],std::ios::binary); std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)),{});
        vb::Machine m; require(m.load(rom.data(),rom.size()),"original cartridge load failed");
        require(m.cpu.state.pc==0xfffffff0 && m.cpu.state.sr[5]==0x8000 && m.cpu.state.sr[4]==0xfff0,"reset state wrong");
        m.cpu.step(); m.cpu.step(); require(m.cpu.state.pc==0x07000000,"reset vector did not reach original entry");
        control(m); muldiv(m); immediates(m); memory(m); states(m,rom,argv[1]);
        std::cout<<"VB control/memory/state/plugin checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
