// Independent scalar/reference checks for the supplied 32-bit condition-register profile.
static uint64_t condition_checks(ppc::Machine& m) {
    constexpr uint32_t values[]={0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef};
    constexpr uint32_t seeds[]={0,0xffffffff,0xa5a5a5a5,0x12345678};
    uint64_t count=0;
    const auto check=[&](uint32_t ins,uint32_t a,uint32_t b,uint32_t cr,uint32_t xer,uint32_t wantcr,uint32_t wantxer,unsigned ra=3,unsigned rb=10) {
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x13579bdf+r;
        m.cpu.r[ra]=a; m.cpu.r[rb]=b; const auto registers=m.cpu.r;
        m.cpu.cr=cr; m.cpu.xer=xer; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        execute(m,ins);
        require(!m.cpu.unsupported && m.cpu.pc==0x80002004 && m.cpu.steps==1,"condition decode/PC");
        require(m.cpu.r==registers && m.cpu.cr==wantcr && m.cpu.xer==wantxer && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"condition registers"); ++count;
    };
    const auto compare=[&](bool logical,uint32_t a,uint32_t b,unsigned field,uint32_t cr,uint32_t xer) {
        const int64_t left=logical?int64_t(a):signed_value(a),right=logical?int64_t(b):signed_value(b);
        const uint32_t nibble=(left<right?8u:left>right?4u:2u)|(xer>>31);
        const auto shift=28-field*4; return (cr&~(15u<<shift))|(nibble<<shift);
    };
    for(bool logical:{false,true}) for(auto a:values) for(auto b:values) for(unsigned field=0;field<8;++field)
        for(unsigned flags=0;flags<8;++flags) for(auto cr:seeds) {
            const uint32_t xer=flags<<29|127,ins=31u<<26|field<<23|3<<16|10<<11|(logical?32u:0u)<<1;
            check(ins,a,b,cr,xer,compare(logical,a,b,field,cr,xer),xer);
        }
    for(bool logical:{false,true}) for(unsigned ra=0;ra<32;++ra) for(unsigned rb=0;rb<32;++rb)
        for(unsigned field=0;field<8;++field) for(auto cr:seeds) {
            const uint32_t a=ra==rb?0xffffffff:0x80000000,b=0xffffffff;
            check(31u<<26|field<<23|ra<<16|rb<<11|(logical?32u:0u)<<1,a,b,cr,0xe000007f,compare(logical,a,b,field,cr,0xe000007f),0xe000007f,ra,rb);
        }
    for(bool logical:{false,true}) for(auto a:values) for(unsigned field=0;field<8;++field) for(unsigned so=0;so<2;++so) {
        const uint32_t xer=so<<31|0x6000007f,cr=seeds[(field+so)%4];
        for(unsigned immediate=0;immediate<65536;++immediate) {
            const uint32_t b=logical?immediate:uint32_t(int64_t(immediate)-(immediate>=32768?65536:0));
            check((logical?10u:11u)<<26|field<<23|3<<16|immediate,a,0x2468ace0,cr,xer,compare(logical,a,b,field,cr,xer),xer);
        }
    }
    // Unlike address-generating instructions, immediate comparisons read GPR0.
    for(bool logical:{false,true}) for(unsigned ra=0;ra<32;++ra) for(unsigned field=0;field<8;++field)
        for(unsigned flags=0;flags<8;++flags) for(auto cr:seeds) {
            const uint32_t xer=flags<<29|127,b=logical?65535u:0xffffffffu;
            check((logical?10u:11u)<<26|field<<23|ra<<16|65535u,0xffffffff,0,cr,xer,compare(logical,0xffffffff,b,field,cr,xer),xer,ra,(ra+1)%32);
        }
    constexpr unsigned xos[]={257,129,289,225,33,449,417,193};
    // Truth-table lookup differs from the CPU's boolean operator implementation.
    constexpr unsigned truth[]={8,4,9,7,1,14,13,6};
    for(unsigned op=0;op<8;++op) for(unsigned dst=0;dst<32;++dst) for(unsigned srca=0;srca<32;++srca) for(unsigned srcb=0;srcb<32;++srcb)
        for(auto cr:seeds) for(unsigned flags=0;flags<8;++flags) {
            const unsigned input=2*((cr>>(31-srca))&1)+((cr>>(31-srcb))&1);
            const uint32_t bit=(truth[op]>>input)&1,mask=1u<<(31-dst),xer=flags<<29|127;
            check(19u<<26|dst<<21|srca<<16|srcb<<11|xos[op]<<1,0x13579bdf,0x2468ace0,cr,xer,(cr&~mask)|(bit<<(31-dst)),xer);
        }
    // Fixed CR seeds cannot produce all truth inputs for every source pair.
    for(unsigned op=0;op<8;++op) for(unsigned dst=0;dst<32;++dst) for(unsigned srca=0;srca<32;++srca) for(unsigned srcb=0;srcb<32;++srcb)
        for(unsigned input=0;input<4;++input) for(unsigned flags=0;flags<8;++flags) {
            if(srca==srcb && (input==1 || input==2)) continue;
            const uint32_t amask=1u<<(31-srca),bmask=1u<<(31-srcb),dmask=1u<<(31-dst);
            const uint32_t cr=(0xa5a5a5a5u&~(amask|bmask))|(input&2?amask:0)|(input&1?bmask:0);
            const uint32_t bit=(truth[op]>>input)&1,xer=flags<<29|127;
            check(19u<<26|dst<<21|srca<<16|srcb<<11|xos[op]<<1,0,0,cr,xer,(cr&~dmask)|(bit<<(31-dst)),xer);
        }
    for(unsigned dst=0;dst<8;++dst) for(unsigned src=0;src<8;++src) for(unsigned sn=0;sn<16;++sn) for(unsigned dn=0;dn<16;++dn) for(auto seed:seeds) for(unsigned flags=0;flags<8;++flags) {
        const auto ds=28-dst*4,ss=28-src*4; uint32_t cr=(seed&~(15u<<ds))|dn<<ds; cr=(cr&~(15u<<ss))|sn<<ss;
        const uint32_t wanted=(cr&~(15u<<ds))|((cr>>ss)&15)<<ds,xer=flags<<29|127;
        check(19u<<26|dst<<23|src<<18,0,0,cr,xer,wanted,xer);
    }
    for(unsigned field=0;field<8;++field) for(unsigned flags=0;flags<8;++flags) for(unsigned bytes=0;bytes<128;++bytes) for(auto cr:seeds) {
        const uint32_t xer=flags<<29|bytes; const auto shift=28-field*4;
        check(31u<<26|field<<23|512u<<1,0,0,cr,xer,(cr&~(15u<<shift))|(flags*2u)<<shift,bytes);
    }
    const auto reject=[&](uint32_t instruction) {
        m.cpu={}; m.cpu.cr=0xa5a5a5a5; m.cpu.xer=0xe000007f; m.cpu.r[0]=0x12345678; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        const auto registers=m.cpu.r; execute(m,instruction);
        require(m.cpu.unsupported && m.cpu.pc==0x80002000 && m.cpu.cr==0xa5a5a5a5 && m.cpu.xer==0xe000007f && m.cpu.r==registers && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"invalid condition form mutation");
    };
    for(unsigned op:{10u,11u,31u}) for(unsigned field=0;field<8;++field) for(unsigned reserved=1;reserved<4;++reserved)
        reject(op<<26|field<<23|reserved<<21|3<<16|10<<11);
    for(unsigned xo:{0u,32u}) reject(31u<<26|3<<16|10<<11|xo<<1|1);
    for(auto xo:xos) for(unsigned d=0;d<32;++d) for(unsigned a=0;a<32;++a) for(unsigned b=0;b<32;++b) reject(19u<<26|d<<21|a<<16|b<<11|xo<<1|1);
    for(unsigned field=0;field<8;++field) for(unsigned bit=0;bit<32;++bit) if((0x007ff801u>>bit)&1) reject(31u<<26|field<<23|512u<<1|1u<<bit);
    for(unsigned d=0;d<8;++d) for(unsigned src=0;src<8;++src) for(unsigned bit=0;bit<32;++bit) if((0x0063f801u>>bit)&1) reject(19u<<26|d<<23|src<<18|1u<<bit);
    return count;
}

