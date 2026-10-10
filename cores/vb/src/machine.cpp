#include "machine.hpp"
#include <algorithm>
#include <cstring>

namespace vb {
bool Machine::load(const uint8_t* data,size_t size) {
    if(!data || size<1024 || size>16*1024*1024 || (size&(size-1)) ||
       std::memcmp(data+size-0x200,"VVBHB001",8)) return false;
    std::vector<uint8_t> image(data,data+size);
    uint32_t hash=0xffffffff; uint64_t id=14695981039346656037ull;
    for(auto byte:image) {
        hash^=byte;
        for(unsigned b=0;b<8;++b) hash=(hash>>1)^((hash&1) ? 0xedb88320u:0);
        id^=byte; id*=1099511628211ull;
    }
    rom=std::move(image); crc=~hash; identity=id; reset(); return true;
}
void Machine::reset() { ram.fill(0); cpu.reset(); frames=0; done=false; }
bool Machine::read(uint32_t address,unsigned bytes,uint32_t& value) {
    if(bytes!=1 && bytes!=2 && bytes!=4) return false;
    const uint32_t p=address&0x07ffffff;
    if(p>=0x05000000 && uint64_t(p)+bytes<=0x06000000) {
        value=0; for(unsigned n=0;n<bytes;++n) value|=uint32_t(ram[(p+n)&0xffff])<<(8*n);
        return true;
    }
    if(!rom.empty() && p>=0x07000000 && uint64_t(p)+bytes<=0x08000000) {
        value=0; for(unsigned n=0;n<bytes;++n) value|=uint32_t(rom[(p+n)&(rom.size()-1)])<<(8*n);
        return true;
    }
    return false;
}
bool Machine::write(uint32_t address,unsigned bytes,uint32_t value) {
    if(bytes!=1 && bytes!=2 && bytes!=4) return false;
    const uint32_t p=address&0x07ffffff;
    // Repository-only debug addresses in an unimplemented board region.
    if(p==0x04000000 && bytes==1) { const char c=char(value); if(sink) sink->write(&c,1); return true; }
    if(p==0x04000004 && bytes==4) { done=true; if(sink) sink->finish(-1); return true; }
    if(p<0x05000000 || uint64_t(p)+bytes>0x06000000) return false;
    for(unsigned n=0;n<bytes;++n) ram[(p+n)&0xffff]=uint8_t(value>>(8*n));
    return true;
}
bool Machine::compare_exchange(uint32_t address,uint32_t expected,uint32_t replacement,uint32_t& previous) {
    // This profile has one serialized CPU bus owner and no asynchronous DMA.
    // Keep the full RMW in one callback; future bus owners must arbitrate here.
    if(!read(address,4,previous)) return false;
    return write(address,4,previous==expected ? replacement:previous);
}
static void append(std::vector<uint8_t>& data,uint64_t value) {
    for(unsigned n=0;n<8;++n) data.push_back(uint8_t(value>>(56-8*n)));
}
bool Machine::save(std::vector<uint8_t>& data) const {
    if(rom.empty()) return false;
    data.assign({'V','V','B','S','0','0','0','1'});
    for(auto value:{identity,uint64_t(crc),frames,uint64_t(done)}) append(data,value);
    for(auto value:cpu.state.gpr) append(data,value);
    for(auto value:cpu.state.sr) append(data,value);
    for(auto value:{uint64_t(cpu.state.pc),cpu.state.steps,uint64_t(cpu.state.halted),uint64_t(cpu.state.unsupported)}) append(data,value);
    data.insert(data.end(),ram.begin(),ram.end()); return true;
}
bool Machine::restore(const std::vector<uint8_t>& data) {
    constexpr size_t prefix=8+(4+32+32+4)*8;
    if(rom.empty() || data.size()!=prefix+ram.size() || std::memcmp(data.data(),"VVBS0001",8)) return false;
    size_t pos=8;
    auto take=[&]() { uint64_t value=0; for(unsigned n=0;n<8;++n) value=(value<<8)|data[pos++]; return value; };
    if(take()!=identity || take()!=crc) return false;
    const auto saved_frames=take(),saved_done=take();
    CpuState c; bool valid=true;
    for(auto& value:c.gpr) { auto v=take(); valid&=v<=0xffffffff; value=uint32_t(v); }
    for(auto& value:c.sr) { auto v=take(); valid&=v<=0xffffffff; value=uint32_t(v); }
    const auto pc=take(); c.pc=uint32_t(pc); c.steps=take(); const auto halted=take(),unsupported=take();
    if(!valid || pc>0xffffffff || (pc&1) || saved_done>1 || halted>1 || unsupported>1 || c.gpr[0] ||
       (c.sr[5]&~0xff3ffu) || (c.sr[0]&1) || (c.sr[2]&1) || (c.sr[25]&1)) return false;
    c.halted=halted; c.unsupported=unsupported;
    cpu.state=c; frames=saved_frames; done=saved_done;
    std::copy_n(data.begin()+pos,ram.size(),ram.begin()); return true;
}
}
