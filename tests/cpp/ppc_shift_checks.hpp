// Independent arithmetic quotient/remainder and bit-length references.
static uint64_t shift_checks(ppc::Machine& m) {
    constexpr uint32_t values[]={0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef};
    constexpr uint32_t high[]={0,0x40,0x80000000,0xffffffc0};
    constexpr uint32_t bytehigh[]={0,0x12345600,0x80000000,0xffffff00};
    constexpr uint32_t halfhigh[]={0,0x12340000,0x80000000,0xffff0000};
    constexpr uint32_t seeds[]={0,0xffffffff,0xa5a5a5a5,0x12345678};
    uint64_t count=0;
    const auto check=[&](uint32_t ins,uint32_t a,uint32_t b,uint32_t cr,uint32_t xer) {
        const unsigned xo=(ins>>1)&1023,rs=(ins>>21)&31,dst=(ins>>16)&31,rb=(ins>>11)&31;
        const bool binary=xo==24 || xo==536 || xo==792;
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x13579bdfu+r;
        m.cpu.r[rs]=a; if(binary) m.cpu.r[rb]=b;
        auto registers=m.cpu.r; a=registers[rs];
        uint32_t result=0,wantxer=xer;
        if(xo==954 || xo==922) {
            const uint32_t modulus=xo==954?256u:65536u,v=a%modulus;
            result=uint32_t(int64_t(v)-(v>=modulus/2?modulus:0));
        } else if(xo==26) {
            unsigned length=0; for(uint32_t number=a;number;number/=2) ++length;
            result=32-length;
        } else {
            const unsigned shift=xo==824?rb:registers[rb]%64;
            const uint64_t factor=1ULL<<(shift<32?shift:32);
            if(xo==24) result=shift>=32?0:uint32_t(uint64_t(a)*factor);
            else if(xo==536) result=a/factor;
            else {
                const int64_t math=signed_value(a),quotient=math/int64_t(factor),remainder=math%int64_t(factor);
                // Counts >=32 saturate; for smaller counts floor division rounds negatives down.
                result=shift>=32?(math<0?0xffffffffu:0):uint32_t(quotient-(remainder<0?1:0));
                const bool carry=math<0 && (shift>=32 || remainder!=0);
                wantxer=(xer&0xdfffffffu)+(carry?0x20000000u:0);
            }
        }
        registers[dst]=result;
        const uint32_t nibble=(result==0?2u:result>=0x80000000u?8u:4u)+xer/0x80000000u;
        const uint32_t wantcr=(ins&1)?cr%0x10000000u+nibble*0x10000000u:cr;
        m.cpu.cr=cr; m.cpu.xer=xer; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        execute(m,ins);
        require(!m.cpu.unsupported && m.cpu.pc==0x80002004 && m.cpu.steps==1,"shift/unary decode/PC");
        require(m.cpu.r==registers && m.cpu.cr==wantcr && m.cpu.xer==wantxer && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"shift/unary registers/flags"); ++count;
    };
    for(unsigned xo:{24u,536u,792u}) for(unsigned ai=0;ai<16;++ai) for(unsigned hi=0;hi<4;++hi) for(unsigned shift=0;shift<64;++shift)
        for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|3<<21|11<<16|10<<11|xo<<1|rc,values[ai],high[hi]+shift,seeds[(ai+hi+flags+rc)%4],flags<<29|127);
    for(unsigned ai=0;ai<16;++ai) for(unsigned shift=0;shift<32;++shift) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
        check(31u<<26|3<<21|11<<16|shift<<11|824<<1|rc,values[ai],0x2468ace0,seeds[(ai+flags+rc)%4],flags<<29|127);
    for(unsigned xo:{954u,922u}) for(unsigned hi=0;hi<4;++hi) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
        for(unsigned low=0;low<(xo==954?256u:65536u);++low)
            check(31u<<26|3<<21|11<<16|xo<<1|rc,(xo==954?bytehigh[hi]:halfhigh[hi])+low,0x2468ace0,seeds[(hi+flags+rc)%4],flags<<29|127);
    for(unsigned pattern=0;pattern<65;++pattern) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc) {
        const uint32_t a=pattern==0?0:pattern<=32?1u<<(pattern-1):uint32_t((1ULL<<(pattern-32))-1);
        check(31u<<26|3<<21|11<<16|26<<1|rc,a,0x2468ace0,seeds[(pattern+flags+rc)%4],flags<<29|127);
    }
    constexpr uint32_t counts[]={0,1,31,32,33,63,64,0xffffffff};
    for(unsigned xo:{24u,536u,792u}) for(unsigned rs=0;rs<32;++rs) for(unsigned dst=0;dst<32;++dst) for(unsigned rb=0;rb<32;++rb)
        for(auto b:counts) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|rs<<21|dst<<16|rb<<11|xo<<1|rc,values[rs%16],b,seeds[(rs+dst+rb)%4],flags<<29|127);
    for(unsigned xo:{954u,922u,26u}) for(unsigned rs=0;rs<32;++rs) for(unsigned dst=0;dst<32;++dst) for(auto a:values)
        for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|rs<<21|dst<<16|xo<<1|rc,a,0x2468ace0,seeds[(rs+dst)%4],flags<<29|127);
    for(unsigned rs=0;rs<32;++rs) for(unsigned dst=0;dst<32;++dst) for(unsigned shift=0;shift<32;++shift)
        for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|rs<<21|dst<<16|shift<<11|824<<1|rc,values[rs%16],0x2468ace0,seeds[(rs+dst)%4],flags<<29|127);
    for(unsigned xo:{24u,536u,792u}) for(unsigned bit=0;bit<32;++bit) for(bool complement:{false,true}) for(unsigned shift=0;shift<64;++shift)
        for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|3<<21|11<<16|10<<11|xo<<1|rc,complement?~(1u<<bit):1u<<bit,shift,0xa5a5a5a5,flags<<29|127);
    for(unsigned xo:{24u,536u,792u,824u,954u,922u,26u}) for(unsigned bytes=0;bytes<128;++bytes) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
        check(31u<<26|3<<21|11<<16|((xo==954 || xo==922 || xo==26)?0u:10u)<<11|xo<<1|rc,0xfffffffe,0xffffffff,0xa5a5a5a5,flags<<29|bytes);
    // Unary RB is reserved: each nonzero encoding must reject before changing state.
    for(unsigned xo:{954u,922u,26u}) for(unsigned rs=0;rs<32;++rs) for(unsigned dst=0;dst<32;++dst) for(unsigned rb=1;rb<32;++rb) for(unsigned rc=0;rc<2;++rc) {
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x89abcdefu+r;
        auto registers=m.cpu.r; m.cpu.cr=0xa5a5a5a5; m.cpu.xer=0xe000007f; m.cpu.lr=123; m.cpu.ctr=456;
        execute(m,31u<<26|rs<<21|dst<<16|rb<<11|xo<<1|rc);
        require(m.cpu.unsupported && m.cpu.pc==0x80002000 && m.cpu.r==registers && m.cpu.cr==0xa5a5a5a5 && m.cpu.xer==0xe000007f && m.cpu.lr==123 && m.cpu.ctr==456,"reserved unary RB mutation");
    }
    return count;
}
static void shift_plugin_replay(emu::IEmulatorPlugin& plugin,bool wii) {
    auto rom=image(wii);
    const uint32_t words[]={15u<<26|3<<21|0xffffu,24u<<26|3<<21|3<<16|0xfffdu,
        15u<<26|9<<21|0xc000u,24u<<26|9<<21|9<<16|127u,spr(9,1,true),
        15u<<26|5<<21|0xa5a5u,24u<<26|5<<21|5<<16|0xa5a5u,31u<<26|5<<21|255<<12|144<<1,
        31u<<26|3<<21|11<<16|1<<11|824<<1|1u,
        31u<<26|11<<21|11<<16|954<<1|1u,31u<<26|11<<21|10<<16|26<<1|1u,
        36u<<26|11<<21|0x100u,36u<<26|10<<21|0x104u,31u<<26|7<<21|19<<1,
        36u<<26|7<<21|0x108u,spr(8,1,false),36u<<26|8<<21|0x10cu,18u<<26|0x03ffffbcu};
    rom.resize(32+sizeof(words)); put(rom,16,sizeof(words));
    for(unsigned n=0;n<sizeof(words)/sizeof(words[0]);++n) put(rom,32+n*4,words[n]);
    require(plugin.load_rom(rom.data(),rom.size()),"shift loop plugin load"); plugin.run_frame({});
    const auto read=[&](unsigned address) { uint32_t value=0; for(unsigned n=0;n<4;++n) value=value*256+plugin.read_memory(address+n); return value; };
    require(read(0x100)==0xfffffffe && read(0x104)==0 && read(0x108)==0x35a5a5a5 && read(0x10c)==0xe000007f,"shift loop captures");
    std::vector<uint8_t> before,after,replayed; require(plugin.save_state(before),"shift plugin prefix");
    plugin.run_frame({}); require(plugin.save_state(after),"shift plugin final"); require(plugin.load_state(before),"shift plugin restore");
    plugin.run_frame({}); require(plugin.save_state(replayed) && replayed==after,"shift plugin replay");
    ppc::Machine machine(wii); require(machine.load(rom.data(),rom.size()),"shift machine load");
    for(unsigned n=0;n<8;++n) machine.step(); // Before negative SRAWI with discarded 1.
    require(machine.cpu.xer==0xc000007f,"shift carry prefix"); require(machine.save(before),"shift instruction prefix"); machine.step(); require(machine.save(after),"shift instruction final");
    require(machine.cpu.r[11]==0xfffffffe && machine.cpu.cr==0x95a5a5a5 && machine.cpu.xer==0xe000007f,"shift carry result");
    require(machine.restore(before),"shift instruction restore"); machine.step(); require(machine.save(replayed) && replayed==after,"shift instruction replay");
}
