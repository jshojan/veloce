#include "machine.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace n64;
static void prepare(Machine& m,unsigned scenario,bool field){
    m.rom.assign(0x1004,0);m.entry=0xffffffff80000100ull;m.reset();
    m.cpu.state.cp0[16]=3;m.cpu.state.cp0[6]=scenario%32;
    m.cpu.state.cp0[9]=0xfffffffd;m.cpu.state.cp0[11]=1;
    m.cpu.state.cp0[12]=scenario%3==0?0x8401:0;
    m.mi_mask=scenario%3==0?44:0;
    m.write(0xffffffffa0000100ull,4,0x1000ffff);m.write(0xffffffffa0000104ull,4,0);
    m.frame_scheduling=field?emu::FrameScheduling::VideoField:emu::FrameScheduling::InstructionQuantum;
    m.rsp_phase=scenario%3;m.crystal_phase=93749999-scenario*101;m.output_phase=93749999-scenario*13;
    m.dp_interface.clock=m.dp_interface.pipe_busy=0xfffffe;
    if(scenario&1){
        m.video_interface.write(6,7);m.video_interface.write(7,1023+scenario*7);
        m.video_interface.write(8,(1000u<<16)|1100u);m.video_interface.write(3,2);
        m.video_interface.write(0,2);m.video_interface.phase=m.video_interface.period()-1;
    }
    if(scenario&2){
        m.audio_interface.write(4,15+scenario*123,m.ram.size());m.audio_interface.write(5,1,m.ram.size());
        m.audio_interface.write(2,1,m.ram.size());m.audio_interface.write(0,0x3000,m.ram.size());
        m.audio_interface.write(1,256,m.ram.size());m.audio_interface.write(1,512,m.ram.size());
        m.audio_interface.phase=m.audio_interface.dacrate;
        for(unsigned n=0;n<768;++n)m.ram[0x3000+n]=uint8_t(n*17);
    }
    if(scenario&4){
        m.sp_interface.status=0;
        if(scenario&16){
            m.sp_interface.status=64;
            m.mi_mask|=1;
            m.sp[4096]=0;m.sp[4097]=0;m.sp[4098]=0;m.sp[4099]=13;
            m.sp_interface.invalidate_imem();
        }
    }
    if(scenario&8){
        m.write(0xffffffffa0000400ull,8,0xe900000000000000ull);
        m.dp_interface.write(0,0x400);m.dp_interface.write(1,0x408);
    }
    if(scenario&16){m.joybus.rtc.configure(true);m.joybus.rtc.clocks=93749995;}
}
int main(){try{
    uint64_t optimized=0,active=0,breaks=0,fullsyncs=0;
    for(unsigned scenario=0;scenario<32;++scenario)for(bool field:{false,true}){
        auto fast=std::make_unique<Machine>(),reference=std::make_unique<Machine>(),unbatched=std::make_unique<Machine>();
        prepare(*fast,scenario,field);prepare(*reference,scenario,field);prepare(*unbatched,scenario,field);
        reference->cpu.cached_idle_enabled=false;
        unbatched->active_rsp_batch_enabled=false;
        for(unsigned frame=0;frame<2;++frame){
            fast->run_frontend_frame();reference->run_frontend_frame();unbatched->run_frontend_frame();
            std::vector<uint8_t> a,b,c;
            if(!fast->save(a) || !reference->save(b) || !unbatched->save(c) || a!=b || a!=c){
                const auto where=std::mismatch(a.begin(),a.end(),b.begin(),b.end());
                std::cerr<<"scenario="<<scenario<<" field="<<field<<" frame="<<frame<<" byte="<<(where.first-a.begin())<<'\n';
                throw std::runtime_error("complete machine continuation changed");
            }
        }
        optimized+=fast->cpu.cached_idle_steps;
        active+=fast->active_rsp_batched_clocks;
        if((scenario&20)==20 && fast->sp_interface.interrupt)++breaks;
        if((scenario&8) && fast->dp_interrupt)++fullsyncs;
    }
    if(!optimized)throw std::runtime_error("idle clock cases never optimized");
    if(!active)throw std::runtime_error("active RSP clocks never batched");
    if(!breaks || !fullsyncs)throw std::runtime_error("SP/DP interrupt paths never exercised");
    std::cout<<"128 complete machine continuations, VI/AI/RSP/DP/RTC/BREAK/interrupt boundaries; "<<optimized<<" cached attempts, "<<active<<" active RSP clocks, "<<breaks<<" BREAK and "<<fullsyncs<<" FullSync continuations PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
