#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace n64 {
// One ninth bit per physical RDRAM byte. Four two-bit halfword entries fit in
// one byte. This component receives physical, already translated addresses;
// it does not own ordinary RAM, bus timing, or the MI test-mode enable latch.
class HiddenRdram {
public:
    struct State {
        std::vector<uint8_t> bits;
        uint64_t history=0;
        int64_t delayed=-1;
        bool operator==(const State& other) const {
            return bits==other.bits && history==other.history && delayed==other.delayed;
        }
    };
    explicit HiddenRdram(size_t bytes=8*1024*1024) : bytes_(bytes),bits_(bytes/8) {
        if(!bytes || bytes%8)throw std::invalid_argument("hidden RDRAM size must be a positive multiple of eight");
    }
    size_t size() const {return bytes_;}
    void reset() {std::fill(bits_.begin(),bits_.end(),0);history_=0;delayed_=-1;}
    // Address may identify either byte of the halfword. Bounds are checked by
    // the transaction methods; direct access is useful for paired RDP reads.
    unsigned halfword(size_t address) const {
        if(address>=bytes_)return 0;
        return (bits_[address/8]>>((address/2%4)*2))&3;
    }
    bool ordinary_write(size_t address,unsigned bytes,uint64_t value) {
        if(!valid(address,bytes))return false;
        if(bytes==1)bit(address,(address&1)?unsigned(value&1):0);
        else for(unsigned n=0;n<bytes;n+=2)set(address+n,((value>>(8*(bytes-n-2)))&1)*3);
        return true;
    }
    // Settled aligned DMA/cache bursts reproduce the LSB on both hidden bits
    // of each transferred halfword. The caller retains responsibility for RAM.
    bool ordinary_burst(size_t address,const uint8_t* data,size_t bytes) {
        if(!data || (address&1) || (bytes&1) || address>bytes_ || bytes>bytes_-address)return false;
        for(size_t n=0;n<bytes;n+=2)set(address+n,(data[n+1]&1)*3);
        return true;
    }
    bool ebus_write(size_t address,unsigned bytes,uint64_t value) {
        if(!valid(address,bytes))return false;
        if(bytes==1)bit(address,(address%4==3)?unsigned(value&1):0);
        else if(bytes==2) {
            bit(address,(address&2)?unsigned(value>>1)&1:0);
            bit(address+1,(address&2)?unsigned(value)&1:0);
        } else for(unsigned n=0;n<bytes;++n) {
            const unsigned shift=(bytes==8 && n<4?32:0)+3-n%4;
            bit(address+n,unsigned(value>>shift)&1);
        }
        return true;
    }
    bool ebus_read(size_t address,unsigned bytes,uint64_t& value) const {
        if(!valid(address,bytes))return false;
        const unsigned first=nibble(address&~size_t(3));
        value=bytes==1?(first>>(24-8*(address%4)))&255:
              bytes==2?(first>>(16-8*(address&2)))&65535:
              bytes==4?first:(uint64_t(first)<<32)|nibble(address+4);
        return true;
    }
    // Explicit coverage/depth metadata; depth stores do not update the color
    // write history. These methods change metadata only, never framebuffer RAM.
    bool rdp_halfword(size_t address,unsigned hidden,bool color) {
        if(!valid(address,2) || hidden>3)return false;
        set(address,hidden);if(color)remember(address,hidden);return true;
    }
    bool rdp_word(size_t address,unsigned first,unsigned second) {
        if(!valid(address,4) || first>3 || second>3)return false;
        set(address,first);set(address+2,second);remember(address,first);remember(address+2,second);return true;
    }
    // Rejected RGBA pixels still replace retained color-write history, without
    // replacing RAM or its hidden bits. Subsequent CI8 writes consume it.
    bool rejected_halfword(size_t address,unsigned value) {
        if(!valid(address,2) || value>3)return false;
        remember(address,value);return true;
    }
    bool rejected_word(size_t address,unsigned first,unsigned second) {
        if(!valid(address,4) || first>3 || second>3)return false;
        remember(address,first);remember(address+2,second);return true;
    }
    bool rdp_byte(size_t address,uint8_t value,bool flip) {
        if(!valid(address,1))return false;
        if(flip && delayed_>=0 && size_t(delayed_)<address)finish_rdp_bytes();
        if(address&1) {
            const unsigned low=value&1;
            set(address,(flip && delayed_>=0)?low*3:(halfword(address)&2)|low);
            remember(address,low*3);if(flip)delayed_=-1;
        } else if(flip)delayed_=int64_t(address+1);
        else set(address,(halfword(address)&1)|(remembered(address)&2));
        return true;
    }
    // Eight-bit raster commands clear the high retained bit in lanes zero and four.
    void reset_byte_raster_history() {history_&=uint16_t(~514u);}
    // Command-local delayed writes are discarded where the native copy path does not flush.
    void discard_rdp_bytes() {delayed_=-1;}
    void finish_rdp_bytes() {
        if(delayed_>=0){const auto address=size_t(delayed_);set(address,(halfword(address)&1)|(remembered(address)&2));}
        delayed_=-1;
    }
    State snapshot() const {return {bits_,history_,delayed_};}
    bool restore(const State& state) {
        if(state.bits.size()!=bits_.size() || state.history>65535 || state.delayed< -1 ||
           (state.delayed>=0 && (!(state.delayed&1) || uint64_t(state.delayed)>=bytes_)))return false;
        bits_=state.bits;history_=uint16_t(state.history);delayed_=state.delayed;return true;
    }
private:
    size_t bytes_;
    std::vector<uint8_t> bits_;
    uint16_t history_=0;
    int64_t delayed_=-1;
    bool valid(size_t address,unsigned bytes) const {
        return (bytes==1 || bytes==2 || bytes==4 || bytes==8) && !(address&(bytes-1)) &&
               address<=bytes_ && bytes<=bytes_-address;
    }
    void set(size_t address,unsigned value) {
        const auto shift=unsigned(address/2%4)*2;
        bits_[address/8]=uint8_t((bits_[address/8]&~(3u<<shift))|(value<<shift));
    }
    void bit(size_t address,unsigned value) {
        const unsigned shift=1-unsigned(address&1);
        set(address,(halfword(address)&~(1u<<shift))|(value<<shift));
    }
    unsigned nibble(size_t address) const {return (halfword(address)<<2)|halfword(address+2);}
    unsigned remembered(size_t address) const {return (history_>>((address/2%8)*2))&3;}
    void remember(size_t address,unsigned value) {
        const auto shift=unsigned(address/2%8)*2;
        history_=uint16_t((history_&~(3u<<shift))|(value<<shift));
    }
};
}
