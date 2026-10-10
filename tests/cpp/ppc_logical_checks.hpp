// Integer logical references use per-bit truth tables, independent of CPU operators.
static uint64_t logical_checks(ppc::Machine& m) {
    constexpr unsigned xos[]={28,60,444,412,316,476,124,284};
    constexpr unsigned truth[]={8,4,14,13,6,7,1,9};
    constexpr uint32_t values[]={0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef};
    constexpr uint32_t immed_values[]={0,1,0x7fffffff,0x80000000,0xffffffff,0x55555555,0xaaaaaaaa,0x89abcdef};
    constexpr uint32_t seeds[]={0,0xffffffff,0xa5a5a5a5,0x12345678};
    uint64_t count=0;
    const auto reference=[](unsigned table,uint32_t a,uint32_t b) {
        uint32_t result=0;
        for(unsigned bit=0;bit<32;++bit) {
            const unsigned input=((a>>bit)&1)*2+((b>>bit)&1);
            result|=((table>>input)&1)<<bit;
        }
        return result;
    };
    const auto check=[&](uint32_t ins,unsigned table,bool record,uint32_t a,uint32_t b,uint32_t cr,uint32_t xer,bool immediate=false) {
        const unsigned rs=(ins>>21)&31,dst=(ins>>16)&31,rb=(ins>>11)&31;
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x13579bdfu+r;
        m.cpu.r[rs]=a; if(!immediate) m.cpu.r[rb]=b;
        auto registers=m.cpu.r;
        const uint32_t result=reference(table,registers[rs],immediate?b:registers[rb]); registers[dst]=result;
        const uint32_t nibble=(result==0?2u:result>=0x80000000u?8u:4u)+(xer/0x80000000u);
        const uint32_t wantcr=record?(cr%0x10000000u)+nibble*0x10000000u:cr;
        m.cpu.cr=cr; m.cpu.xer=xer; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        execute(m,ins);
        require(!m.cpu.unsupported && m.cpu.pc==0x80002004 && m.cpu.steps==1,"integer logical decode/PC");
        require(m.cpu.r==registers && m.cpu.cr==wantcr && m.cpu.xer==xer && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"integer logical registers/flags"); ++count;
    };
    for(unsigned op=0;op<8;++op) for(unsigned ai=0;ai<16;++ai) for(unsigned bi=0;bi<16;++bi)
        for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|3<<21|11<<16|10<<11|xos[op]<<1|rc,truth[op],rc,values[ai],values[bi],seeds[(ai+bi)%4],flags<<29|127);
    // Every RS/RA/RB encoding, including all aliases and ordinary GPR0.
    for(unsigned op=0;op<8;++op) for(unsigned rs=0;rs<32;++rs) for(unsigned dst=0;dst<32;++dst) for(unsigned rb=0;rb<32;++rb)
        for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|rs<<21|dst<<16|rb<<11|xos[op]<<1|rc,truth[op],rc,values[rs%16],values[(rb+7)%16],seeds[(rs+dst+rb)%4],flags<<29|127);
    // Exercise all four bit inputs at every bit position, including sign bit.
    for(unsigned op=0;op<8;++op) for(unsigned bit=0;bit<32;++bit) for(unsigned input=0;input<4;++input)
        for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|3<<21|11<<16|10<<11|xos[op]<<1|rc,truth[op],rc,input&2?1u<<bit:0,input&1?1u<<bit:0,seeds[bit%4],flags<<29|127);
    for(unsigned op=24;op<=29;++op) for(auto a:immed_values) for(unsigned so=0;so<2;++so) for(unsigned imm=0;imm<65536;++imm)
        check(op<<26|3<<21|11<<16|imm,op<26?14:op<28?6:8,op>=28,a,imm*((op&1)?65536u:1u),seeds[so+2],so<<31|0x6000007f,true);
    constexpr unsigned immediates[]={0,1,0x7fff,0x8000,0xffff,0x5555,0xaaaa,0x89ab};
    for(unsigned op=24;op<=29;++op) for(unsigned rs=0;rs<32;++rs) for(unsigned dst=0;dst<32;++dst)
        for(auto imm:immediates) for(unsigned flags=0;flags<8;++flags)
            check(op<<26|rs<<21|dst<<16|imm,op<26?14:op<28?6:8,op>=28,values[rs%16],imm*((op&1)?65536u:1u),seeds[(rs+dst)%4],flags<<29|127,true);
    // These X-form operations have no OE bit: the high XO bit is not an OE alias.
    for(auto xo:xos) for(unsigned rc=0;rc<2;++rc) {
        m.cpu={}; m.cpu.r[3]=0x80000000; m.cpu.r[10]=0xffffffff; m.cpu.cr=0xa5a5a5a5; m.cpu.xer=0xe000007f;
        const auto registers=m.cpu.r;
        execute(m,31u<<26|3<<21|11<<16|10<<11|xo<<1|0x400u|rc);
        require(m.cpu.unsupported && m.cpu.pc==0x80002000 && m.cpu.r==registers && m.cpu.cr==0xa5a5a5a5 && m.cpu.xer==0xe000007f,"logical invalid XO mutation");
    }
    return count;
}
static void logical_plugin_replay(emu::IEmulatorPlugin& plugin,bool wii) {
    auto rom=image(wii);
    const uint32_t words[]={15u<<26|3<<21|0x8000u,24u<<26|3<<21|3<<16|1u,
        15u<<26|10<<21|0xffffu,15u<<26|9<<21|0xe000u,24u<<26|9<<21|9<<16|127u,spr(9,1,true),
        15u<<26|5<<21|0xa5a5u,24u<<26|5<<21|5<<16|0xa5a5u,31u<<26|5<<21|255<<12|144<<1,
        31u<<26|3<<21|11<<16|10<<11|28<<1|1u,26u<<26|11<<21|11<<16|65535u,
        29u<<26|11<<21|11<<16|0x7fffu,31u<<26|11<<21|11<<16|3<<11|284<<1|1u,
        36u<<26|11<<21|0x100u,31u<<26|7<<21|19<<1,36u<<26|7<<21|0x104u,
        spr(8,1,false),36u<<26|8<<21|0x108u,18u<<26|0x03ffffb8u};
    rom.resize(32+sizeof(words)); put(rom,16,sizeof(words));
    for(unsigned n=0;n<sizeof(words)/sizeof(words[0]);++n) put(rom,32+n*4,words[n]);
    require(plugin.load_rom(rom.data(),rom.size()),"logical loop plugin load"); plugin.run_frame({});
    const auto read=[&](unsigned address) { uint32_t value=0; for(unsigned n=0;n<4;++n) value=value*256+plugin.read_memory(address+n); return value; };
    require(read(0x100)==0x7ffffffe && read(0x104)==0x55a5a5a5 && read(0x108)==0xe000007f,"logical loop captures");
    std::vector<uint8_t> before,after,replayed; require(plugin.save_state(before),"logical plugin prefix");
    plugin.run_frame({}); require(plugin.save_state(after),"logical plugin final"); require(plugin.load_state(before),"logical plugin restore");
    plugin.run_frame({}); require(plugin.save_state(replayed) && replayed==after,"logical plugin replay");
    ppc::Machine machine(wii); require(machine.load(rom.data(),rom.size()),"logical machine load");
    for(unsigned n=0;n<12;++n) machine.step(); // Before aliased EQV.
    require(machine.cpu.r[11]==0 && machine.cpu.cr==0x35a5a5a5,"logical alias prefix");
    require(machine.save(before),"logical instruction prefix"); machine.step(); require(machine.save(after),"logical instruction final");
    require(machine.cpu.r[11]==0x7ffffffe && machine.cpu.cr==0x55a5a5a5,"logical alias result");
    require(machine.restore(before),"logical instruction restore"); machine.step(); require(machine.save(replayed) && replayed==after,"logical instruction replay");
}
