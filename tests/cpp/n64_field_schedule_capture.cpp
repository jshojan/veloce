#include "machine.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
using emulator_test::require;
static uint64_t be(const std::vector<uint8_t>& bytes,size_t at){uint64_t value=0;for(unsigned n=0;n<8;++n)value=value*256+bytes.at(at+n);return value;}
int main(int argc,char** argv){try{
    require(argc==4 || (argc==5 && std::string(argv[4])=="quantum"),"core, original ROM, output and optional quantum control");
    std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
    emulator_test::LoadedCore core(argv[1],rom);auto* scheduling=dynamic_cast<emu::IFrameScheduling*>(core.plugin);
    require(scheduling && scheduling->get_frame_scheduling()==emu::FrameScheduling::VideoField,"native startup must default to VI fields");
    n64::Machine initial;require(initial.load(rom.data(),rom.size()),"original cartridge load");
    for(unsigned n=0;n<10;++n)initial.cpu.step();
    require(initial.video_interface.active() && !initial.video_interface.phase && initial.cpu.state.instructions==10,"original guest configured VI");
    std::ofstream out(argv[3]);require(bool(out),"output file");out<<"{\"suite\":\"n64-vi-field-schedule\",\"cases\":5160,\"words\":19,\"rom_crc32\":"<<core.plugin->get_rom_crc32()<<"}\n";
    unsigned index=0;
    auto capture=[&](unsigned h,unsigned v,unsigned leap,unsigned a,unsigned b,unsigned pattern,unsigned current,unsigned phase,uint64_t crystal){
        auto& vi=initial.video_interface;vi.reset();
        require(vi.write(7,h|(leap<<16)) && vi.write(8,(a<<16)|b) && vi.write(6,v) && vi.write(0,2),"VI input registers");
        vi.pattern=pattern;vi.current=current;vi.phase=phase;vi.fields=0;
        initial.crystal_phase=crystal;initial.frame_scheduling=emu::FrameScheduling::VideoField;
        std::vector<uint8_t> input,actual;require(initial.save(input) && core.plugin->load_state(input),"native input checkpoint");
        if(argc==5)scheduling->set_frame_scheduling(emu::FrameScheduling::InstructionQuantum);
        core.plugin->run_frame({});core.plugin->run_frame({});require(core.plugin->save_state(actual),"actual production state");
        const size_t vi_at=actual.size()-1055152,ram_at=704+0x10000;
        const uint32_t guest=(uint32_t(actual.at(ram_at))<<24)|(uint32_t(actual.at(ram_at+1))<<16)|(uint32_t(actual.at(ram_at+2))<<8)|actual.at(ram_at+3);
        out<<"["<<index++<<','<<h<<','<<v<<','<<leap<<','<<a<<','<<b<<','<<pattern<<','<<current<<','<<phase<<','<<crystal
           <<','<<(core.plugin->get_cycle_count()-10)<<','<<be(actual,vi_at+15*8)<<','<<be(actual,vi_at+16*8)<<','<<be(actual,vi_at+17*8)<<','<<be(actual,vi_at+18*8)
           <<','<<be(actual,vi_at-33840)<<','<<guest<<','<<core.plugin->get_frame_count()<<','<<(scheduling->get_frame_scheduling()==emu::FrameScheduling::VideoField?1:0)<<"]\n";
    };
    // Every horizontal period with a one-line field; every vertical terminal
    // count with nontrivial lines. These are separate exhaustive field domains.
    for(unsigned h=0;h<4096;++h)capture(h,0,0,h,h,0,0,h/2,(uint64_t(h)*7919)%93750000);
    for(unsigned v=0;v<1024;++v)capture(5,v,0,5,5,0,0,3,(uint64_t(v)*104729)%93750000);
    for(unsigned pattern=0;pattern<5;++pattern)for(unsigned leap:{0u,31u,21u,10u})capture(3,5,leap,7,11,pattern,0,1,pattern*17000000);
    for(unsigned pattern=0;pattern<5;++pattern)for(unsigned current:{1u,2u,3u,4u})capture(9,6,21,4,15,pattern,current,0,pattern*17000000);
    require(index==5160,"complete capture domain");out<<"{\"complete\":5160}\n";out.close();require(bool(out),"complete file write");
    if(argc==5)return 0;
    // Mode is serialized, restored atomically and preserved across reset. Legacy
    // S029 always resumes the exact old quantum regardless of native boot mode.
    std::vector<uint8_t> saved,replay;require(core.plugin->save_state(saved),"mode save");
    require(saved.size()==9528128 && saved[6]=='3' && saved[7]=='2',"S032 exact ABI capacity");
    for(unsigned bit=0;bit<64;++bit){if(bit==0 || bit==8 || bit==16)continue;auto bad=saved;bad[639-bit/8]|=uint8_t(1u<<(bit%8));require(!core.plugin->load_state(bad) && core.plugin->save_state(replay) && replay==saved,"reserved schedule flag atomic rejection");}
    scheduling->set_frame_scheduling(emu::FrameScheduling::InstructionQuantum);require(core.plugin->load_state(saved) && scheduling->get_frame_scheduling()==emu::FrameScheduling::VideoField,"field schedule restore");
    auto old=saved;old.resize(old.size()-512);old[6]='2';old[7]='9';old[638]=0;require(core.plugin->load_state(old) && scheduling->get_frame_scheduling()==emu::FrameScheduling::InstructionQuantum,"S029 exact legacy scheduling");
    const auto before=core.plugin->get_cycle_count();core.plugin->run_frame({});require(core.plugin->get_cycle_count()-before==100000,"actual imported quantum budget");
    core.plugin->reset();require(scheduling->get_frame_scheduling()==emu::FrameScheduling::InstructionQuantum,"legacy movie reset must preserve schedule");
    const auto reset_start=core.plugin->get_cycle_count();core.plugin->run_frame({});require(core.plugin->get_cycle_count()-reset_start==100000,"actual legacy reset budget");
    core.fresh_load(rom);scheduling=dynamic_cast<emu::IFrameScheduling*>(core.plugin);
    require(scheduling && scheduling->get_frame_scheduling()==emu::FrameScheduling::VideoField,"new native session selects VI after legacy reset");
    // Actual ordinary and rollback execution have identical complete states.
    require(core.plugin->load_state(saved),"ordinary start");core.plugin->run_frame({7});core.plugin->save_state(replay);
    require(core.plugin->load_state(saved),"rollback start");core.netplay().run_frame_netplay_n({7,0,0,0});require(emulator_test::snapshot(core.netplay())==replay,"VI ordinary/rollback state equality");
    // Unconfigured VI is bounded while native startup still executes normally.
    initial.video_interface.reset();initial.frame_scheduling=emu::FrameScheduling::VideoField;
    initial.save(saved);require(core.plugin->load_state(saved),"blank start");const auto blank=core.plugin->get_cycle_count();core.plugin->run_frame({});require(core.plugin->get_cycle_count()-blank==1562500,"blank VI startup budget");
    // Largest stable register combination must reach its field ending rather
    // than the safety cap. An intentionally saturated diagnostic counter must
    // still return within the documented cap.
    initial.video_interface.write(7,4095);initial.video_interface.write(8,0x0fff0fff);
    initial.video_interface.write(6,1023);initial.video_interface.write(0,2);initial.crystal_phase=0;
    initial.save(saved);require(core.plugin->load_state(saved),"largest field start");const auto maximum=core.plugin->get_cycle_count();core.plugin->run_frame({});
    require(core.plugin->save_state(saved) && be(saved,saved.size()-1055152+17*8)==1,"largest stable field boundary");
    const uint64_t wanted=(uint64_t(512)*4096*93750000+48681817)/48681818;
    require(core.plugin->get_cycle_count()-maximum==wanted,"largest field rational clock duration");
    initial.video_interface.fields=~uint64_t(0);initial.save(saved);require(core.plugin->load_state(saved),"saturated start");const auto bounded=core.plugin->get_cycle_count();core.plugin->run_frame({});require(core.plugin->get_cycle_count()-bounded==6250000,"saturated-counter safety cap");
    for(unsigned offset:{4u,12u}){
        initial.reset();for(unsigned n=0;n<10;++n)initial.cpu.step();
        initial.cpu.state.gpr[30]=n64::sign32(0xbff00000);
        const uint32_t store=(43u<<26)|(30u<<21)|offset;
        for(unsigned byte=0;byte<4;++byte)initial.sp[0x68+byte]=uint8_t(store>>(24-byte*8));
        initial.save(saved);require(core.plugin->load_state(saved),"debug completion start");const auto before_debug=core.plugin->get_cycle_count();core.plugin->run_frame({});
        require(core.plugin->get_cycle_count()-before_debug==1 && core.plugin->save_state(saved) && be(saved,offset==4?32:632)==(offset==4?1:257),"native debug done/yield boundary");
    }
    std::cout<<"5160 actual VI boundaries, guest CURRENT RAM, S030 mode/61 rejection controls, S029 budget and native reset, ordinary/rollback and blank startup PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
