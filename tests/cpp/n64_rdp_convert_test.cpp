#include "machine.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
static void verify(bool v,unsigned line){if(!v)throw std::runtime_error("conversion mismatch at line "+std::to_string(line));}
#define check(...) verify((__VA_ARGS__),__LINE__)
static void put(std::vector<uint8_t>& memory,unsigned at,uint64_t word){for(unsigned n=0;n<8;++n)memory[at+n]=uint8_t(word>>(56-n*8));}
int main(int argc,char** argv){try{
    uint64_t commands=0;
    n64::RdpConvert initial;check(initial.weights()==std::array<int64_t,4>{});initial.command(0);check(initial.weights()==std::array<int64_t,4>{1,1,1,1});
    for(unsigned field=0;field<6;++field)for(unsigned raw=0;raw<512;++raw)for(unsigned pattern=0;pattern<4;++pattern){
        std::array<unsigned,6> expected{};uint64_t payload=0;
        for(unsigned n=0;n<6;++n){expected[n]=n==field?raw:(pattern*137+n*73)%512;payload=payload*512+expected[n];}
        for(unsigned alias:{44u,108u,172u,236u})for(unsigned xbus=0;xbus<2;++xbus){
            n64::DpInterface dp;std::vector<uint8_t> ram(4096);std::array<uint8_t,8192> sp{};bool irq=false;
            const auto word=(uint64_t(alias)<<56)|(uint64_t(pattern)<<54)|payload;
            put(ram,0xff8,word);for(unsigned n=0;n<8;++n)sp[0xff8+n]=ram[0xff8+n];
            put(ram,0,0xe900000000000000);for(unsigned n=0;n<8;++n)sp[n]=ram[n];
            dp.flags=xbus;dp.write(0,0xff8);dp.write(1,0x1000);dp.tick(ram,sp,irq);
            check(!dp.blocked && dp.current==0x1000 && !irq && dp.conversion.snapshot()==(payload|(1ull<<54)));
            for(unsigned n=0;n<6;++n)check(dp.conversion.coefficient(n)==expected[n]);
            const auto weights=dp.conversion.weights();for(unsigned n=0;n<4;++n)check(weights[n]==2*(int(expected[n])-(expected[n]>=256?512:0))+1);
            if(xbus){dp.write(1,0x1008);dp.tick(ram,sp,irq);check(irq && !dp.pipeline);}
            ++commands;
        }
    }
    unsigned pixels=0;
    n64::RdpPacket triangle;triangle.count=triangle.needed=12;triangle.words[0]=0xca80001000080000ull;
    triangle.words[1]=0x00020000ffff0000ull;triangle.words[2]=0;triangle.words[3]=65536;triangle.words[4]=(25ull<<48)|(17ull<<32);
    n64::RdpTexture texture;texture.tiles[0].descriptor=(1ull<<53)|(2ull<<51)|(1ull<<41);texture.tiles[0].bounds=(4ull<<12)|4;
    n64::RdpTmem tmem;tmem.bytes[0]=128;tmem.bytes[1]=192;tmem.bytes[2048]=10;tmem.bytes[2049]=20;tmem.bytes[2060]=30;tmem.bytes[2061]=40;
    n64::RdpCombiner combiner;combiner.combine=(15ull<<37)|(15ull<<24)|(31ull<<32)|(1ull<<6)|(7ull<<21)|(7ull<<3)|(7ull<<18)|1;
    for(unsigned probe=0;probe<4;++probe){n64::RdpConvert conversion;if(probe)conversion.command(uint64_t(probe==1?1:probe==2?255:256)<<45);
        std::vector<uint8_t> ram(160,0x5a),expected=ram;const unsigned red[]={40,41,168,0};
        for(unsigned at:{4u,8u,9u,12u}){expected[64+at*4]=uint8_t(red[probe]);expected[64+at*4+1]=expected[64+at*4+2]=40;expected[64+at*4+3]=uint8_t((at==4 || at==8?7:3)*32);}
        check(n64::RdpTriangleShade::draw(triangle,(3ull<<51)|(3ull<<32)|64,(15ull<<36)|(1ull<<45),(16ull<<12)|16,combiner,ram,&texture,&tmem,&conversion)==0 && ram==expected);++pixels;
    }
    check(argc==2);std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};
    n64::Machine machine;check(machine.load(rom.data(),rom.size()));auto& dp=machine.dp_interface;
    put(machine.ram,0x100,0xec3fffffffffffff);put(machine.ram,0x108,0xe900000000000000);
    dp.flags=2;dp.write(0,0x100);dp.write(1,0x110);
    std::vector<uint8_t> saved,after,replay;check(machine.save(saved) && saved.size()==9529672 && saved[6]=='3' && saved[7]=='3');
    dp.flags=0;machine.clock();machine.clock();machine.clock();check(machine.save(after) && dp.conversion.snapshot()==((1ull<<55)-1) && machine.dp_interrupt);
    check(machine.restore(saved));dp.flags=0;machine.clock();machine.clock();machine.clock();check(machine.save(replay) && replay==after);
    for(unsigned bit=55;bit<64;++bit){auto bad=after;bad[bad.size()-1050728+7-bit/8]|=uint8_t(1u<<(bit%8));check(!machine.restore(bad) && machine.save(replay) && replay==after);}
    auto bad=after;bad[bad.size()-1050728+1]&=~64u;check(!machine.restore(bad) && machine.save(replay) && replay==after);
    auto old=after;old.resize(old.size()-1050728);old[6]='2';old[7]='4';check(machine.restore(old) && dp.conversion.snapshot()==0 && machine.save(replay) && std::equal(old.begin()+8,old.end(),replay.begin()+8));
    machine.reset();check(dp.conversion.snapshot()==0);
    std::cout<<commands<<" independent field/alias/DRAM-XBUS commands; "<<pixels<<" mixed-chroma/luma framebuffer goldens; reset weights, FullSync and S030 replay/rejection/S024 import PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
