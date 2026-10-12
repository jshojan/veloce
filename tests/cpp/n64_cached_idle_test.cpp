#include "vr4300.hpp"
#include <array>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace n64;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Bus:Memory {
    std::array<uint8_t,32768> ram{};
    uint64_t ticks=0,reads=0,writes=0,irq_at=0;
    void clock() override{++ticks;}
    uint8_t interrupt_pending()const override{return irq_at && ticks>=irq_at?4:0;}
    bool read(uint64_t address,unsigned bytes,uint64_t& value)override{
        ++reads;const uint32_t p=uint32_t(address)&0x1fffffff;
        if(p>ram.size() || bytes>ram.size()-p)return false;
        value=0;for(unsigned n=0;n<bytes;++n)value=(value<<8)|ram[p+n];return true;
    }
    bool write(uint64_t address,unsigned bytes,uint64_t value)override{return write_masked(address,bytes,value,uint8_t((1u<<bytes)-1));}
    bool write_masked(uint64_t address,unsigned bytes,uint64_t value,uint8_t mask)override{
        ++writes;const uint32_t p=uint32_t(address)&0x1fffffff;
        if(p>ram.size() || bytes>ram.size()-p)return false;
        for(unsigned n=0;n<bytes;++n)if(mask&(1u<<n))ram[p+n]=uint8_t(value>>(8*(bytes-1-n)));
        return true;
    }
};
static void equal(const VR4300& a,const VR4300& b,const Bus& x,const Bus& y){
    const auto& s=a.state;const auto& t=b.state;
    require(s.gpr==t.gpr && s.cp0==t.cp0 && s.fpr==t.fpr && s.fcr31==t.fcr31 &&
        s.hi==t.hi && s.lo==t.lo && s.pc==t.pc && s.next_pc==t.next_pc &&
        s.instructions==t.instructions && s.delay_slot==t.delay_slot && s.llbit==t.llbit,"CPU continuation changed");
    require(a.exception_taken==b.exception_taken && a.instruction_valid==b.instruction_valid &&
        a.instruction_word==b.instruction_word && a.access_address==b.access_address &&
        a.access_bytes==b.access_bytes && a.access_store==b.access_store,"instruction diagnostics changed");
    for(unsigned n=0;n<512;++n){
        const auto& i=a.caches.instruction[n];const auto& j=b.caches.instruction[n];
        const auto& d=a.caches.data[n];const auto& e=b.caches.data[n];
        require(i.page==j.page && i.flags==j.flags && i.data==j.data &&
            d.page==e.page && d.flags==e.flags && d.data==e.data,"cache continuation changed");
    }
    for(unsigned n=0;n<32;++n){const auto& i=a.tlb.entries[n];const auto& j=b.tlb.entries[n];
        require(i.hi==j.hi && i.mask==j.mask && i.even==j.even && i.odd==j.odd,"TLB continuation changed");}
    require(x.ram==y.ram && x.ticks==y.ticks && x.reads==y.reads && x.writes==y.writes,"bus clock or transaction changed");
}
int main(){try{
    uint64_t optimized=0,checks=0;
    for(unsigned bulk=0;bulk<2;++bulk)for(unsigned scenario=0;scenario<12;++scenario)for(unsigned offset=0;offset<=28;offset+=4){
        Bus x,y;VR4300 a(x),b(y);
        uint64_t pc=0xffffffff80000100ull+offset;
        if(scenario==1)pc+=0x20000000;
        if(scenario==2)pc=uint32_t(pc);
        a.reset(pc);b.reset(pc);a.state.cp0[16]=b.state.cp0[16]=3;b.cached_idle_enabled=false;
        a.state.cp0[6]=b.state.cp0[6]=offset;
        x.write(pc,4,scenario==5?0x1400ffff:0x1000ffff);x.write(pc+4,4,scenario==4?0x25080001:0);
        if(scenario==3)a.state.cp0[12]=b.state.cp0[12]=16;
        if(scenario==6 || scenario==7){x.irq_at=y.irq_at=scenario==6?10:11;a.state.cp0[12]=b.state.cp0[12]=0x401;}
        if(scenario==8 || scenario==9){a.state.cp0[12]=b.state.cp0[12]=0x8001;
            a.state.cp0[9]=b.state.cp0[9]=scenario==8?0:0xfffffffd;
            a.state.cp0[11]=b.state.cp0[11]=scenario==8?5:0;}
        y.ram=x.ram;y.writes=x.writes;
        for(unsigned step=0;step<64;){
            if(scenario==10 && step==8){x.write(pc+4,4,0x25080001);y.write(pc+4,4,0x25080001);}
            if(scenario==10 && step==16){a.caches.instruction[(pc>>5)&511].flags=0;b.caches.instruction[(pc>>5)&511].flags=0;}
            if(scenario==11 && step==12){a.caches.instruction[(pc>>5)&511].page^=1;b.caches.instruction[(pc>>5)&511].page^=1;}
            unsigned maximum=std::min(4u,64-step);
            if(scenario==10 && step<8)maximum=std::min(maximum,8-step);
            if(scenario==10 && step<16)maximum=std::min(maximum,16-step);
            if(scenario==11 && step<12)maximum=std::min(maximum,12-step);
            const unsigned idle=bulk?a.cached_idle_budget(maximum):0;
            unsigned advanced=1;
            if(idle){advanced=0;do{x.clock();++advanced;
                    if((a.state.cp0[12]&7)==1 && (a.state.cp0[12]&(uint64_t(x.interrupt_pending())<<8)))break;
                }while(advanced<idle);
                a.complete_cached_idle(advanced,x.interrupt_pending());
            }else a.step();
            for(unsigned n=0;n<advanced;++n)b.step();
            equal(a,b,x,y);checks+=advanced;step+=advanced;
        }
        if(scenario==0 && offset<=24)require(a.cached_idle_steps>0,"eligible loop was not optimized");
        if(scenario>=1 && scenario<=5)require(a.cached_idle_steps==0,"ineligible loop was optimized");
        if(offset==28)require(a.cached_idle_steps==0,"cross-line pair was optimized");
        optimized+=a.cached_idle_steps;
    }
    for(unsigned wired:{0u,1u,15u,30u,31u})for(unsigned random:{0u,1u,15u,30u,31u})for(unsigned parity:{0u,1u}){
        Bus x,y;VR4300 a(x),b(y);const uint64_t pc=0xffffffff80000100ull;
        a.reset(pc);b.reset(pc);b.cached_idle_enabled=false;
        a.state.cp0[16]=b.state.cp0[16]=3;a.state.instructions=b.state.instructions=parity;
        a.state.cp0[6]=b.state.cp0[6]=wired;a.state.cp0[1]=b.state.cp0[1]=32|random;
        a.state.cp0[9]=b.state.cp0[9]=0xfffffffd;a.state.cp0[11]=b.state.cp0[11]=1;
        x.write(pc,4,0x1000ffff);y.ram=x.ram;y.writes=x.writes;
        for(unsigned step=0;step<2048;){
            const unsigned idle=a.cached_idle_budget(std::min(257u,2048-step));
            if(idle){for(unsigned n=0;n<idle;++n)x.clock();a.complete_cached_idle(idle,x.interrupt_pending());}
            else a.step();
            const unsigned advanced=idle?idle:1;
            for(unsigned n=0;n<advanced;++n)b.step();
            equal(a,b,x,y);checks+=advanced;step+=advanced;
        }
        require(a.cached_idle_steps>0,"counter batch did not optimize");optimized+=a.cached_idle_steps;
    }
    std::cout<<checks<<" CPU attempts covered by scalar/batched CPU/cache/TLB/bus continuation checks; "<<optimized<<" cached idle attempts PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
