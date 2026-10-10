#include "machine.hpp"
#include <algorithm>
#include <cstring>
namespace wiiu {
static uint32_t be(const uint8_t* p) { uint32_t v=0;for(unsigned n=0;n<4;++n)v=v*256+p[n];return v; }
struct Segment { uint32_t offset,address,entry,size; };
static Segment segment(const uint8_t* p) { return {be(p),be(p+4),be(p+8),be(p+12)}; }
bool Machine::location(uint32_t a,unsigned w,size_t& offset) {
    if(w!=1 && w!=2 && w!=4) return false;
    if(uint64_t(a)+w<=0x02000000) { offset=a;return true; }
    if(a>=0x10000000 && uint64_t(a)+w<=0x90000000) { offset=size_t(a-0x10000000)+0x02000000;return true; }
    return false;
}
bool Machine::read(unsigned id,uint32_t a,unsigned w,uint32_t& value) const {
    if(id>2) return false;
    if(a==0xffe0000c && w==4) { value=buttons;return true; } // Original input probe convention.
    size_t offset=0;if(!location(a,w,offset))return false;
    uint32_t result=0;
    for(unsigned n=0;n<w;++n) { const auto& page=pages[(offset+n)/4096];result=result*256+(page?(*page)[(offset+n)%4096]:0); }
    value=result;return true;
}
bool Machine::store(uint32_t a,unsigned w,uint32_t value) {
    size_t offset=0;if(!location(a,w,offset))return false;
    for(unsigned n=0;n<w;++n) {
        auto& page=pages[(offset+n)/4096];const uint8_t byte=uint8_t(value>>(8*(w-n-1)));
        if(!page && byte)page=std::make_unique<Page>();
        if(page)(*page)[(offset+n)%4096]=byte;
    }
    return true;
}
bool Machine::write(unsigned id,uint32_t a,unsigned w,uint32_t value) {
    if(id>2)return false;
    if(a==0xffe00000 && w==1) { const char c=char(value);if(sink)sink->write(&c,1);return true; }
    if(a==0xffe00008 && w==4) {
        if(sink) { constexpr char digits[]="0123456789abcdef";char text[8];for(unsigned n=0;n<8;++n)text[n]=digits[(value>>(28-n*4))&15];sink->write(text,8); }return true;
    }
    if(a==0xffe00004 && w==4) { done|=1u<<id;if(done==7 && sink)sink->finish(-1);return true; }
    if(a==0xffe0000c && w==4)return true;
    return store(a,w,value);
}
bool Machine::load(const uint8_t* data,size_t size) {
    if(!data || size<64 || size>0x04000000 || std::memcmp(data,"VWUP0001",8))return false;
    for(unsigned n=8;n<16;++n)if(data[n])return false;
    Segment regions[3];
    for(unsigned id=0;id<3;++id) {
        auto& s=regions[id];s=segment(data+16+id*16);size_t offset=0;
        const bool in_mem1=uint64_t(s.address)+s.size<=0x02000000;
        const bool in_mem2=s.address>=0x10000000 && uint64_t(s.address)+s.size<=0x90000000;
        if(s.offset<64 || (s.offset&3) || (s.address&3) || (s.entry&3) || !s.size || (s.size&3) ||
           uint64_t(s.offset)+s.size>size || s.entry<s.address || uint64_t(s.entry)+4>uint64_t(s.address)+s.size ||
           (!in_mem1 && !in_mem2) || !location(s.address,4,offset))return false;
        const auto overlaps=[](uint32_t a,uint32_t an,uint32_t b,uint32_t bn) { return uint64_t(a)<uint64_t(b)+bn && uint64_t(b)<uint64_t(a)+an; };
        for(unsigned prev=0;prev<id;++prev)if(overlaps(s.offset,s.size,regions[prev].offset,regions[prev].size) || overlaps(s.address,s.size,regions[prev].address,regions[prev].size))return false;
    }
    uint32_t checksum=0xffffffff;uint64_t hash=0xcbf29ce484222325ULL;
    for(size_t n=0;n<size;++n) { checksum^=data[n];hash=(hash^data[n])*0x100000001b3ULL;for(unsigned bit=0;bit<8;++bit)checksum=checksum>>1^((checksum&1)?0xedb88320u:0); }
    rom.assign(data,data+size);crc=~checksum;identity=hash;reset();return true;
}
void Machine::reset() {
    for(auto& page:pages)page.reset();
    cpus={};frames=0;done=0;buttons=0;
    if(rom.empty())return;
    for(unsigned id=0;id<3;++id) {
        const auto s=segment(rom.data()+16+id*16);cpus[id].pc=s.entry;
        for(unsigned n=0;n<s.size;++n)store(s.address+n,1,rom[s.offset+n]);
    }
}
void Machine::step(unsigned id) {
    if(id>2 || (done&(1u<<id)))return;
    ppc::execute(cpus[id],id==0?bus0:id==1?bus1:bus2);
}
void Machine::frame() {
    if(rom.empty())return;
    for(unsigned n=0;n<100000 && done!=7;++n) {
        bool running=false;
        for(unsigned id=0;id<3;++id)if(!(done&(1u<<id)) && !cpus[id].unsupported) { step(id);running=true; }
        if(!running)break;
    }
    ++frames;
}
bool Machine::active(const std::unique_ptr<Page>& page) { return page && !std::all_of(page->begin(),page->end(),[](uint8_t b){return !b;}); }
size_t Machine::state_size() const {
    if(rom.empty())return 0;
    size_t count=0;for(const auto& page:pages)if(active(page))++count;
    return 992+count*4100;
}
void Machine::save_prefix(uint8_t* out,size_t count) const {
    std::memcpy(out,"VWUS0001",8);size_t pos=8;
    const auto append=[&](uint64_t value) { for(unsigned n=0;n<8;++n)out[pos++]=uint8_t(value>>(56-n*8)); };
    for(auto value:{identity,uint64_t(crc),frames,uint64_t(done),uint64_t(buttons)})append(value);
    for(const auto& cpu:cpus) {
        for(auto value:cpu.r)append(value);
        for(uint64_t value:{uint64_t(cpu.pc),uint64_t(cpu.cr),uint64_t(cpu.xer),uint64_t(cpu.lr),uint64_t(cpu.ctr),cpu.steps,uint64_t(cpu.unsupported)})append(value);
    }
    append(count);
}
size_t Machine::save_into(uint8_t* out,size_t capacity) const {
    const size_t size=state_size();if(!size || !out)return size;
    if(capacity<size)return 0;
    save_prefix(out,(size-992)/4100);size_t pos=992;
    for(size_t index=0;index<pages.size();++index)if(active(pages[index])) {
        for(unsigned n=0;n<4;++n)out[pos++]=uint8_t(index>>(24-n*8));
        std::memcpy(out+pos,pages[index]->data(),4096);pos+=4096;
    }
    return size;
}
bool Machine::save(std::vector<uint8_t>& out) const {
    const size_t size=state_size();if(!size)return false;
    out.resize(size);return save_into(out.data(),out.size())==size;
}
uint64_t Machine::state_hash() const {
    const size_t size=state_size();if(!size)return 0;
    uint8_t prefix[992];save_prefix(prefix,(size-992)/4100);uint64_t hash=0xcbf29ce484222325ULL;
    const auto add=[&](const uint8_t* bytes,size_t count) { for(size_t n=0;n<count;++n)hash=(hash^bytes[n])*0x100000001b3ULL; };
    add(prefix,sizeof(prefix));
    for(size_t index=0;index<pages.size();++index)if(active(pages[index])) {
        uint8_t encoded[4];for(unsigned n=0;n<4;++n)encoded[n]=uint8_t(index>>(24-n*8));
        add(encoded,4);add(pages[index]->data(),4096);
    }
    return hash;
}
bool Machine::restore(const std::vector<uint8_t>& in) { return restore(in.data(),in.size()); }
bool Machine::restore(const uint8_t* in,size_t size) {
    constexpr size_t prefix=992;
    if(rom.empty() || !in || size<prefix || std::memcmp(in,"VWUS0001",8))return false;
    size_t pos=8;const auto take=[&](unsigned width=8) { uint64_t v=0;for(unsigned n=0;n<width;++n)v=v*256+in[pos++];return v; };
    if(take()!=identity || take()!=crc)return false;
    const auto frame=take(),finished=take(),input=take();if(finished>7 || input>0xfff)return false;
    std::array<ppc::State,3> states{};
    for(auto& cpu:states) {
        for(auto& r:cpu.r) { const auto value=take();if(value>0xffffffff)return false;r=uint32_t(value); }
        for(auto* r:{&cpu.pc,&cpu.cr,&cpu.xer,&cpu.lr,&cpu.ctr}) { const auto value=take();if(value>0xffffffff)return false;*r=uint32_t(value); }
        cpu.steps=take();const auto stopped=take();if(stopped>1 || (cpu.pc&3) || (cpu.xer&~0xe000007fu))return false;cpu.unsupported=stopped;
    }
    const auto count=take();if(count>page_count || size!=prefix+count*4100)return false;
    std::vector<std::unique_ptr<Page>> replacement(page_count);uint64_t previous=0;
    for(uint64_t n=0;n<count;++n) {
        const auto index=take(4);if(index>=page_count || (n && index<=previous))return false;
        auto page=std::make_unique<Page>();std::memcpy(page->data(),in+pos,4096);pos+=4096;
        if(std::all_of(page->begin(),page->end(),[](uint8_t b){return !b;}))return false;
        replacement[index]=std::move(page);previous=index;
    }
    pages.swap(replacement);cpus=states;frames=frame;done=uint8_t(finished);buttons=uint32_t(input);return true;
}
}
