#include "cpu.hpp"
namespace ppc {
static int64_t signed32(uint32_t v) { return v&0x80000000 ? int64_t(v)-0x100000000LL:v; }
void execute(State& s,Memory& bus) {
    if(s.unsupported) return;
    ++s.steps;
    uint32_t ins=0; const auto pc=s.pc;
    if((pc&3) || !bus.read(pc,4,ins)) { s.unsupported=true; return; }
    const unsigned op=ins>>26,rt=(ins>>21)&31,ra=(ins>>16)&31,rb=(ins>>11)&31;
    const uint32_t immediate=uint32_t(int64_t(int(ins&0xffff))-((ins&0x8000) ? 65536:0));
    const auto record_compare=[&](unsigned field,int64_t a,int64_t b) {
        const unsigned shift=28-field*4;
        const uint32_t result=(a<b ? 8u:a>b ? 4u:2u)|((s.xer>>31)&1);
        s.cr=(s.cr&~(15u<<shift))|(result<<shift);
    };
    const auto memory_access=[&](bool load,unsigned width,bool algebraic,bool reverse,bool update,uint32_t address) {
        // Read all old operands before either RT or an updated base is written.
        if((update && (!ra || (load && ra==rt))) || (address&(width-1))) return false;
        uint32_t value=s.r[rt];
        if(load) {
            if(!bus.read(address,width,value)) return false;
        }
        if(reverse) {
            uint32_t swapped=0;
            for(unsigned n=0;n<width;++n) { swapped=swapped*256+(value&255); value>>=8; }
            value=swapped;
        }
        if(algebraic && (value&0x8000)) value|=0xffff0000u;
        if(load) s.r[rt]=value;
        else if(!bus.write(address,width,value)) return false;
        if(update) s.r[ra]=address;
        return true;
    };
    if(op==7) s.r[rt]=uint32_t(signed32(s.r[ra])*signed32(immediate));
    else if(op==8 || op==12 || op==13) {
        const uint32_t source=s.r[ra]; // Unlike ADDI, carry immediates read GPR0.
        const uint64_t wide=uint64_t(op==8 ? ~source:source)+immediate+(op==8 ? 1:0);
        const uint32_t result=uint32_t(wide);
        s.xer=(s.xer&~0x20000000u)|(wide>>32 ? 0x20000000u:0);
        s.r[rt]=result;
        if(op==13) record_compare(0,signed32(result),0);
    }
    else if(op==10 || op==11) {
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
    else if(op>=32 && op<=45) {
        const bool load=op<36 || (op>=40 && op<=43);
        const unsigned width=op>=40 ? 2:op==34 || op==35 || op==38 || op==39 ? 1:4;
        const uint32_t address=(ra ? s.r[ra]:0)+immediate;
        if(!memory_access(load,width,op==42 || op==43,false,op&1,address)) { s.unsupported=true; return; }
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
        if(xo==23 || xo==55 || xo==87 || xo==119 || xo==151 || xo==183 || xo==215 || xo==247 ||
           xo==279 || xo==311 || xo==343 || xo==375 || xo==407 || xo==439 ||
           xo==534 || xo==790 || xo==662 || xo==918) {
            const bool reverse=xo==534 || xo==790 || xo==662 || xo==918;
            const bool load=xo==23 || xo==55 || xo==87 || xo==119 || xo==279 || xo==311 || xo==343 || xo==375 || xo==534 || xo==790;
            const unsigned width=xo==87 || xo==119 || xo==215 || xo==247 ? 1:
                xo==279 || xo==311 || xo==343 || xo==375 || xo==407 || xo==439 || xo==790 || xo==918 ? 2:4;
            const uint32_t address=(ra ? s.r[ra]:0)+s.r[rb];
            if((ins&1) || !memory_access(load,width,xo==343 || xo==375,reverse,!reverse && (xo&32),address)) { s.unsupported=true; return; }
        } else if(xo==0 || xo==32) {
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
        } else if(arithmetic==104 || arithmetic==202 || arithmetic==234 || arithmetic==200 || arithmetic==232) {
            if(rb) { s.unsupported=true; return; }
            const uint32_t source=s.r[ra]; const unsigned carry=(s.xer>>29)&1;
            const bool negate=arithmetic==104,subtract=arithmetic==200 || arithmetic==232;
            const bool minus_one=arithmetic==234 || arithmetic==232;
            const uint64_t wide=uint64_t(subtract ? ~source:source)+(minus_one ? 0xffffffffu:0)+carry;
            const uint32_t result=negate ? 0u-source:uint32_t(wide);
            const int64_t math=negate ? -signed32(source):subtract ? -signed32(source)+carry-1-(minus_one ? 1:0):signed32(source)+carry-(minus_one ? 1:0);
            if(!negate) s.xer=(s.xer&~0x20000000u)|(wide>>32 ? 0x20000000u:0);
            if(ins&0x400) {
                const bool overflow=math< -0x80000000LL || math>0x7fffffffLL;
                s.xer=(s.xer&~0x40000000u)|(overflow ? 0xc0000000u:0);
            }
            s.r[rt]=result;
            if(ins&1) record_compare(0,signed32(result),0);
        } else if(xo==75 || xo==11 || arithmetic==235 || arithmetic==491 || arithmetic==459) {
            const uint32_t a=s.r[ra],b=s.r[rb]; uint32_t result=0; bool overflow=false;
            if(xo==11) result=uint32_t((uint64_t(a)*b)>>32);
            else if(xo==75 || arithmetic==235) {
                const int64_t product=signed32(a)*signed32(b);
                result=xo==75 ? uint32_t(uint64_t(product)>>32):uint32_t(product);
                overflow=product< -0x80000000LL || product>0x7fffffffLL;
            } else {
                overflow=!b || (arithmetic==491 && a==0x80000000u && b==0xffffffffu);
                // Undefined division quotients use zero in this deterministic profile.
                if(!overflow) result=arithmetic==459 ? a/b:uint32_t(signed32(a)/signed32(b));
            }
            if(arithmetic!=75 && arithmetic!=11 && (ins&0x400)) {
                s.xer=(s.xer&~0x40000000u)|(overflow ? 0xc0000000u:0);
            }
            s.r[rt]=result;
            if(ins&1) record_compare(0,signed32(result),0);
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
        } else if(xo==24 || xo==536 || xo==792 || xo==824 || xo==954 || xo==922 || xo==26) {
            const bool unary=xo==954 || xo==922 || xo==26;
            if(unary && rb) { s.unsupported=true; return; }
            const uint32_t source=s.r[rt]; uint32_t result=0;
            if(xo==954 || xo==922) {
                const uint32_t mask=xo==954 ? 255u:65535u,sign=xo==954 ? 128u:32768u;
                result=source&mask; if(result&sign) result|=~mask;
            } else if(xo==26) {
                result=32;
                for(unsigned bit=0;bit<32;++bit) if(source&(0x80000000u>>bit)) { result=bit; break; }
            } else {
                const unsigned shift=xo==824 ? rb:s.r[rb]&63;
                const bool negative=(source&0x80000000u)!=0,algebraic=xo==792 || xo==824;
                if(xo==24) result=shift<32 ? source<<shift:0;
                else if(!algebraic) result=shift<32 ? source>>shift:0;
                else {
                    if(shift>=32) result=negative ? 0xffffffffu:0;
                    else if(!shift) result=source;
                    else result=(source>>shift)|(negative ? 0xffffffffu<<(32-shift):0);
                    const bool discarded=shift>=32 ? source!=0:shift && (source&((1u<<shift)-1));
                    s.xer=(s.xer&~0x20000000u)|(negative && discarded ? 0x20000000u:0);
                }
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
}
