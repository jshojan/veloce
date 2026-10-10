#include "machine.hpp"
#include <algorithm>
#include <cstring>
namespace ppc {
static uint32_t be(const uint8_t* p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
static int64_t signed32(uint32_t v) { return v&0x80000000 ? int64_t(v)-0x100000000LL:v; }
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
    cpu={}; frames=0; done=false; std::fill(ram.begin(),ram.end(),0);
    if(rom.empty()) return;
    const auto address=be(rom.data()+8); cpu.pc=be(rom.data()+12);
    std::copy(rom.begin()+32,rom.end(),ram.begin()+(address-0x80000000));
}
bool Machine::read(uint32_t address,unsigned width,uint32_t& value) const {
    size_t offset=0; if(!location(address,width,offset)) return false;
    uint32_t result=0; for(unsigned n=0;n<width;++n) result=result<<8|ram[offset+n]; value=result; return true;
}
bool Machine::write(uint32_t address,unsigned width,uint32_t value) {
    if(address==0xfff00000 && width==1) { const char c=char(value); if(sink) sink->write(&c,1); return true; }
    if(address==0xfff00008 && width==4) {
        if(sink) { static constexpr char digits[]="0123456789abcdef"; char text[8]; for(unsigned n=0;n<8;++n) text[n]=digits[(value>>(28-n*4))&15]; sink->write(text,8); } return true;
    }
    if(address==0xfff00004 && width==4) { done=true; if(sink) sink->finish(-1); return true; }
    size_t offset=0; if(!location(address,width,offset)) return false;
    for(unsigned n=0;n<width;++n) ram[offset+n]=uint8_t(value>>(8*(width-n-1)));
    return true;
}
void Machine::step() {
    auto& s=cpu; if(s.unsupported || done) return; ++s.steps;
    uint32_t ins=0; const auto pc=s.pc;
    if((pc&3) || !read(pc,4,ins)) { s.unsupported=true; return; }
    const unsigned op=ins>>26,rt=(ins>>21)&31,ra=(ins>>16)&31,rb=(ins>>11)&31;
    const uint32_t immediate=uint32_t(int64_t(int(ins&0xffff))-((ins&0x8000) ? 65536:0));
    const auto record_compare=[&](unsigned field,int64_t a,int64_t b) {
        const unsigned shift=28-field*4;
        const uint32_t result=(a<b ? 8u:a>b ? 4u:2u)|((s.xer>>31)&1);
        s.cr=(s.cr&~(15u<<shift))|(result<<shift);
    };
    if(op==10 || op==11) {
        if(rt&3) { s.unsupported=true; return; } // Reserved bit and unsupported L=1.
        record_compare(rt>>2,op==11 ? signed32(s.r[ra]):s.r[ra],op==11 ? signed32(immediate):ins&65535);
    }
    else if(op==14 || op==15) s.r[rt]=(ra ? s.r[ra]:0)+(op==15 ? immediate<<16:immediate);
    else if(op==20 || op==21 || op==23) {
        const unsigned shift=op==23 ? s.r[rb]&31:rb;
        const unsigned begin=(ins>>6)&31,end=(ins>>1)&31;
        // PowerPC numbers mask bits from the most significant end, inclusively.
        const uint32_t mask=begin<=end ? (0xffffffffu>>begin)&(0xffffffffu<<(31-end)):
            (0xffffffffu>>begin)|(0xffffffffu<<(31-end));
        const uint32_t source=s.r[rt],old=s.r[ra];
        const uint32_t rotated=shift ? (source<<shift)|(source>>(32-shift)):source;
        const uint32_t result=(rotated&mask)|(op==20 ? old&~mask:0);
        s.r[ra]=result;
        if(ins&1) record_compare(0,signed32(result),0);
    }
    else if(op>=24 && op<=29) {
        const uint32_t operand=(ins&65535)<<((op&1) ? 16:0);
        const uint32_t source=s.r[rt]; // Logical instructions read ordinary GPR0.
        const uint32_t result=op<26 ? source|operand:op<28 ? source^operand:source&operand;
        s.r[ra]=result;
        if(op>=28) record_compare(0,signed32(result),0); // ANDI./ANDIS. always record.
    }
    else if(op==32 || op==34 || op==36 || op==38 || op==40 || op==44) {
        const bool load=op==32 || op==34 || op==40;
        const unsigned width=op==34 || op==38 ? 1:op==40 || op==44 ? 2:4;
        const uint32_t address=(ra ? s.r[ra]:0)+immediate;
        if(address&(width-1)) { s.unsupported=true; return; }
        uint32_t value=s.r[rt];
        if(!(load ? read(address,width,value):write(address,width,value))) { s.unsupported=true; return; }
        if(load) s.r[rt]=value;
    } else if(op==18) {
        uint32_t delta=ins&0x03fffffc; if(delta&0x02000000) delta|=0xfc000000;
        if(ins&1) s.lr=pc+4;
        s.pc=(ins&2) ? delta:pc+delta; return;
    } else if(op==16 || (op==19 && (((ins>>1)&1023)==16 || ((ins>>1)&1023)==528))) {
        const unsigned bo=rt,bi=ra;
        const bool indirect=op==19,to_count=indirect && ((ins>>1)&1023)==528;
        // The supplied 750 profile uses the original XL form, without BH.
        // An invalid form must not decrement CTR or overwrite LR.
        if((indirect && rb) || (to_count && !(bo&4))) { s.unsupported=true; return; }
        uint32_t target;
        if(indirect) target=(to_count ? s.ctr:s.lr)&~3u;
        else {
            uint32_t delta=ins&0xfffc; if(delta&0x8000) delta|=0xffff0000u;
            target=(ins&2) ? delta:pc+delta;
        }
        if(!(bo&4)) --s.ctr;
        const bool count_ok=(bo&4) || ((s.ctr!=0) != bool(bo&2));
        const bool condition_ok=(bo&16) || (((s.cr>>(31-bi))&1)==unsigned(bool(bo&8)));
        // LK updates LR on both taken and untaken paths, after reading targets.
        if(ins&1) s.lr=pc+4;
        s.pc=count_ok && condition_ok ? target:pc+4; return;
    } else if(op==19) {
        const unsigned xo=(ins>>1)&1023;
        if(ins&1) { s.unsupported=true; return; }
        if(xo==0) {
            if((rt&3) || (ra&3) || rb) { s.unsupported=true; return; }
            const unsigned shift=28-(rt>>2)*4,source=28-(ra>>2)*4;
            const uint32_t value=(s.cr>>source)&15;
            s.cr=(s.cr&~(15u<<shift))|(value<<shift);
        } else {
            const bool a=(s.cr>>(31-ra))&1,b=(s.cr>>(31-rb))&1;
            bool result;
            switch(xo) {
                case 257: result=a && b; break;
                case 129: result=a && !b; break;
                case 289: result=a==b; break;
                case 225: result=!(a && b); break;
                case 33: result=!(a || b); break;
                case 449: result=a || b; break;
                case 417: result=a || !b; break;
                case 193: result=a!=b; break;
                default: s.unsupported=true; return;
            }
            const uint32_t mask=1u<<(31-rt);
            s.cr=(s.cr&~mask)|(result ? mask:0);
        }
    } else if(op==31) {
        const unsigned xo=(ins>>1)&1023,arithmetic=xo&511;
        if(xo==0 || xo==32) {
            if((rt&3) || (ins&1)) { s.unsupported=true; return; }
            record_compare(rt>>2,xo==0 ? signed32(s.r[ra]):s.r[ra],xo==0 ? signed32(s.r[rb]):s.r[rb]);
        } else if(xo==512) {
            if(ins&0x007ff801u) { s.unsupported=true; return; }
            const unsigned shift=28-(rt>>2)*4;
            s.cr=(s.cr&~(15u<<shift))|((s.xer>>28)<<shift);
            s.xer&=0x0fffffffu;
        } else if(arithmetic==266 || arithmetic==10 || arithmetic==138 || arithmetic==40 || arithmetic==8 || arithmetic==136) {
            const bool subtract=arithmetic==40 || arithmetic==8 || arithmetic==136;
            const bool extended=arithmetic==138 || arithmetic==136;
            const unsigned carry=(s.xer>>29)&1;
            const uint32_t a=s.r[ra],b=s.r[rb];
            // Complemented subtraction exposes the architectural no-borrow carry.
            const uint64_t wide=uint64_t(subtract ? ~a:a)+b+(extended ? carry:subtract ? 1:0);
            const uint32_t result=uint32_t(wide);
            const int64_t math=subtract ? signed32(b)-signed32(a)-(extended ? 1-carry:0):signed32(a)+signed32(b)+(extended ? carry:0);
            if(arithmetic!=266 && arithmetic!=40) s.xer=(s.xer&~0x20000000u)|(wide>>32 ? 0x20000000u:0);
            // OE updates OV and only sets sticky SO; Rc observes the new SO.
            if(ins&0x400) {
                const bool overflow=math< -0x80000000LL || math>0x7fffffffLL;
                s.xer=(s.xer&~0x40000000u)|(overflow ? 0xc0000000u:0);
            }
            s.r[rt]=result;
            if(ins&1) s.cr=(s.cr&0x0fffffffu)|((result==0 ? 2u:result&0x80000000 ? 8u:4u)|((s.xer>>31)&1))<<28;
        } else if(xo==28 || xo==60 || xo==444 || xo==412 || xo==316 || xo==476 || xo==124 || xo==284) {
            // X-form logical destinations are RA; read both old sources before writing.
            const uint32_t a=s.r[rt],b=s.r[rb]; uint32_t result=0;
            switch(xo) {
                case 28: result=a&b; break;
                case 60: result=a&~b; break;
                case 444: result=a|b; break;
                case 412: result=a|~b; break;
                case 316: result=a^b; break;
                case 476: result=~(a&b); break;
                case 124: result=~(a|b); break;
                case 284: result=~(a^b); break;
            }
            s.r[ra]=result;
            if(ins&1) record_compare(0,signed32(result),0);
        } else if(xo==339 || xo==467) {
            if(ins&1) { s.unsupported=true; return; }
            const unsigned spr=((ins>>16)&31)|((ins>>6)&992);
            uint32_t* target=spr==1 ? &s.xer:spr==8 ? &s.lr:spr==9 ? &s.ctr:nullptr;
            if(!target) { s.unsupported=true; return; }
            if(xo==339) s.r[rt]=*target;
            else *target=spr==1 ? s.r[rt]&0xe000007fu:s.r[rt];
        } else if(xo==19 && !(ins&0x001ff801)) s.r[rt]=s.cr;
        else if(xo==144 && !(ins&0x00100801)) {
            const unsigned mask=(ins>>12)&255; for(unsigned field=0;field<8;++field) if(mask&(128>>field)) {
                const uint32_t bits=0xf0000000u>>(field*4); s.cr=(s.cr&~bits)|(s.r[rt]&bits);
            }
        } else { s.unsupported=true; return; }
    } else { s.unsupported=true; return; }
    s.pc=pc+4;
}
void Machine::frame() { if(rom.empty()) return; for(unsigned n=0;n<100000 && !done && !cpu.unsupported;++n) step(); ++frames; }
static void append(std::vector<uint8_t>& out,uint64_t value) { for(int n=7;n>=0;--n) out.push_back(uint8_t(value>>(n*8))); }
bool Machine::save(std::vector<uint8_t>& out) const {
    if(rom.empty()) return false;
    out.assign({'V','P','P','C','0','0','0','1'});
    for(uint64_t value:{uint64_t(wii),identity,uint64_t(crc),frames,uint64_t(done)}) append(out,value);
    for(auto value:cpu.r) append(out,value);
    for(uint64_t value:{uint64_t(cpu.pc),uint64_t(cpu.cr),uint64_t(cpu.xer),uint64_t(cpu.lr),uint64_t(cpu.ctr),cpu.steps,uint64_t(cpu.unsupported)}) append(out,value);
    out.insert(out.end(),ram.begin(),ram.end()); return true;
}
bool Machine::restore(const std::vector<uint8_t>& in) {
    // 44 big endian 64-bit scalars follow the eight-byte state magic.
    if(rom.empty() || in.size()!=360+ram.size() || std::memcmp(in.data(),"VPPC0001",8)) return false;
    size_t pos=8; const auto take=[&]() { uint64_t value=0; for(unsigned n=0;n<8;++n) value=value<<8|in[pos++]; return value; };
    if(take()!=uint64_t(wii) || take()!=identity || take()!=crc) return false;
    const auto frame=take(),finished=take(); if(finished>1) return false;
    State state;
    for(auto& reg:state.r) { const auto value=take(); if(value>0xffffffff) return false; reg=uint32_t(value); }
    for(auto* reg:{&state.pc,&state.cr,&state.xer,&state.lr,&state.ctr}) { const auto value=take(); if(value>0xffffffff) return false; *reg=uint32_t(value); }
    state.steps=take(); const auto stopped=take(); if(stopped>1 || (state.pc&3) || (state.xer&~0xe000007fu)) return false; state.unsupported=stopped;
    cpu=state; frames=frame; done=finished; std::copy(in.begin()+pos,in.end(),ram.begin()); return true;
}
}
