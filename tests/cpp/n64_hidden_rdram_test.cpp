#include "hidden_rdram.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

static void verify(bool ok,unsigned line){if(!ok){std::cerr<<"hidden RDRAM mismatch at line "<<line<<'\n';std::exit(1);}}
#define check(...) verify((__VA_ARGS__),__LINE__)
// Independent model: one byte per physical ninth bit, with separate lane
// history. It shares no bitmap addressing/packing with the production module.
struct Model {
    std::array<unsigned,32> bit{};
    std::array<unsigned,8> old{};
    int pending=-1;
    unsigned half(unsigned a) const {a=a/2*2;return bit[a]*2+bit[a+1];}
    void half(unsigned a,unsigned v){a=a/2*2;bit[a]=v/2;bit[a+1]=v%2;}
    void ordinary(unsigned a,unsigned bytes,uint64_t v){
        if(bytes==1)bit[a]=a%2?v%2:0;
        else for(unsigned n=0;n<bytes;n+=2)half(a+n,unsigned(v/(uint64_t(1)<<(8*(bytes-n-2))))%2*3);
    }
    void scatter(unsigned a,unsigned bytes,uint64_t v){
        for(unsigned n=0;n<bytes;++n){
            const unsigned offset=(a+n)%4;
            if(bytes==1)bit[a+n]=offset==3?v%2:0;
            else if(bytes==2)bit[a+n]=a%4==2?(v/(n==0?2:1))%2:0;
            else {const uint64_t word=bytes==8 && n<4?v/4294967296ull:v%4294967296ull;bit[a+n]=(word/(1u<<(3-offset)))%2;}
        }
    }
    uint64_t gather(unsigned a,unsigned bytes) const {
        uint64_t result=0;
        for(unsigned n=0;n<bytes;++n){const unsigned base=(a+n)/4*4;unsigned nibble=0;for(unsigned k=0;k<4;++k)nibble=nibble*2+bit[base+k];result=result*256+((a+n)%4==3?nibble:0);}
        return result;
    }
    void finish(){if(pending>=0)bit[unsigned(pending)/2*2]=old[unsigned(pending)/2%8]/2;pending=-1;}
    void rdp(unsigned a,unsigned value,bool flip){
        if(flip && pending>=0 && unsigned(pending)<a)finish();
        if(a%2){
            bit[a]=value%2;
            if(flip && pending>=0)bit[a-1]=value%2;
            old[a/2%8]=value%2*3;
            if(flip)pending=-1;
        } else if(flip)pending=int(a+1);
        else bit[a]=old[a/2%8]/2;
    }
};
static void compare(const n64::HiddenRdram& actual,const Model& model){
    for(unsigned a=0;a<32;++a)check(actual.halfword(a)==model.half(a));
    const auto state=actual.snapshot();uint64_t history=0;
    for(unsigned i=0;i<8;++i)history+=uint64_t(model.old[i])*(1u<<(2*i));
    check(state.history==history && state.delayed==model.pending);
    for(unsigned bytes:{1u,2u,4u,8u})for(unsigned a=0;a<32;a+=bytes){uint64_t value=~0ull;check(actual.ebus_read(a,bytes,value) && value==model.gather(a,bytes));}
}
static void initialize(n64::HiddenRdram& actual,Model& model,unsigned pattern,unsigned history){
    auto state=actual.snapshot();state.delayed=-1;state.history=0;
    for(unsigned a=0;a<32;a+=2){const unsigned v=(pattern+a/2)%4;model.half(a,v);check(actual.rdp_halfword(a,v,false));}
    state=actual.snapshot();state.history=0;
    for(unsigned n=0;n<8;++n){model.old[n]=(history+n)%4;state.history+=uint64_t(model.old[n])*(1u<<(2*n));}
    check(actual.restore(state));model.pending=-1;
}
int main(){
    uint64_t packing=0,cpu=0,ebus=0,rdp=0,burst=0;
    n64::HiddenRdram full;
    check(full.snapshot().bits.size()==1048576);
    for(size_t a=0;a<full.size();a+=2)for(unsigned value=0;value<4;++value){
        check(full.rdp_halfword(a,value,false) && full.halfword(a)==value && full.halfword(a+1)==value);
        if(a)check(full.halfword(a-2)==3);
        if(a+2<full.size())check(full.halfword(a+2)==0);
        ++packing;
    }
    n64::HiddenRdram actual(32);Model model;
    for(unsigned initial=0;initial<4;++initial)for(unsigned raw=0;raw<65536;++raw)for(unsigned bytes:{1u,2u,4u,8u}){
        const uint64_t value=uint64_t(raw)|(uint64_t(raw/2)<<16)|(uint64_t(raw/4)<<32)|(uint64_t(raw/8)<<48);
        const unsigned address=(raw/16% (32/bytes))*bytes;
        initialize(actual,model,initial,raw%4);
        check(actual.ordinary_write(address,bytes,value));model.ordinary(address,bytes,value);compare(actual,model);++cpu;
    }
    for(unsigned initial=0;initial<16;++initial)for(unsigned raw=0;raw<256;++raw)for(unsigned bytes:{1u,2u,4u,8u})for(unsigned address=0;address<32;address+=bytes){
        initialize(actual,model,initial/4,initial%4);
        const uint64_t value=(uint64_t(raw)<<32)|(raw^255);
        check(actual.ebus_write(address,bytes,value));model.scatter(address,bytes,value);compare(actual,model);++ebus;
    }
    for(unsigned initial=0;initial<4;++initial)for(unsigned history=0;history<4;++history)for(unsigned raw=0;raw<256;++raw)for(unsigned lane=0;lane<8;++lane)for(bool flip:{false,true}){
        initialize(actual,model,initial,history);
        // Includes pairs, ascending and descending traversal, delayed writes
        // across gaps, repeated addresses, and wrap of the eight history lanes.
        for(unsigned a:{lane*2,lane*2+1,24u,25u,8u,7u,6u,31u,30u,30u,0u,17u}){
            check(actual.rdp_byte(a,uint8_t(raw),flip));model.rdp(a,raw,flip);compare(actual,model);++rdp;
        }
        actual.finish_rdp_bytes();model.finish();compare(actual,model);
    }
    for(unsigned lane=0;lane<8;++lane)for(unsigned first=0;first<4;++first)for(unsigned second=0;second<4;++second){
        initialize(actual,model,1,2);const unsigned a=lane*4;
        check(actual.rdp_word(a,first,second));model.half(a,first);model.half(a+2,second);model.old[a/2%8]=first;model.old[(a/2+1)%8]=second;compare(actual,model);
        check(actual.rdp_halfword(a,second,false));model.half(a,second);compare(actual,model);
        check(actual.rdp_halfword(a,first,true));model.half(a,first);model.old[a/2%8]=first;compare(actual,model);rdp+=3;
    }
    std::array<uint8_t,32> data{};
    for(unsigned pattern=0;pattern<256;++pattern)for(unsigned a=0;a<=32;a+=2)for(unsigned bytes=0;bytes<=32-a;bytes+=2){
        initialize(actual,model,2,3);for(unsigned n=0;n<32;++n)data[n]=uint8_t(pattern+n*73);
        check(actual.ordinary_burst(a,data.data(),bytes));for(unsigned n=0;n<bytes;n+=2)model.half(a+n,data[n+1]%2*3);compare(actual,model);++burst;
    }
    const auto before=actual.snapshot();
    for(unsigned bytes:{0u,1u,2u,3u,4u,8u,16u})for(size_t a:{size_t(1),size_t(31),size_t(32),std::numeric_limits<size_t>::max()}){
        if((bytes==1 || bytes==2 || bytes==4 || bytes==8) && a<32 && !(a&(bytes-1)) && bytes<=32-a)continue;
        uint64_t value=0xdeadbeef;check(!actual.ordinary_write(a,bytes,~0ull) && !actual.ebus_write(a,bytes,~0ull) && !actual.ebus_read(a,bytes,value) && value==0xdeadbeef && actual.snapshot()==before);
    }
    for(unsigned fault=0;fault<6;++fault){auto bad=before;
        if(fault==0)bad.bits.pop_back();
        if(fault==1)bad.bits.push_back(0);
        if(fault==2)bad.history=65536;
        if(fault==3)bad.delayed=-2;
        if(fault==4)bad.delayed=2;
        if(fault==5)bad.delayed=33;
        check(!actual.restore(bad) && actual.snapshot()==before);
    }
    check(!actual.rdp_halfword(1,0,true) && !actual.rdp_halfword(0,4,true) && !actual.rdp_word(0,4,0) && !actual.rdp_word(0,0,4) && !actual.rdp_byte(32,0,true) && actual.snapshot()==before);
    check(!actual.ordinary_burst(1,data.data(),2) && !actual.ordinary_burst(0,data.data(),3) && !actual.ordinary_burst(32,data.data(),2) && !actual.ordinary_burst(0,nullptr,2) && actual.snapshot()==before);
    actual.rdp_byte(4,0,true);const auto checkpoint=actual.snapshot();actual.rdp_byte(5,1,true);const auto after=actual.snapshot();check(actual.restore(checkpoint));actual.rdp_byte(5,1,true);check(actual.snapshot()==after);
    actual.reset();check(actual.snapshot().history==0 && actual.snapshot().delayed==-1);for(unsigned a=0;a<32;++a)check(actual.halfword(a)==0);
    bool rejected=false;try{n64::HiddenRdram bad(7);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
    std::cout<<packing<<" packed halfwords; "<<cpu<<" CPU stores; "<<ebus<<" EBUS transactions; "<<rdp<<" RDP history writes; "<<burst<<" settled bursts; atomic rejection/replay PASS\n";
}
