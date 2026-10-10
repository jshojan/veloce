// Independent circular-mask walk and arithmetic word rotation reference.
static uint64_t rotate_checks(ppc::Machine& m) {
    constexpr uint32_t sources[]={0,1,0x7fffffff,0x80000000,0xffffffff,0x55555555,0xaaaaaaaa,0x89abcdef};
    constexpr uint32_t destinations[]={0,0xffffffff,0x12345678,0xa5a5a5a5};
    constexpr uint32_t high[]={0,0x20,0x80000000,0xffffffe0};
    constexpr uint32_t seeds[]={0,0xffffffff,0xa5a5a5a5,0x12345678};
    std::array<uint32_t,1024> masks{};
    for(unsigned begin=0;begin<32;++begin) for(unsigned end=0;end<32;++end) {
        unsigned cursor=begin;
        do { masks[begin*32+end]|=1u<<(31-cursor); if(cursor==end) break; cursor=(cursor+1)%32; } while(true);
    }
    uint64_t count=0;
    const auto check=[&](uint32_t ins,uint32_t a,uint32_t b,uint32_t old,uint32_t cr,uint32_t xer) {
        const unsigned op=ins>>26,rs=(ins>>21)&31,dst=(ins>>16)&31,rb=(ins>>11)&31;
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x13579bdfu+r;
        m.cpu.r[dst]=old; m.cpu.r[rs]=a; if(op==23) m.cpu.r[rb]=b;
        auto registers=m.cpu.r;
        const unsigned shift=op==23?registers[rb]%32:rb;
        const uint64_t factor=1ULL<<shift,modulus=0x100000000ULL;
        const uint32_t rotation=(uint64_t(registers[rs])*factor)%modulus+registers[rs]/(modulus/factor);
        const auto mask=masks[((ins>>6)&31)*32+((ins>>1)&31)];
        const uint32_t result=(rotation&mask)+(op==20?registers[dst]-(registers[dst]&mask):0);
        registers[dst]=result;
        const uint32_t nibble=(result==0?2u:result>=0x80000000u?8u:4u)+xer/0x80000000u;
        const uint32_t wantcr=(ins&1)?cr%0x10000000u+nibble*0x10000000u:cr;
        m.cpu.cr=cr; m.cpu.xer=xer; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        execute(m,ins);
        require(!m.cpu.unsupported && m.cpu.pc==0x80002004 && m.cpu.steps==1,"rotate decode/PC");
        require(m.cpu.r==registers && m.cpu.cr==wantcr && m.cpu.xer==xer && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"rotate registers/flags"); ++count;
    };
    for(unsigned op:{20u,21u,23u}) for(unsigned ai=0;ai<8;++ai) for(unsigned di=0;di<(op==20?4u:1u);++di)
        for(unsigned shift=0;shift<32;++shift) for(unsigned hi=0;hi<(op==23?4u:1u);++hi)
        for(unsigned mask=0;mask<1024;++mask) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc) {
            const uint32_t ins=op<<26|3<<21|11<<16|(op==23?10u:shift)<<11|mask<<1|rc;
            check(ins,sources[ai],high[hi]+shift,destinations[di],seeds[(ai+di+rc+flags)%4],flags<<29|127);
        }
    // Every register-field encoding; immediate SH is also a complete five-bit field.
    for(unsigned op:{20u,21u,23u}) for(unsigned rs=0;rs<32;++rs) for(unsigned dst=0;dst<32;++dst) for(unsigned rb=0;rb<32;++rb)
        for(unsigned mask:{31u,992u,1023u,0u,481u,511u,512u,514u}) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
            check(op<<26|rs<<21|dst<<16|rb<<11|mask<<1|rc,sources[rs%8],0xffffffe0u+rb,destinations[dst%4],seeds[(rs+dst+rb)%4],flags<<29|127);
    // Walking inputs prove rotation direction and every mask's bit selection.
    for(unsigned op:{20u,21u,23u}) for(unsigned bit=0;bit<32;++bit) for(unsigned shift=0;shift<32;++shift)
        for(unsigned mask=0;mask<1024;++mask) for(unsigned rc=0;rc<2;++rc)
            check(op<<26|3<<21|11<<16|(op==23?10u:shift)<<11|mask<<1|rc,1u<<bit,shift,~(1u<<bit),0xa5a5a5a5,0xe000007f);
    for(unsigned op:{20u,21u,23u}) for(unsigned bytes=0;bytes<128;++bytes) for(unsigned flags=0;flags<8;++flags) for(unsigned rc=0;rc<2;++rc)
        check(op<<26|3<<21|11<<16|10<<11|31<<6|1<<1|rc,0x89abcdef,0xffffffff,0x12345678,0xa5a5a5a5,flags<<29|bytes);
    return count;
}
static void rotate_plugin_replay(emu::IEmulatorPlugin& plugin,bool wii) {
    auto rom=image(wii);
    const uint32_t words[]={15u<<26|3<<21|0x89abu,24u<<26|3<<21|3<<16|0xcdefu,
        15u<<26|11<<21|0x1234u,24u<<26|11<<21|11<<16|0x5678u,
        15u<<26|9<<21|0xe000u,24u<<26|9<<21|9<<16|127u,spr(9,1,true),
        15u<<26|5<<21|0xa5a5u,24u<<26|5<<21|5<<16|0xa5a5u,31u<<26|5<<21|255<<12|144<<1,
        20u<<26|3<<21|11<<16|4<<11|28<<6|3<<1|1u,
        21u<<26|11<<21|11<<16|31<<11|0<<6|31<<1|1u,
        15u<<26|10<<21|0xffffu,24u<<26|10<<21|10<<16|65535u,
        23u<<26|11<<21|10<<16|10<<11|28<<6|3<<1|1u,
        36u<<26|11<<21|0x100u,36u<<26|10<<21|0x104u,31u<<26|7<<21|19<<1,
        36u<<26|7<<21|0x108u,spr(8,1,false),36u<<26|8<<21|0x10cu,18u<<26|0x03ffffacu};
    rom.resize(32+sizeof(words)); put(rom,16,sizeof(words));
    for(unsigned n=0;n<sizeof(words)/sizeof(words[0]);++n) put(rom,32+n*4,words[n]);
    require(plugin.load_rom(rom.data(),rom.size()),"rotate loop plugin load"); plugin.run_frame({});
    const auto read=[&](unsigned address) { uint32_t value=0; for(unsigned n=0;n<4;++n) value=value*256+plugin.read_memory(address+n); return value; };
    require(read(0x100)==0x491a2b3c && read(0x104)==0x2000000e && read(0x108)==0x55a5a5a5 && read(0x10c)==0xe000007f,"rotate loop captures");
    std::vector<uint8_t> before,after,replayed; require(plugin.save_state(before),"rotate plugin prefix");
    plugin.run_frame({}); require(plugin.save_state(after),"rotate plugin final"); require(plugin.load_state(before),"rotate plugin restore");
    plugin.run_frame({}); require(plugin.save_state(replayed) && replayed==after,"rotate plugin replay");
    ppc::Machine machine(wii); require(machine.load(rom.data(),rom.size()),"rotate machine load");
    for(unsigned n=0;n<14;++n) machine.step(); // Before RLWNM with RA=RB.
    require(machine.cpu.r[11]==0x491a2b3c && machine.cpu.r[10]==0xffffffff,"rotate alias prefix");
    require(machine.save(before),"rotate instruction prefix"); machine.step(); require(machine.save(after),"rotate instruction final");
    require(machine.cpu.r[10]==0x2000000e && machine.cpu.cr==0x55a5a5a5,"rotate alias result");
    require(machine.restore(before),"rotate instruction restore"); machine.step(); require(machine.save(replayed) && replayed==after,"rotate instruction replay");
}
