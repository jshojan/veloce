#include "machine.hpp"
#include <algorithm>
#include <cstring>
namespace nds {
static uint32_t little(const uint8_t* p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
struct Segment { uint32_t offset,entry,address,size; };
static Segment segment(const uint8_t* p) { return {little(p),little(p+4),little(p+8),little(p+12)}; }
bool Machine::load(const uint8_t* data,size_t size) {
    if(!data || size<512 || size>16*1024*1024 || data[0x12] || std::memcmp(data+0x180,"VDSHB001",8)) return false;
    const auto nine=segment(data+0x20),seven=segment(data+0x30);
    const auto valid=[&](Segment s,bool arm9) {
        if(s.offset<512 || (s.offset&3) || !s.size || (s.size&3) || (s.address&3) || (s.entry&3) ||
           uint64_t(s.offset)+s.size>size || s.entry<s.address || uint64_t(s.entry)+4>uint64_t(s.address)+s.size) return false;
        return (s.address>=0x02000000 && uint64_t(s.address)+s.size<=0x02400000) ||
            (!arm9 && s.address>=0x03800000 && uint64_t(s.address)+s.size<=0x03810000);
    };
    if(!valid(nine,true) || !valid(seven,false)) return false;
    const auto overlap=[](uint32_t a,uint32_t as,uint32_t b,uint32_t bs) { return uint64_t(a)<uint64_t(b)+bs && uint64_t(b)<uint64_t(a)+as; };
    if(overlap(nine.offset,nine.size,seven.offset,seven.size) || overlap(nine.address,nine.size,seven.address,seven.size)) return false;
    std::vector<uint8_t> image(data,data+size);
    uint32_t hash=0xffffffff; uint64_t id=14695981039346656037ull;
    for(auto b:image) { hash^=b; for(unsigned n=0;n<8;++n) hash=(hash>>1)^((hash&1) ? 0xedb88320u:0); id^=b; id*=1099511628211ull; }
    rom=std::move(image); crc=~hash; identity=id; reset(); return true;
}
void Machine::reset() {
    std::fill(main.begin(),main.end(),0); std::fill(shared.begin(),shared.end(),0); std::fill(private7.begin(),private7.end(),0);
    frames=0; done=0; wramcnt=3; cpu9.reset(0); cpu7.reset(0);
    if(rom.empty()) return;
    for(bool nine:{true,false}) {
        const auto s=segment(rom.data()+(nine ? 0x20:0x30));
        auto& memory=s.address<0x03000000 ? main:private7;
        const auto offset=s.address-(s.address<0x03000000 ? 0x02000000:0x03800000);
        std::copy_n(rom.begin()+s.offset,s.size,memory.begin()+offset);
        (nine ? cpu9:cpu7).reset(s.entry);
    }
}
uint8_t* Machine::byte(bool arm9,uint32_t address) {
    if(address>=0x02000000 && address<0x03000000) return &main[address&0x3fffff];
    if(!arm9 && address>=0x03800000 && address<0x04000000) return &private7[address&0xffff];
    if(address>=0x03000000 && address<(arm9 ? 0x04000000u:0x03800000u)) {
        if(wramcnt==0) return arm9 ? &shared[address&0x7fff]:&private7[address&0xffff];
        if(wramcnt==3) return arm9 ? nullptr:&shared[address&0x7fff];
        const bool upper=arm9 ? wramcnt==1:wramcnt==2;
        return &shared[(upper ? 0x4000:0)+(address&0x3fff)];
    }
    return nullptr;
}
bool Machine::read(bool arm9,uint32_t address,unsigned bytes,uint32_t& value) {
    if((bytes!=1 && bytes!=2 && bytes!=4) || uint64_t(address)+bytes>0x100000000ull) return false;
    if(bytes==1 && address==(arm9 ? 0x04000247u:0x04000241u)) { value=wramcnt; return true; }
    uint32_t result=0;
    for(unsigned n=0;n<bytes;++n) { auto* p=byte(arm9,address+n); if(!p) return false; result|=uint32_t(*p)<<(n*8); }
    value=result; return true;
}
bool Machine::write(bool arm9,uint32_t address,unsigned bytes,uint32_t value) {
    if((bytes!=1 && bytes!=2 && bytes!=4) || uint64_t(address)+bytes>0x100000000ull) return false;
    // Repository-only debug port, outside DS peripheral registers.
    if(address==0x04ff0000 && bytes==1) { const char c=char(value); if(sink) sink->write(&c,1); return true; }
    if(address==0x04ff0008 && bytes==4) {
        if(sink) {
            static constexpr char digits[]="0123456789abcdef";
            char text[8];
            for(unsigned n=0;n<8;++n) text[n]=digits[(value>>(28-n*4))&15];
            sink->write(text,sizeof(text));
        }
        return true;
    }
    if(address==0x04ff0004 && bytes==4) { done|=arm9 ? 1:2; if(done==3 && sink) sink->finish(-1); return true; }
    if(arm9 && address==0x04000247 && bytes==1) { wramcnt=value&3; return true; }
    uint8_t* destinations[4]{};
    for(unsigned n=0;n<bytes;++n) { destinations[n]=byte(arm9,address+n); if(!destinations[n]) return false; }
    for(unsigned n=0;n<bytes;++n) *destinations[n]=uint8_t(value>>(n*8));
    return true;
}
void Machine::frame() {
    if(rom.empty()) return;
    // Scheduling ratio only; no verified bus, cache or pipeline timing.
    for(unsigned n=0;n<100000 && done!=3;++n) {
        if(!(done&1)) { cpu9.step(); if(!(done&1)) cpu9.step(); }
        if(!(done&2)) cpu7.step();
        if(((done&1)||cpu9.state.unsupported) && ((done&2)||cpu7.state.unsupported)) break;
    }
    ++frames;
}
static void append(std::vector<uint8_t>& out,uint64_t value) { for(unsigned n=0;n<8;++n) out.push_back(uint8_t(value>>(56-n*8))); }
bool Machine::save(std::vector<uint8_t>& out) const {
    if(rom.empty()) return false;
    out.assign({'V','N','D','S','0','0','0','1'});
    for(auto v:{identity,uint64_t(crc),frames,uint64_t(done),uint64_t(wramcnt)}) append(out,v);
    for(const auto* cpu:{&cpu9,&cpu7}) {
        for(auto v:cpu->state.r) append(out,v);
        append(out,cpu->state.cpsr); append(out,cpu->state.steps); append(out,cpu->state.unsupported);
    }
    for(const auto* memory:{&main,&shared,&private7}) out.insert(out.end(),memory->begin(),memory->end());
    return true;
}
bool Machine::restore(const std::vector<uint8_t>& in) {
    constexpr size_t prefix=8+(5+2*19)*8;
    if(rom.empty() || in.size()!=prefix+main.size()+shared.size()+private7.size() || std::memcmp(in.data(),"VNDS0001",8)) return false;
    size_t pos=8; const auto take=[&]() { uint64_t v=0; for(unsigned n=0;n<8;++n) v=(v<<8)|in[pos++]; return v; };
    if(take()!=identity || take()!=crc) return false;
    const auto frame=take(),finished=take(),wram=take(); if(finished>3 || wram>3) return false;
    ArmState cpus[2];
    for(unsigned index=0;index<2;++index) {
        auto& cpu=cpus[index];
        for(auto& r:cpu.r) { const auto v=take(); if(v>0xffffffff) return false; r=uint32_t(v); }
        const auto psw=take(); if((psw&~(index==0 ? 0xf8000020ull:0xf0000020ull))!=0xdf || (cpu.r[15]&((psw&0x20) ? 1:3))) return false;
        cpu.cpsr=uint32_t(psw); cpu.steps=take(); const auto stopped=take(); if(stopped>1) return false; cpu.unsupported=stopped;
    }
    cpu9.state=cpus[0]; cpu7.state=cpus[1]; frames=frame; done=uint8_t(finished); wramcnt=uint8_t(wram);
    for(auto* memory:{&main,&shared,&private7}) { std::copy_n(in.begin()+pos,memory->size(),memory->begin()); pos+=memory->size(); }
    return true;
}
}
