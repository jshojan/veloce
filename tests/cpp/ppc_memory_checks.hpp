// Integer memory transfers: independent byte-array oracle and full register checks.
struct MemoryForm { unsigned code,width; bool indexed,load,update,sign,reverse; };
static constexpr MemoryForm memory_forms[]={
    {32,4,0,1,0,0,0},{33,4,0,1,1,0,0},{34,1,0,1,0,0,0},{35,1,0,1,1,0,0},
    {36,4,0,0,0,0,0},{37,4,0,0,1,0,0},{38,1,0,0,0,0,0},{39,1,0,0,1,0,0},
    {40,2,0,1,0,0,0},{41,2,0,1,1,0,0},{42,2,0,1,0,1,0},{43,2,0,1,1,1,0},
    {44,2,0,0,0,0,0},{45,2,0,0,1,0,0},
    {23,4,1,1,0,0,0},{55,4,1,1,1,0,0},{87,1,1,1,0,0,0},{119,1,1,1,1,0,0},
    {151,4,1,0,0,0,0},{183,4,1,0,1,0,0},{215,1,1,0,0,0,0},{247,1,1,0,1,0,0},
    {279,2,1,1,0,0,0},{311,2,1,1,1,0,0},{343,2,1,1,0,1,0},{375,2,1,1,1,1,0},
    {407,2,1,0,0,0,0},{439,2,1,0,1,0,0},
    {534,4,1,1,0,0,1},{790,2,1,1,0,0,1},{662,4,1,0,0,0,1},{918,2,1,0,0,0,1}
};
struct MemoryChecks { uint64_t rows=0,diagnostics=0; };
static MemoryChecks memory_transfer_checks(ppc::Machine& m) {
    constexpr uint32_t values[]={0,1,0x7fff8000,0x8000ffff,0xffffffff,0x89abcdef,0x5555aaaa,0x12345678};
    constexpr uint32_t aliases[]={0,0x80000000,0xc0000000};
    MemoryChecks counts;
    const auto check=[&](const MemoryForm& f,unsigned rt,unsigned ra,unsigned rb,unsigned imm,uint32_t a,uint32_t b,uint32_t data,uint32_t cr,uint32_t xer,bool reserved=false) {
        m.cpu={}; for(unsigned r=0;r<32;++r) m.cpu.r[r]=0x80010000u+r*4;
        m.cpu.r[rt]=data; m.cpu.r[ra]=a; if(f.indexed) m.cpu.r[rb]=b;
        auto expected=m.cpu.r;
        const uint32_t displacement=f.indexed?expected[rb]:uint32_t(int64_t(imm)-(imm>=32768?65536:0));
        const uint32_t address=uint32_t(uint64_t(ra?expected[ra]:0)+displacement);
        const unsigned offset=address%0x20000000u;
        const bool valid=(!f.update || (ra!=0 && (!f.load || rt!=ra))) && !reserved && !(address%f.width) &&
            (address/0x20000000u==0 || address/0x20000000u==4 || address/0x20000000u==6) && uint64_t(offset)+f.width<=m.ram.size();
        // Only valid mapped aligned cases seed RAM. Invalid cases must be atomic.
        std::array<uint8_t,16> bytes{}; size_t start=0;
        const bool seeded=uint64_t(offset)+16<=m.ram.size();
        if(seeded) {
            start=offset>=8?offset-8:0;
            for(unsigned n=0;n<16;++n) { bytes[n]=uint8_t((n*73+data/((n%4==0)?1u:n%4==1?256u:n%4==2?65536u:16777216u))%256); m.ram[start+n]=bytes[n]; }
        }
        auto memory=bytes; const size_t position=offset-start;
        if(valid) {
            uint32_t result=0;
            if(f.load) {
                for(unsigned n=0;n<f.width;++n) result=result*256+bytes[position+(f.reverse?f.width-1-n:n)];
                if(f.sign && result>=32768) result+=0xffff0000u;
                expected[rt]=result;
            } else {
                const uint32_t source=expected[rt];
                for(unsigned n=0;n<f.width;++n) memory[position+n]=uint8_t(source>>(8*(f.reverse?n:f.width-1-n)));
            }
            if(f.update) expected[ra]=address;
        }
        m.cpu.cr=cr; m.cpu.xer=xer; m.cpu.lr=0x1234567b; m.cpu.ctr=0x89abcdef;
        const uint32_t ins=f.indexed?31u<<26|rt<<21|ra<<16|rb<<11|f.code<<1|unsigned(reserved):f.code<<26|rt<<21|ra<<16|imm;
        execute(m,ins);
        require(m.cpu.unsupported==!valid && m.cpu.pc==(valid?0x80002004u:0x80002000u) && m.cpu.steps==1,"memory transfer decode/PC");
        require(m.cpu.r==expected && m.cpu.cr==cr && m.cpu.xer==xer && m.cpu.lr==0x1234567b && m.cpu.ctr==0x89abcdef,"memory transfer registers");
        if(seeded) require(std::equal(memory.begin(),memory.end(),m.ram.begin()+start),"memory transfer byte/guard mutation");
        ++counts.rows; if(!valid) ++counts.diagnostics;
    };
    // Every aligned D-form displacement on eight patterns and all three RAM aliases.
    for(const auto& f:memory_forms) if(!f.indexed)
        for(auto alias:aliases) for(auto data:values) for(unsigned imm=0;imm<65536;imm+=f.width) {
            const uint32_t d=uint32_t(int64_t(imm)-(imm>=32768?65536:0));
            check(f,11,3,10,imm,alias+0x100000-d,0,data,0xa5a5a5a5,((imm/f.width)%8)<<29|127);
        }
    // Every RT/RA/RB field, including source/base/index aliases and ordinary RB=0.
    for(const auto& f:memory_forms) {
        for(unsigned rt=0;rt<32;++rt) for(unsigned ra=0;ra<32;++ra) for(unsigned rb=0;rb<(f.indexed?32u:1u);++rb)
            check(f,rt,ra,rb,0,0x80010000,0x100,values[(rt+ra+rb)%8],0x12345678,((rt+ra+rb)%8)<<29|127);
        for(auto alias:aliases) for(auto data:values) for(uint32_t index:{0u,4u,0xffff8000u,0x7ffcu,0x80000000u,0xfffffffcu}) {
            if(!f.indexed) continue;
            check(f,11,3,10,0,alias+0x100000-index,index,data,0xffffffff,0xe000007f);
        }
        // Mapped unaligned and unmapped accesses preserve registers and target bytes.
        for(unsigned low=0;low<4;++low) for(uint32_t address:{0x80010000u,0x81800000u,0xa0010000u,0xfffffffcu})
            check(f,11,3,10,0,address+low,0,0x89abcdef,0xa5a5a5a5,0xe000007f);
        if(f.indexed) for(unsigned rt=0;rt<32;++rt) for(unsigned ra=0;ra<32;++ra) for(unsigned rb=0;rb<32;++rb)
            check(f,rt,ra,rb,0,0x80010000,0x100,0x89abcdef,0xa5a5a5a5,0xe000007f,true);
    }
    return counts;
}
static void memory_plugin_replay(emu::IEmulatorPlugin& plugin,bool wii) {
    auto rom=image(wii);
    const uint32_t words[]={15u<<26|3<<21|0x8000u,24u<<26|3<<21|3<<16|0x4000u,
        15u<<26|4<<21|0x89abu,24u<<26|4<<21|4<<16|0xcdefu,14u<<26|10<<21|4u,
        31u<<26|4<<21|3<<16|10<<11|183<<1,
        43u<<26|5<<21|3<<16|2u,14u<<26|11<<21|3<<16|0xfffeu,
        31u<<26|7<<21|11<<11|534<<1,31u<<26|5<<21|11<<11|918<<1,
        36u<<26|3<<21|0x100u,36u<<26|5<<21|0x104u,36u<<26|7<<21|0x108u,18u<<26};
    rom.resize(32+sizeof(words)); put(rom,16,sizeof(words));
    for(unsigned n=0;n<sizeof(words)/sizeof(words[0]);++n) put(rom,32+n*4,words[n]);
    require(plugin.load_rom(rom.data(),rom.size()),"memory guest plugin load"); plugin.run_frame({});
    const auto read=[&](unsigned address) { uint32_t value=0; for(unsigned n=0;n<4;++n) value=value*256+plugin.read_memory(address+n); return value; };
    require(read(0x100)==0x80004006 && read(0x104)==0xffffcdef && read(0x108)==0xefcdab89 && read(0x4004)==0xefcdcdef,"memory guest captures");
    std::vector<uint8_t> before,after,replayed; require(plugin.save_state(before),"memory plugin prefix");
    plugin.run_frame({}); require(plugin.save_state(after),"memory plugin final"); require(plugin.load_state(before),"memory plugin restore");
    plugin.run_frame({}); require(plugin.save_state(replayed) && replayed==after,"memory plugin replay");
    ppc::Machine machine(wii); require(machine.load(rom.data(),rom.size()),"memory machine load");
    for(unsigned n=0;n<5;++n) machine.step(); // Immediately before STWUX changes RAM and RA.
    require(machine.save(before),"memory update prefix"); machine.step(); require(machine.save(after),"memory update final");
    uint32_t value=0;
    require(machine.cpu.r[3]==0x80004004 && machine.read(0x4004,4,value) && value==0x89abcdef,"memory update result");
    require(machine.restore(before),"memory update restore"); machine.step();
    require(machine.save(replayed) && replayed==after,"memory update instruction replay");
}