static std::vector<uint8_t> condition_loop_image(bool wii) {
    auto bytes=image(wii);
    const uint32_t words[]={15u<<26|3<<21|65535u,24u<<26|3<<21|3<<16|65535u,
        15u<<26|9<<21|0xe000u,24u<<26|9<<21|9<<16|127u,spr(9,1,true),
        11u<<26|3<<16,11u<<26|1u<<23|3<<16|65535u,
        19u<<26|31u<<21|4u<<16|6u<<11|257u<<1,
        19u<<26|30u<<21|4u<<16|6u<<11|449u<<1,
        19u<<26|2u<<23|1u<<18,31u<<26|3u<<23|512u<<1,
        31u<<26|4u<<21|19u<<1,36u<<26|4u<<21|0x100u,
        spr(5,1,false),36u<<26|5u<<21|0x104u,18u<<26|0x03ffffc4u};
    bytes.resize(32+sizeof(words)); put(bytes,16,sizeof(words));
    for(unsigned n=0;n<sizeof(words)/sizeof(words[0]);++n) put(bytes,32+n*4,words[n]); return bytes;
}
static void condition_plugin_replay(emu::IEmulatorPlugin& plugin,bool wii) {
    const auto rom=condition_loop_image(wii); require(plugin.load_rom(rom.data(),rom.size()),"condition loop plugin load");
    plugin.run_frame({}); std::vector<uint8_t> prefix,final,replayed; require(plugin.save_state(prefix),"condition prefix save");
    require(plugin.read_memory(0x100)==0x93 && plugin.read_memory(0x101)==0x3e && plugin.read_memory(0x102)==0 && plugin.read_memory(0x103)==2 && plugin.read_memory(0x104)==0 && plugin.read_memory(0x105)==0 && plugin.read_memory(0x106)==0 && plugin.read_memory(0x107)==127,"condition loop guest captures");
    plugin.run_frame({}); require(plugin.save_state(final),"condition final save");
    require(plugin.load_state(prefix),"condition prefix restore"); plugin.run_frame({}); require(plugin.save_state(replayed) && replayed==final,"condition plugin replay");
    ppc::Machine machine(wii); require(machine.load(rom.data(),rom.size()),"condition machine load");
    for(unsigned n=0;n<10;++n) machine.step(); // Immediately before MCRXR.
    require(machine.cpu.xer==0xe000007f,"MCRXR prefix flags");
    require(machine.save(prefix),"MCRXR prefix save"); machine.step(); require(machine.save(final),"MCRXR cleared save");
    require(machine.cpu.xer==127 && machine.cpu.cr==0x933e0002,"MCRXR cleared flags/field");
    require(machine.restore(prefix),"MCRXR prefix restore"); machine.step(); require(machine.save(replayed) && replayed==final,"MCRXR instruction replay");
}
