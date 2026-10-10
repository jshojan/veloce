// Independent signed mathematical results and unsigned no-borrow carry rules.
static uint64_t carry_checks(ppc::Machine& m) {
    constexpr uint32_t values[]={0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef};
    constexpr unsigned xos[]={104,202,234,200,232};
    constexpr uint32_t seeds[]={0,0xffffffff,0xa5a5a5a5,0x12345678};
    uint64_t count=0;
    const auto check=[&](uint32_t ins,uint32_t a,uint32_t cr,uint32_t xer) {
        const unsigned op=ins>>26,xo=(ins>>1)&511,dst=(ins>>21)&31,ra=(ins>>16)&31;
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x13579bdfu+r;
        m.cpu.r[ra]=a; auto registers=m.cpu.r;
        const unsigned ca=xer/0x20000000u%2; bool carry=false,updates=true;
        int64_t math=0;
        if(op==31) {
            if(xo==104) { math=-signed_value(a); updates=false; }
            else if(xo==202) { math=signed_value(a)+ca; carry=a==0xffffffffu && ca; }
            else if(xo==234) { math=signed_value(a)+ca-1; carry=ca || a!=0; }
            else if(xo==200) { math=-signed_value(a)+ca-1; carry=a==0 && ca; }
            else { math=-signed_value(a)+ca-2; carry=ca || a!=0xffffffffu; }
        } else {
            const int64_t imm=int64_t(ins%65536)-(ins%65536>=32768?65536:0);
            const uint32_t bits=uint32_t(imm);
            math=op==8?imm-signed_value(a):signed_value(a)+imm;
            carry=op==8?bits>=a:uint64_t(a)+bits>=0x100000000ULL;
        }
        const uint32_t result=uint32_t(math); registers[dst]=result;
        uint32_t wantxer=updates?(xer&0xdfffffffu)+(carry?0x20000000u:0):xer;
        if(op==31 && (ins&0x400)) {
            const bool ov=math < -2147483648LL || math > 2147483647LL;
            wantxer=(wantxer&0xbfffffffu)+(ov?0x40000000u:0);
            if(ov) wantxer|=0x80000000u;
        }
        const bool record=op==13 || (op==31 && (ins&1));
        const uint32_t nibble=(result==0?2u:result>=0x80000000u?8u:4u)+wantxer/0x80000000u;
        const uint32_t wantcr=record?cr%0x10000000u+nibble*0x10000000u:cr;
        m.cpu.cr=cr; m.cpu.xer=xer; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        execute(m,ins);
        require(!m.cpu.unsupported && m.cpu.pc==0x80002004 && m.cpu.steps==1,"carry arithmetic decode/PC");
        require(m.cpu.r==registers && m.cpu.cr==wantcr && m.cpu.xer==wantxer && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"carry arithmetic registers/flags"); ++count;
    };
    for(auto xo:xos) for(unsigned ai=0;ai<16;++ai) for(unsigned flags=0;flags<8;++flags) for(unsigned oe=0;oe<2;++oe) for(unsigned rc=0;rc<2;++rc)
        check(31u<<26|11<<21|3<<16|oe<<10|xo<<1|rc,values[ai],seeds[(ai+flags+oe+rc)%4],flags<<29|127);
    for(unsigned op:{12u,13u,8u}) for(unsigned ai=0;ai<16;++ai) for(unsigned flags=0;flags<8;++flags) for(unsigned imm=0;imm<65536;++imm)
        check(op<<26|11<<21|3<<16|imm,values[ai],seeds[(ai+flags)%4],flags<<29|127);
    for(auto xo:xos) for(unsigned dst=0;dst<32;++dst) for(unsigned ra=0;ra<32;++ra) for(auto a:values)
        for(unsigned flags=0;flags<8;++flags) for(unsigned oe=0;oe<2;++oe) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|dst<<21|ra<<16|oe<<10|xo<<1|rc,a,seeds[(dst+ra)%4],flags<<29|127);
    constexpr unsigned immediates[]={0,1,0x7fff,0x8000,0xffff,0x5555,0xaaaa,0x89ab};
    for(unsigned op:{12u,13u,8u}) for(unsigned dst=0;dst<32;++dst) for(unsigned ra=0;ra<32;++ra) for(auto imm:immediates)
        for(unsigned flags=0;flags<8;++flags)
            check(op<<26|dst<<21|ra<<16|imm,values[ra%16],seeds[(dst+ra)%4],flags<<29|127);
    for(unsigned bit=0;bit<32;++bit) for(bool complement:{false,true}) for(auto xo:xos)
        for(unsigned flags=0;flags<8;++flags) for(unsigned oe=0;oe<2;++oe) for(unsigned rc=0;rc<2;++rc)
            check(31u<<26|11<<21|3<<16|oe<<10|xo<<1|rc,complement?~(1u<<bit):1u<<bit,0xa5a5a5a5,flags<<29|127);
    for(auto xo:xos) for(unsigned bytes=0;bytes<128;++bytes) for(unsigned flags=0;flags<8;++flags) for(unsigned oe=0;oe<2;++oe) for(unsigned rc=0;rc<2;++rc)
        check(31u<<26|11<<21|3<<16|oe<<10|xo<<1|rc,0x80000000,0xa5a5a5a5,flags<<29|bytes);
    for(unsigned op:{12u,13u,8u}) for(unsigned bytes=0;bytes<128;++bytes) for(unsigned flags=0;flags<8;++flags) for(auto imm:immediates)
        check(op<<26|11<<21|3<<16|imm,0x80000000,0xa5a5a5a5,flags<<29|bytes);
    for(auto xo:xos) for(unsigned dst=0;dst<32;++dst) for(unsigned ra=0;ra<32;++ra) for(unsigned rb=1;rb<32;++rb) for(unsigned oe=0;oe<2;++oe) for(unsigned rc=0;rc<2;++rc) {
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x89abcdefu+r;
        auto registers=m.cpu.r; m.cpu.cr=0xa5a5a5a5; m.cpu.xer=0xe000007f; m.cpu.lr=123; m.cpu.ctr=456;
        execute(m,31u<<26|dst<<21|ra<<16|rb<<11|oe<<10|xo<<1|rc);
        require(m.cpu.unsupported && m.cpu.pc==0x80002000 && m.cpu.r==registers && m.cpu.cr==0xa5a5a5a5 && m.cpu.xer==0xe000007f && m.cpu.lr==123 && m.cpu.ctr==456,"reserved carry unary RB mutation");
    }
    return count;
}
static void carry_plugin_replay(emu::IEmulatorPlugin& plugin,bool wii) {
    auto rom=image(wii);
    const uint32_t words[]={15u<<26|3<<21|0x8000u,15u<<26|9<<21|0x2000u,
        24u<<26|9<<21|9<<16|127u,spr(9,1,true),15u<<26|5<<21|0xa5a5u,
        24u<<26|5<<21|5<<16|0xa5a5u,31u<<26|5<<21|255<<12|144<<1,
        31u<<26|11<<21|3<<16|1<<10|104<<1|1u,
        31u<<26|11<<21|11<<16|1<<10|234<<1|1u,
        13u<<26|11<<21|11<<16|1u,8u<<26|10<<21|11<<16,
        36u<<26|11<<21|0x100u,36u<<26|10<<21|0x104u,31u<<26|7<<21|19<<1,
        36u<<26|7<<21|0x108u,spr(8,1,false),36u<<26|8<<21|0x10cu,18u<<26|0x03ffffbcu};
    rom.resize(32+sizeof(words)); put(rom,16,sizeof(words));
    for(unsigned n=0;n<sizeof(words)/sizeof(words[0]);++n) put(rom,32+n*4,words[n]);
    require(plugin.load_rom(rom.data(),rom.size()),"carry loop plugin load"); plugin.run_frame({});
    const auto read=[&](unsigned address) { uint32_t value=0; for(unsigned n=0;n<4;++n) value=value*256+plugin.read_memory(address+n); return value; };
    require(read(0x100)==0x80000001 && read(0x104)==0x7fffffff && read(0x108)==0x95a5a5a5 && read(0x10c)==0x8000007f,"carry loop captures");
    std::vector<uint8_t> before,after,replayed; require(plugin.save_state(before),"carry plugin prefix");
    plugin.run_frame({}); require(plugin.save_state(after),"carry plugin final"); require(plugin.load_state(before),"carry plugin restore");
    plugin.run_frame({}); require(plugin.save_state(replayed) && replayed==after,"carry plugin replay");
    ppc::Machine machine(wii); require(machine.load(rom.data(),rom.size()),"carry machine load");
    for(unsigned n=0;n<7;++n) machine.step(); // Before NEGO. sets OV/SO and preserves CA.
    require(machine.cpu.xer==0x2000007f,"carry overflow prefix"); require(machine.save(before),"carry instruction prefix");
    machine.step(); require(machine.save(after),"carry instruction final");
    require(machine.cpu.r[11]==0x80000000 && machine.cpu.cr==0x95a5a5a5 && machine.cpu.xer==0xe000007f,"carry overflow result");
    require(machine.restore(before),"carry instruction restore"); machine.step(); require(machine.save(replayed) && replayed==after,"carry instruction replay");
}
