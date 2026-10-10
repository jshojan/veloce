struct MultiplyChecks { uint64_t rows=0,undefined=0; };
// Four 16-bit product limbs and restoring integer division are independent references.
static MultiplyChecks multiply_checks(ppc::Machine& m) {
    constexpr uint32_t values[]={0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef};
    constexpr unsigned xos[]={75,11,235,491,459};
    constexpr uint32_t seeds[]={0,0xffffffff,0xa5a5a5a5,0x12345678};
    MultiplyChecks counts;
    const auto quotient=[](uint32_t a,uint32_t b) {
        uint32_t result=0; uint64_t remainder=0;
        for(int bit=31;bit>=0;--bit) {
            remainder=remainder*2+((a>>bit)&1);
            if(remainder>=b) { remainder-=b; result|=1u<<bit; }
        }
        return result;
    };
    const auto check=[&](uint32_t ins,uint32_t a,uint32_t b,uint32_t cr,uint32_t xer) {
        const unsigned op=ins>>26,xo=(ins>>1)&511,dst=(ins>>21)&31,ra=(ins>>16)&31,rb=(ins>>11)&31;
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x13579bdfu+r;
        m.cpu.r[ra]=a; if(op==31) m.cpu.r[rb]=b;
        auto registers=m.cpu.r; a=registers[ra];
        if(op==31) b=registers[rb];
        else b=uint32_t(int64_t(ins%65536)-(ins%65536>=32768?65536:0));
        const uint64_t lo_a=a%65536,hi_a=a/65536,lo_b=b%65536,hi_b=b/65536;
        const uint64_t product=lo_a*lo_b+((lo_a*hi_b+hi_a*lo_b)*65536)+(hi_a*hi_b*0x100000000ULL);
        const uint32_t low=uint32_t(product),unsigned_high=uint32_t(product/0x100000000ULL);
        const uint32_t signed_high=unsigned_high-(a>=0x80000000u?b:0)-(b>=0x80000000u?a:0);
        bool undefined=false,overflow=false; uint32_t result=low;
        if(op==31) {
            if(xo==75) result=signed_high;
            else if(xo==11) result=unsigned_high;
            else if(xo==235) overflow=signed_high!=(low>=0x80000000u?0xffffffffu:0);
            else {
                undefined=b==0 || (xo==491 && a==0x80000000u && b==0xffffffffu);
                overflow=undefined; result=0;
                if(!undefined) {
                    const bool negative=xo==491 && ((a>=0x80000000u)!=(b>=0x80000000u));
                    const uint32_t ma=xo==491 && a>=0x80000000u?0u-a:a,mb=xo==491 && b>=0x80000000u?0u-b:b;
                    const auto magnitude=quotient(ma,mb); result=negative?0u-magnitude:magnitude;
                }
            }
        }
        uint32_t wantxer=xer;
        if(op==31 && xo!=75 && xo!=11 && (ins&0x400)) {
            wantxer=(xer&0xbfffffffu)+(overflow?0x40000000u:0);
            if(overflow) wantxer|=0x80000000u;
        }
        const uint32_t nibble=(result==0?2u:result>=0x80000000u?8u:4u)+wantxer/0x80000000u;
        const uint32_t wantcr=op==31 && (ins&1)?cr%0x10000000u+nibble*0x10000000u:cr;
        registers[dst]=result;
        m.cpu.cr=cr; m.cpu.xer=xer; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        execute(m,ins);
        require(!m.cpu.unsupported && m.cpu.pc==0x80002004 && m.cpu.steps==1,"multiply/divide decode/PC");
        // Undefined quotient and CR0 LT/GT/EQ are deterministic profile fixtures here,
        // separately counted from defined hardware result checks.
        require(m.cpu.r==registers && m.cpu.cr==wantcr && m.cpu.xer==wantxer && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"multiply/divide registers/flags");
        ++counts.rows; if(undefined) ++counts.undefined;
    };
    for(auto xo:xos) for(unsigned ai=0;ai<16;++ai) for(unsigned bi=0;bi<16;++bi) for(unsigned flags=0;flags<8;++flags)
        for(unsigned oe=0;oe<(xo==75 || xo==11?1u:2u);++oe) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|11<<21|3<<16|10<<11|oe<<10|xo<<1|rc,values[ai],values[bi],seeds[(ai+bi+flags+oe+rc)%4],flags<<29|127);
    for(unsigned ai=0;ai<16;++ai) for(unsigned flags=0;flags<8;++flags) for(unsigned imm=0;imm<65536;++imm)
        check(7u<<26|11<<21|3<<16|imm,values[ai],0x2468ace0,seeds[(ai+flags)%4],flags<<29|127);
    for(auto xo:xos) for(unsigned dst=0;dst<32;++dst) for(unsigned ra=0;ra<32;++ra) for(unsigned rb=0;rb<32;++rb)
        for(unsigned flags=0;flags<8;++flags) for(unsigned oe=0;oe<(xo==75 || xo==11?1u:2u);++oe) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|dst<<21|ra<<16|rb<<11|oe<<10|xo<<1|rc,values[ra%16],values[(rb+7)%16],seeds[(dst+ra+rb)%4],flags<<29|127);
    constexpr unsigned immediates[]={0,1,0x7fff,0x8000,0xffff,0x5555,0xaaaa,0x89ab};
    for(unsigned dst=0;dst<32;++dst) for(unsigned ra=0;ra<32;++ra) for(auto imm:immediates) for(unsigned flags=0;flags<8;++flags)
        check(7u<<26|dst<<21|ra<<16|imm,values[ra%16],0x2468ace0,seeds[(dst+ra)%4],flags<<29|127);
    for(auto xo:xos) for(unsigned abit=0;abit<32;++abit) for(unsigned bbit=0;bbit<32;++bbit) for(unsigned signs=0;signs<4;++signs)
        for(unsigned flags=0;flags<8;++flags) for(unsigned oe=0;oe<(xo==75 || xo==11?1u:2u);++oe) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|11<<21|3<<16|10<<11|oe<<10|xo<<1|rc,signs&1?~(1u<<abit):1u<<abit,signs&2?~(1u<<bbit):1u<<bbit,0xa5a5a5a5,flags<<29|127);
    for(auto xo:xos) for(unsigned bytes=0;bytes<128;++bytes) for(unsigned flags=0;flags<8;++flags)
        for(unsigned oe=0;oe<(xo==75 || xo==11?1u:2u);++oe) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|11<<21|3<<16|10<<11|oe<<10|xo<<1|rc,0x80000000,0xffffffff,0xa5a5a5a5,flags<<29|bytes);
    for(unsigned xo:{75u,11u}) for(unsigned dst=0;dst<32;++dst) for(unsigned ra=0;ra<32;++ra) for(unsigned rb=0;rb<32;++rb) for(unsigned rc=0;rc<2;++rc) {
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x89abcdefu+r;
        auto registers=m.cpu.r; m.cpu.cr=0xa5a5a5a5; m.cpu.xer=0xe000007f; m.cpu.lr=123; m.cpu.ctr=456;
        execute(m,31u<<26|dst<<21|ra<<16|rb<<11|1u<<10|xo<<1|rc);
        require(m.cpu.unsupported && m.cpu.pc==0x80002000 && m.cpu.r==registers && m.cpu.cr==0xa5a5a5a5 && m.cpu.xer==0xe000007f && m.cpu.lr==123 && m.cpu.ctr==456,"reserved multiply-high bit mutation");
    }
    return counts;
}
static void multiply_plugin_replay(emu::IEmulatorPlugin& plugin,bool wii) {
    auto rom=image(wii);
    const uint32_t words[]={15u<<26|3<<21|0x8000u,15u<<26|10<<21|0xffffu,24u<<26|10<<21|10<<16|65535u,
        15u<<26|9<<21|0x2000u,24u<<26|9<<21|9<<16|127u,spr(9,1,true),
        15u<<26|5<<21|0xa5a5u,24u<<26|5<<21|5<<16|0xa5a5u,31u<<26|5<<21|255<<12|144<<1,
        31u<<26|11<<21|3<<16|10<<11|1<<10|235<<1|1u,
        31u<<26|11<<21|11<<16|10<<11|75<<1|1u,
        7u<<26|11<<21|11<<16|0x7fffu,
        31u<<26|11<<21|11<<16|10<<11|1<<10|491<<1|1u,
        36u<<26|11<<21|0x100u,31u<<26|7<<21|19<<1,36u<<26|7<<21|0x104u,
        spr(8,1,false),36u<<26|8<<21|0x108u,18u<<26|0x03ffffb8u};
    rom.resize(32+sizeof(words)); put(rom,16,sizeof(words));
    for(unsigned n=0;n<sizeof(words)/sizeof(words[0]);++n) put(rom,32+n*4,words[n]);
    require(plugin.load_rom(rom.data(),rom.size()),"multiply loop plugin load"); plugin.run_frame({});
    const auto read=[&](unsigned address) { uint32_t value=0; for(unsigned n=0;n<4;++n) value=value*256+plugin.read_memory(address+n); return value; };
    require(read(0x100)==0 && read(0x104)==0x35a5a5a5 && read(0x108)==0xa000007f,"multiply loop captures");
    std::vector<uint8_t> before,after,replayed; require(plugin.save_state(before),"multiply plugin prefix");
    plugin.run_frame({}); require(plugin.save_state(after),"multiply plugin final"); require(plugin.load_state(before),"multiply plugin restore");
    plugin.run_frame({}); require(plugin.save_state(replayed) && replayed==after,"multiply plugin replay");
    ppc::Machine machine(wii); require(machine.load(rom.data(),rom.size()),"multiply machine load");
    for(unsigned n=0;n<9;++n) machine.step(); // Before MULLWO. overflows, preserving CA.
    require(machine.cpu.xer==0x2000007f,"multiply overflow prefix"); require(machine.save(before),"multiply instruction prefix");
    machine.step(); require(machine.save(after),"multiply instruction final");
    require(machine.cpu.r[11]==0x80000000 && machine.cpu.cr==0x95a5a5a5 && machine.cpu.xer==0xe000007f,"multiply overflow result");
    require(machine.restore(before),"multiply instruction restore"); machine.step(); require(machine.save(replayed) && replayed==after,"multiply instruction replay");
}
