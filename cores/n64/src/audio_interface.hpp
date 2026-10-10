#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace n64 {
class AudioInterface {
public:
    struct Transfer {uint32_t address=0,remaining=0;};
    using State=std::array<uint64_t,15>;
    uint32_t address=0,control=0,dacrate=0,bitrate=0,count=0,interrupt=0,phase=0,carry=0;
    std::array<Transfer,2> fifo{};
    uint16_t left=0,right=0;bool valid=false;
    void reset() {*this=AudioInterface{};}
    uint32_t read(unsigned reg) const {
        if(reg==3)return 0x01100000u|(control<<25)|(count?0x40000000u:0)|(count==2?0x80000001u:0);
        return count?(fifo[0].remaining&0x3fff8u):0; // Write-only mirrors LENGTH.
    }
    bool write(unsigned reg,uint32_t value,size_t ram_size) {
        if(reg==0) {address=value&0xfffff8;return true;}
        if(reg==2) {control=value&1;return true;}
        if(reg==3) {interrupt=0;return true;}
        if(reg==4) {dacrate=value&16383;phase%=dacrate+1;return true;}
        if(reg==5) {bitrate=value&15;return true;}
        if(reg!=1)return false;
        const uint32_t length=value&0x3fff8;
        if(!length || count==2)return true;
        // Include a carry that will be applied when this transfer becomes active.
        const bool offset=count?((fifo[0].address+fifo[0].remaining)%8192==0):bool(carry);
        const uint64_t effective=uint64_t(address)+(offset?8192:0);
        if(effective+length>ram_size)return false;
        fifo[count++]={address,length};
        if(count==1)activate();
        return true;
    }
    // One VI-crystal tick; no CPU or host clock is hidden in this module.
    void tick(const std::vector<uint8_t>& ram) {
        if(!bitrate)return;
        if(++phase<=dacrate)return;
        phase=0;valid=false;
        if(!control || !count) {left=right=0;return;}
        auto& t=fifo[0];const uint32_t p=t.address;
        left=uint16_t(unsigned(ram[p])*256+ram[p+1]);right=uint16_t(unsigned(ram[p+2])*256+ram[p+3]);valid=true;
        t.address+=4;t.remaining-=4;
        if(!t.remaining) {
            carry=(t.address%8192)==0;
            fifo[0]=fifo[1];fifo[1]={};--count;
            if(count)activate();
        }
    }
    State snapshot() const {return {address,control,dacrate,bitrate,count,interrupt,phase,carry,fifo[0].address,fifo[0].remaining,fifo[1].address,fifo[1].remaining,left,right,uint64_t(valid)};}
    bool restore(const State& s,size_t ram_size) {
        constexpr uint64_t masks[]={0xfffff8,1,16383,15,3,1,16383,1,0xffffff,0x3fffc,0xfffff8,0x3fff8,65535,65535,1};
        for(unsigned n=0;n<s.size();++n)if(s[n]&~masks[n])return false;
        if(s[9]>0x3fff8 || s[11]>0x3fff8 || s[4]>2 || s[6]>s[2] || (s[8]&3))return false;
        for(unsigned n=0;n<2;++n) {
            const auto p=s[8+n*2],remaining=s[9+n*2];
            if(n>=s[4]) {if(p || remaining)return false;}
            else if(!remaining || p+remaining>ram_size)return false;
        }
        // An active transfer has already incorporated any delayed carry.
        if(s[4] && s[7])return false;
        if(s[4]==2 && s[10]+s[11]+(((s[8]+s[9])%8192==0)?8192:0)>ram_size)return false;
        address=uint32_t(s[0]);control=uint32_t(s[1]);dacrate=uint32_t(s[2]);bitrate=uint32_t(s[3]);count=uint32_t(s[4]);interrupt=uint32_t(s[5]);phase=uint32_t(s[6]);carry=uint32_t(s[7]);
        fifo={Transfer{uint32_t(s[8]),uint32_t(s[9])},Transfer{uint32_t(s[10]),uint32_t(s[11])}};
        left=uint16_t(s[12]);right=uint16_t(s[13]);valid=s[14];return true;
    }
private:
    void activate() {fifo[0].address+=carry?8192:0;carry=0;interrupt=1;}
};
}
