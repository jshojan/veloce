#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <iostream>
#include <cmath>
using emulator_test::require;
static uint64_t address(unsigned n) {return n64::sign32(0xa4500000+n*4);}
static void put(n64::Machine& m,unsigned n,uint32_t v) {require(m.write(address(n),4,v),"AI write");}
static uint32_t get(n64::Machine& m,unsigned n) {uint64_t v=0;require(m.read(address(n),4,v),"AI read");return uint32_t(v);}
static std::vector<uint8_t> image(const char* name) {std::ifstream f(name,std::ios::binary);require(f.good(),"ROM missing");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv) {
 try {
    require(argc==3,"plugin and original PCM ROM required");auto rom=image(argv[2]);n64::Machine m;require(m.load(rom.data(),rom.size()),"ROM load");auto& a=m.audio_interface;
    for(unsigned raw=0;raw<262144;++raw) {
        a.reset();put(m,0,0xff70000f);put(m,1,0xfffc0000|raw);const unsigned length=raw/8*8;
        for(unsigned reg:{0u,1u,2u,4u,5u})require(get(m,reg)==length,"all 18-bit lengths/read mirrors");
        require(a.address==0x700008 && a.count==unsigned(length!=0) && a.interrupt==unsigned(length!=0),"aligned address, initial FIFO/IRQ");
    }
    for(unsigned value=0;value<2097152;++value) {put(m,0,0xff000007u|(value*8));require(a.address==value*8,"all 21-bit aligned addresses");}
    for(unsigned rate=0;rate<16384;++rate)for(unsigned bit=0;bit<16;++bit) {put(m,4,0xffffc000|rate);put(m,5,0xfffffff0|bit);require(a.dacrate==rate && a.bitrate==bit,"rate/bit masks");}
    // Actual signed stereo domain, fetched from RAM when played, through two FIFO slots.
    a.reset();for(unsigned n=0;n<65536;++n) {unsigned at=0x700008+n*4;const unsigned r=n^65535;m.ram[at]=uint8_t(n>>8);m.ram[at+1]=uint8_t(n);m.ram[at+2]=uint8_t(r>>8);m.ram[at+3]=uint8_t(r);}
    put(m,2,1);put(m,5,15);put(m,0,0x700008);put(m,1,131072);put(m,3,0);put(m,0,0x720008);put(m,1,131072);
    require(a.count==2 && !a.interrupt,"second FIFO slot raised premature IRQ");auto full=a.snapshot();put(m,1,8);require(a.snapshot()==full,"full FIFO length write modified queue");
    for(unsigned n=0;n<65536;++n) {
        a.tick(m.ram);require(a.valid && a.left==n && a.right==(n^65535),"signed stereo PCM domain");
        if(n==32767) {require(a.count==1 && a.interrupt && a.fifo[0].address==0x720008,"FIFO promotion/start IRQ");put(m,3,0);}
    }
    require(!a.count && !a.interrupt,"empty FIFO raised completion IRQ");a.tick(m.ram);require(!a.valid && !a.left && !a.right,"underrun PCM");
    // Every DAC divider schedules an 8-byte buffer at the declared crystal period.
    for(unsigned rate=0;rate<16384;++rate) {
        a.reset();put(m,2,1);put(m,5,15);put(m,4,rate);put(m,0,0x700008);put(m,1,8);
        for(unsigned tick=0;tick<rate;++tick)a.tick(m.ram);
        require(a.fifo[0].remaining==8,"sample before divider expiry");a.tick(m.ram);require(a.fifo[0].remaining==4,"missing sample at divider expiry");
    }
    a.reset();put(m,2,1);put(m,5,15);put(m,0,0x701ff8);put(m,1,8);put(m,0,0x710000);put(m,1,8);
    m.ram[0x712000]=0x12;m.ram[0x712001]=0x34;m.ram[0x712002]=0xab;m.ram[0x712003]=0xcd;
    a.tick(m.ram);a.tick(m.ram);require(a.fifo[0].address==0x712000,"8 KiB delayed carry");a.tick(m.ram);require(a.left==0x1234 && a.right==0xabcd,"delayed-carry source bytes");
    put(m,5,0);auto paused=a.snapshot();for(unsigned n=0;n<100;++n)a.tick(m.ram);require(a.snapshot()==paused,"bitrate-zero clock pause");
    put(m,5,15);put(m,2,0);const auto remaining=a.fifo[0].remaining;for(unsigned n=0;n<100;++n)a.tick(m.ram);require(a.fifo[0].remaining==remaining,"DMA disable did not pause consumption");
    m.clear_audio();put(m,2,1);a.tick(m.ram);require(a.valid,"pause source sample");put(m,5,0);for(unsigned n=0;n<100000;++n)m.clock();require(!m.audio_count,"stopped DAC generated host samples");
    // Bounds, including the carry applied to a pending descriptor, reject atomically.
    a.reset();put(m,0,0x701ff8);put(m,1,8);put(m,0,0x7ffff8);full=a.snapshot();require(!m.write(address(1),4,8) && a.snapshot()==full,"pending carry bounds");
    a.reset();put(m,0,0x7ffff8);full=a.snapshot();require(!m.write(address(1),4,16) && a.snapshot()==full,"DMA RAM bounds");
    // RAM is sampled at playback; queued data is not snapshotted or cache-snooped.
    m.reset();m.ram[0x700000]=9;put(m,2,1);put(m,5,15);put(m,0,0x700000);put(m,1,8);
    require(m.cpu.caches.write(m,n64::sign32(0x80700000),1,77,1,3),"dirty audio source");a.tick(m.ram);require(a.left==0x0900,"AI read dirty CPU cache");
    m.ram[0x700004]=31;a.tick(m.ram);require(a.left==0x1f00,"AI snapshotted queued RAM");
    uint32_t tag=0;require(m.cpu.caches.operation(m,25,n64::sign32(0x80700000),tag),"source cache writeback");
    put(m,0,0x700000);put(m,1,8);a.tick(m.ram);require(a.left==0x4d00,"AI did not see cache writeback");
    m.reset();put(m,2,1);put(m,5,15);put(m,0,0x700008);put(m,1,8);require(!m.interrupt_pending(),"unmasked AI IRQ");
    require(m.write(n64::sign32(0xa430000c),4,32),"MI AI mask");m.cpu.state.cp0[12]=0x401;m.cpu.step();require((m.cpu.state.cp0[12]&2) && (m.cpu.state.cp0[13]&0x400),"AI CPU interrupt");put(m,3,0xffffffff);require(!m.interrupt_pending(),"AI IRQ acknowledge");
    require(m.write(n64::sign32(0xa0000180),4,0x42000018),"ERET instruction");m.cpu.step();require(!(m.cpu.state.cp0[12]&2),"AI interrupt ERET");
    // Partially consumed FIFO, timer phases and undelivered host PCM round-trip exactly.
    m.reset();put(m,2,1);put(m,5,15);put(m,4,1023);put(m,0,0x700008);put(m,1,128);put(m,0,0x700108);put(m,1,64);
    for(unsigned n=0;n<4097;++n)m.clock();std::vector<uint8_t> state,after,replay;require(m.save(state),"audio state save");
    auto advance=[&]() {for(unsigned n=0;n<100000;++n)m.clock();};advance();require(m.save(after) && m.restore(state),"audio state restore");advance();require(m.save(replay) && replay==after,"audio exact replay");
    constexpr uint64_t invalid[]={1,2,16384,16,3,2,16384,2,1,1,1,1,65536,65536,2,93750000,93750000,8192,8193};
    for(unsigned field=0;field<19;++field) {auto bad=after;const size_t at=bad.size()-n64::Machine::audio_state_size-8+field*8;for(unsigned byte=0;byte<8;++byte)bad[at+byte]=uint8_t(invalid[field]>>(56-byte*8));require(!m.restore(bad) && m.save(replay) && replay==after,"audio invalid state atomicity");}
    auto old=after;old.resize(old.size()-n64::Machine::audio_state_size-8);old[6]='0';old[7]='8';require(m.restore(old) && !a.count && !m.audio_count && !m.crystal_phase,"S008 neutral audio conversion");
    // Bound host output without stopping guest DMA.
    m.reset();put(m,2,1);put(m,5,15);put(m,4,16383);put(m,0,0x700008);put(m,1,0x3fff8);
    for(unsigned n=0;n<18000000;++n)m.clock();require(m.audio_count==8192 && m.audio_dropped>0 && a.count==1,"host ring bound/backpressure");
    for(unsigned reg=0;reg<6;++reg)for(unsigned width:{1u,2u,8u}) {uint64_t v=99;require(!m.read(address(reg),width,v) && v==99 && !m.write(address(reg),width,0),"AI MMIO width");}
    for(unsigned reg=0;reg<6;++reg) {uint64_t v=99;require(!m.read(address(reg)+1,4,v) && v==99 && !m.write(address(reg)+1,4,0),"AI alignment");}
    // Production buffered and callback paths must return identical samples/states.
    emulator_test::LoadedCore buffered(argv[1],rom),streamed(argv[1],rom);std::vector<float> stream;
    streamed.plugin->set_audio_callback([&](const float* samples,size_t pairs,int rate) {require(rate==48000,"stream rate");stream.insert(stream.end(),samples,samples+pairs*2);});
    bool finished=false;uint64_t pairs=0;
    for(unsigned frame=0;frame<4000;++frame) {
        stream.clear();buffered.plugin->run_frame({});streamed.plugin->run_frame({});auto output=buffered.plugin->get_audio();
        require(stream.size()==size_t(output.sample_count)*2 && std::equal(stream.begin(),stream.end(),output.samples),"stream/buffer PCM mismatch");pairs+=output.sample_count;
        buffered.plugin->clear_audio_buffer();require(buffered.plugin->save_state(state) && streamed.plugin->save_state(replay) && replay==state,"audio consumers changed emulated state");
        if(state[39]) {finished=true;break;}
    }
    require(finished && pairs>65536,"loaded PCM ROM did not produce domain");buffered.plugin->reset();require(buffered.plugin->get_audio().sample_count==0,"plugin audio reset");
    std::cout<<"262144 lengths, 2097152 aligned addresses, 262144 rate/bit fields, 65536 PCM codes and 16384 dividers passed; FIFO/IRQ/carry, pause, state/ring bounds and production buffered/streaming output passed\n";return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
