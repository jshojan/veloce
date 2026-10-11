#include "machine.hpp"
#include <algorithm>
#include <cstring>
namespace ppc {
static uint32_t be(const uint8_t* p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
bool Machine::location(uint32_t address,unsigned width,size_t& offset) const {
    if(width!=1 && width!=2 && width!=4) return false;
    const uint32_t bank=address&0xe0000000u;
    if(bank!=0 && bank!=0x80000000 && bank!=0xc0000000) return false;
    offset=address&0x1fffffffu;
    return uint64_t(offset)+width<=ram.size();
}
bool Machine::load(const uint8_t* data,size_t size) {
    if(!data || size<36 || size>ram.size()+32 || std::memcmp(data,wii ? "VWIP0001":"VGCP0001",8)) return false;
    const auto address=be(data+8),entry=be(data+12),length=be(data+16);
    size_t offset=0;
    if(address<0x80000000 || address>=0x81800000 || (address&3) || (entry&3) || !length || (length&3) ||
       uint64_t(length)+32!=size || entry<address || uint64_t(entry)+4>uint64_t(address)+length ||
       !location(address,4,offset) || uint64_t(offset)+length>ram.size()) return false;
    for(unsigned n=20;n<32;++n) if(data[n]) return false;
    uint32_t checksum=0xffffffff; uint64_t hash=0xcbf29ce484222325ULL;
    for(size_t n=0;n<size;++n) { const auto b=data[n]; hash=(hash^b)*0x100000001b3ULL; checksum^=b; for(unsigned bit=0;bit<8;++bit) checksum=checksum>>1^((checksum&1) ? 0xedb88320u:0); }
    rom.assign(data,data+size); crc=~checksum; identity=hash; reset(); return true;
}
void Machine::reset() {
    cpu={}; serial.reset(); frames=0; done=false; std::fill(ram.begin(),ram.end(),0);
    if(rom.empty()) return;
    const auto address=be(rom.data()+8); cpu.pc=be(rom.data()+12);
    std::copy(rom.begin()+32,rom.end(),ram.begin()+(address-0x80000000));
}
bool Machine::read(uint32_t address,unsigned width,uint32_t& value) const {
    if((address&0xffffff00u)==(wii?0xcd006400u:0xcc006400u))
        return width==4 && !(address&3) && serial.read(address&255,value);
    size_t offset=0; if(!location(address,width,offset)) return false;
    uint32_t result=0; for(unsigned n=0;n<width;++n) result=result<<8|ram[offset+n]; value=result; return true;
}
bool Machine::write(uint32_t address,unsigned width,uint32_t value) {
    if((address&0xffffff00u)==(wii?0xcd006400u:0xcc006400u))
        return width==4 && !(address&3) && serial.write(address&255,value);
    if(address==0xfff00000 && width==1) { const char c=char(value); if(sink) sink->write(&c,1); return true; }
    if(address==0xfff00008 && width==4) {
        if(sink) { static constexpr char digits[]="0123456789abcdef"; char text[8]; for(unsigned n=0;n<8;++n) text[n]=digits[(value>>(28-n*4))&15]; sink->write(text,8); } return true;
    }
    if(address==0xfff00004 && width==4) { done=true; if(sink) sink->finish(-1); return true; }
    size_t offset=0; if(!location(address,width,offset)) return false;
    for(unsigned n=0;n<width;++n) ram[offset+n]=uint8_t(value>>(8*(width-n-1)));
    return true;
}
void Machine::step() { if(!done) execute(cpu,*this); }
void Machine::frame() { if(rom.empty()) return; for(unsigned n=0;n<100000 && !done && !cpu.unsupported;++n) step(); ++frames; }
void Machine::header(uint8_t* out) const {
    std::memcpy(out,"VPPC0002",8);size_t pos=8;
    const auto put=[&](uint64_t v){for(unsigned n=0;n<8;++n)out[pos++]=uint8_t(v>>(56-n*8));};
    for(uint64_t v:{uint64_t(wii),identity,uint64_t(crc),frames,uint64_t(done)})put(v);
    for(auto v:cpu.r)put(v);
    for(uint64_t v:{uint64_t(cpu.pc),uint64_t(cpu.cr),uint64_t(cpu.xer),uint64_t(cpu.lr),uint64_t(cpu.ctr),cpu.steps,uint64_t(cpu.unsupported)})put(v);
}
size_t Machine::save_into(uint8_t* out,size_t capacity) const {
    if(rom.empty())return 0;
    if(!out)return state_size;
    if(capacity<state_size)return 0;
    header(out);std::memcpy(out+360,ram.data(),ram.size());
    const auto peripheral=serial.save();std::memcpy(out+360+ram.size(),peripheral.data(),peripheral.size());return state_size;
}
bool Machine::save(std::vector<uint8_t>& out) const {
    if(rom.empty())return false;
    out.resize(state_size);return save_into(out.data(),out.size())==state_size;
}
uint64_t Machine::state_hash() const {
    if(rom.empty())return 0;
    std::array<uint8_t,360> bytes{};header(bytes.data());uint64_t hash=0xcbf29ce484222325ull;
    const auto add=[&](uint8_t byte){hash=(hash^byte)*0x100000001b3ull;};
    for(auto byte:bytes)add(byte);
    for(auto byte:ram)add(byte);
    for(auto byte:serial.save())add(byte);
    return hash;
}
bool Machine::restore(const std::vector<uint8_t>& in) {return restore(in.data(),in.size());}
bool Machine::restore(const uint8_t* in,size_t size) {
    // 44 big endian 64-bit scalars follow the eight-byte state magic.
    if(rom.empty() || !in || size<8) return false;
    const bool legacy=std::memcmp(in,"VPPC0001",8)==0;
    if((!legacy && std::memcmp(in,"VPPC0002",8)) || size!=360+ram.size()+(legacy?0:Serial::state_size)) return false;
    size_t pos=8; const auto take=[&]() { uint64_t value=0; for(unsigned n=0;n<8;++n) value=value<<8|in[pos++]; return value; };
    if(take()!=uint64_t(wii) || take()!=identity || take()!=crc) return false;
    const auto frame=take(),finished=take(); if(finished>1) return false;
    State state;
    for(auto& reg:state.r) { const auto value=take(); if(value>0xffffffff) return false; reg=uint32_t(value); }
    for(auto* reg:{&state.pc,&state.cr,&state.xer,&state.lr,&state.ctr}) { const auto value=take(); if(value>0xffffffff) return false; *reg=uint32_t(value); }
    state.steps=take(); const auto stopped=take(); if(stopped>1 || (state.pc&3) || (state.xer&~0xe000007fu)) return false; state.unsupported=stopped;
    Serial peripheral;
    if(!legacy && !peripheral.restore(in+pos+ram.size(),Serial::state_size)) return false;
    cpu=state; serial=peripheral; frames=frame; done=finished; std::copy_n(in+pos,ram.size(),ram.begin()); return true;
}
}
