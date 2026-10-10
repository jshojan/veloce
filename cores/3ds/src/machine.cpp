#include "machine.hpp"
#include <algorithm>
#include <cstring>
namespace ctr {
static uint32_t little(const uint8_t* p) { uint32_t v=0; for(unsigned n=0;n<4;++n) v|=uint32_t(p[n])<<(n*8); return v; }
struct Segment { uint32_t offset,address,entry,size; };
static Segment segment(const uint8_t* p) { return {little(p),little(p+4),little(p+8),little(p+12)}; }
uint8_t* Machine::byte(unsigned id,uint32_t address) {
    if(id>2) return nullptr;
    if(address>=0x20000000 && address<0x28000000) return &fcram[address-0x20000000];
    if(address>=0x1ff80000 && address<0x20000000) return &axi[address-0x1ff80000];
    if(id==2 && address>=0x08000000 && address<0x08100000) return &private9[address-0x08000000];
    return nullptr;
}
bool Machine::load(const uint8_t* data,size_t size) {
    if(!data || size<64 || size>0x1000000 || std::memcmp(data,"V3DSP001",8)) return false;
    for(unsigned n=8;n<16;++n) if(data[n]) return false;
    Segment regions[3];
    for(unsigned id=0;id<3;++id) {
        auto& s=regions[id]; s=segment(data+16+id*16);
        if(s.offset<64 || (s.offset&3) || (s.address&3) || (s.entry&3) || !s.size || (s.size&3) ||
           uint64_t(s.offset)+s.size>size || s.entry<s.address || uint64_t(s.entry)+4>uint64_t(s.address)+s.size ||
           uint64_t(s.address)+s.size>0x100000000ull) return false;
        const bool f=s.address>=0x20000000 && uint64_t(s.address)+s.size<=0x28000000;
        const bool a=s.address>=0x1ff80000 && uint64_t(s.address)+s.size<=0x20000000;
        const bool p=id==2 && s.address>=0x08000000 && uint64_t(s.address)+s.size<=0x08100000;
        if(!f && !a && !p) return false;
        const auto overlaps=[](uint32_t a,uint32_t an,uint32_t b,uint32_t bn) { return uint64_t(a)<uint64_t(b)+bn && uint64_t(b)<uint64_t(a)+an; };
        for(unsigned prev=0;prev<id;++prev) if(overlaps(s.offset,s.size,regions[prev].offset,regions[prev].size) ||
            overlaps(s.address,s.size,regions[prev].address,regions[prev].size)) return false;
    }
    uint32_t checksum=0xffffffff; uint64_t hash=0xcbf29ce484222325ULL;
    for(size_t n=0;n<size;++n) { checksum^=data[n]; hash=(hash^data[n])*0x100000001b3ULL; for(unsigned bit=0;bit<8;++bit) checksum=checksum>>1^((checksum&1)?0xedb88320u:0); }
    rom.assign(data,data+size); crc=~checksum; identity=hash; reset(); return true;
}
void Machine::reset() {
    std::fill(fcram.begin(),fcram.end(),0); std::fill(axi.begin(),axi.end(),0); std::fill(private9.begin(),private9.end(),0);
    frames=0; done=0; keys=0;
    for(unsigned id=0;id<3;++id) {
        cpu(id).reset(0); if(rom.empty()) continue;
        const auto s=segment(rom.data()+16+id*16);
        std::copy_n(rom.data()+s.offset,s.size,byte(id,s.address)); cpu(id).reset(s.entry);
    }
}
void Machine::set_input(uint32_t buttons) {
    static constexpr unsigned virtual_bits[]={0,1,7,6,11,10,8,9,5,4,2,3};
    keys=0; for(unsigned n=0;n<12;++n) if(buttons&(1u<<virtual_bits[n])) keys|=1u<<n;
}
bool Machine::read(unsigned id,uint32_t address,unsigned width,uint32_t& value) {
    if(id>2 || (width!=1 && width!=2 && width!=4) || uint64_t(address)+width>0x100000000ull) return false;
    if(address>=0x10146000 && uint64_t(address)+width<=0x10146002 && !(address&(width-1))) {
        value=((~keys&0xfffu)>>((address-0x10146000)*8))&(width==1?255u:65535u); return true;
    }
    uint32_t result=0;
    for(unsigned n=0;n<width;++n) { const auto* p=byte(id,address+n); if(!p) return false; result|=uint32_t(*p)<<(n*8); }
    value=result; return true;
}
bool Machine::write(unsigned id,uint32_t address,unsigned width,uint32_t value) {
    if(id>2 || (width!=1 && width!=2 && width!=4) || uint64_t(address)+width>0x100000000ull) return false;
    if(address==0x1fe00000 && width==1) { const char c=char(value); if(sink) sink->write(&c,1); return true; }
    if(address==0x1fe00008 && width==4) {
        if(sink) { static constexpr char digits[]="0123456789abcdef"; char text[8]; for(unsigned n=0;n<8;++n) text[n]=digits[(value>>(28-n*4))&15]; sink->write(text,8); } return true;
    }
    if(address==0x1fe00004 && width==4) { done|=1u<<id; if(done==7 && sink) sink->finish(-1); return true; }
    if(address>=0x10146000 && uint64_t(address)+width<=0x10146002 && !(address&(width-1))) return true;
    uint8_t* targets[4]{};
    for(unsigned n=0;n<width;++n) { targets[n]=byte(id,address+n); if(!targets[n]) return false; }
    for(unsigned n=0;n<width;++n) *targets[n]=uint8_t(value>>(n*8));
    return true;
}
void Machine::frame() {
    if(rom.empty()) return;
    // One attempt per active context per quantum; hardware timing/boot are pending.
    for(unsigned n=0;n<100000 && done!=7;++n) {
        bool running=false;
        for(unsigned id=0;id<3;++id) if(!(done&(1u<<id)) && !cpu(id).state.unsupported) { cpu(id).step(); running=true; }
        if(!running) break;
    }
    ++frames;
}
static void append(std::vector<uint8_t>& out,uint64_t value) { for(unsigned n=0;n<8;++n) out.push_back(uint8_t(value>>(56-n*8))); }
bool Machine::save(std::vector<uint8_t>& out) const {
    if(rom.empty()) return false;
    out.assign({'V','3','D','S','0','0','0','1'});
    for(auto value:{identity,uint64_t(crc),frames,uint64_t(done),uint64_t(keys)}) append(out,value);
    for(unsigned id=0;id<3;++id) {
        for(auto r:cpu(id).state.r) append(out,r);
        append(out,cpu(id).state.cpsr); append(out,cpu(id).state.steps); append(out,cpu(id).state.unsupported);
    }
    for(const auto* ram:{&fcram,&axi,&private9}) out.insert(out.end(),ram->begin(),ram->end());
    return true;
}
bool Machine::restore(const std::vector<uint8_t>& in) {
    const size_t prefix=504,expected=prefix+fcram.size()+axi.size()+private9.size();
    if(rom.empty() || in.size()!=expected || std::memcmp(in.data(),"V3DS0001",8)) return false;
    size_t position=8; const auto take=[&]() { uint64_t v=0; for(unsigned n=0;n<8;++n) v=v*256+in[position++]; return v; };
    if(take()!=identity || take()!=crc) return false;
    const auto frame=take(),finished=take(),input=take(); if(finished>7 || input>0xfff) return false;
    nds::ArmState states[3];
    for(unsigned id=0;id<3;++id) {
        for(auto& r:states[id].r) { const auto value=take(); if(value>0xffffffff) return false; r=uint32_t(value); }
        const auto psw=take(); if((psw&~(id<2?0xf80f0020ull:0xf8000020ull))!=0xdf || (states[id].r[15]&((psw&0x20)?1u:3u))) return false;
        states[id].cpsr=uint32_t(psw); states[id].steps=take(); const auto stopped=take(); if(stopped>1) return false; states[id].unsupported=stopped;
    }
    frames=frame; done=uint8_t(finished); keys=uint16_t(input);
    for(unsigned id=0;id<3;++id) cpu(id).state=states[id];
    for(auto* ram:{&fcram,&axi,&private9}) { std::copy_n(in.begin()+position,ram->size(),ram->begin()); position+=ram->size(); }
    return true;
}
}
