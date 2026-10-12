#include "machine.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace n64;
static void require(bool ok){if(!ok)throw std::runtime_error("DP idle clock differs from original tick");}
static void command(Machine& m,unsigned at,uint64_t word){
    for(unsigned i=0;i<8;++i)m.ram[at+i]=uint8_t(word>>(56-8*i));
}
static void prepare(Machine& m,unsigned seed){
    auto& d=m.dp_interface;d.reset();m.dp_interrupt=false;m.rsp_phase=seed%3;
    m.rsp.reset();
    m.sp_interface.status=1;m.sp_interface.pc=0;m.sp.fill(0);
    d.clock=(seed*733)&0xffffff;d.pipe_busy=(seed*997)&0xffffff;
    d.pipeline=!(seed&1);d.flags=(seed>>1)&7;d.blocked=false;
    command(m,0x100,(seed&16)?0x2900000000000000ull:0);
    switch((seed>>4)%8){
    case 0:break;
    case 1:d.current=d.active_end=0x108;break;
    case 2:d.current=d.active_end=0x100;d.start_valid=true;d.start=0x100;break;
    case 3:d.current=d.active_end=0x100;d.start_valid=d.end_valid=true;d.start=0x100;d.end=0x108;break;
    case 4:d.current=0x100;d.active_end=0x108;break;
    case 5:d.current=d.active_end=0x100;d.packet.push(0x100,0x2900000000000000ull);break;
    case 6:d.current=d.active_end=0x100;d.blocked=true;d.blocked_reason=1;d.blocked_word=0xfeed;break;
    case 7:{
        d.start_valid=true;d.start=0x100;
        m.sp_interface.status=0;m.rsp_phase=1;
        const uint32_t mtc0=0x40824800;
        for(unsigned i=0;i<4;++i)m.sp[4096+i]=uint8_t(mtc0>>(24-8*i));
        m.sp_interface.invalidate_imem();
        m.rsp.gpr[2]=0x108;
        break;
    }
    }
}
int main(){try{
    auto fast=std::make_unique<Machine>(),slow=std::make_unique<Machine>();
    fast->rom.assign(0x1004,0);slow->rom=fast->rom;
    slow->fast_dp_clock_enabled=false;
    uint64_t ticks=0;
    for(unsigned seed=0;seed<8192;++seed){
        prepare(*fast,seed);prepare(*slow,seed);
        for(unsigned step=0;step<5;++step){
            fast->clock();slow->clock();++ticks;
            if(fast->dp_interface.snapshot()!=slow->dp_interface.snapshot() ||
               fast->dp_interface.packet.snapshot()!=slow->dp_interface.packet.snapshot() ||
               fast->dp_interrupt!=slow->dp_interrupt || fast->rsp.snapshot()!=slow->rsp.snapshot()){
                std::cerr<<"seed="<<seed<<" step="<<step<<'\n';require(false);
            }
        }
        // One full state per 64 seeds, rotating through all eight stream
        // scenarios rather than sampling only the settled case.
        if((seed&63)==((seed>>6)&7)*8){
            std::vector<uint8_t> a,b;require(fast->save(a)&&slow->save(b));
            if(a!=b){
                unsigned at=0;while(at<a.size()&&a[at]==b[at])++at;
                std::cerr<<"seed="<<seed<<" state byte="<<at<<'\n';require(false);
            }
        }
    }
    std::cout<<ticks<<" DP ticks across settled/queued/frozen/blocked/RSP-write cases and 128 full states PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
