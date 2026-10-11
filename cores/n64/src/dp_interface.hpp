#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <algorithm>
#include "rdp_combiner.hpp"
#include "rdp_fill.hpp"
#include "rdp_texture.hpp"
#include "rdp_convert.hpp"
#include "rdp_tmem.hpp"
#include "rdp_packet.hpp"
#include "rdp_triangle_fill.hpp"
#include "rdp_triangle_shade.hpp"
namespace n64 {
// Shared CPU/RSP DP interface and bounded, functional RDP fill engine.
// Command issue/counters are nominal clocks, not DMA FIFO or rasterizer timing.
class DpInterface {
public:
    uint32_t start=0,end=0,current=0,active_end=0,flags=0;
    bool start_valid=false,end_valid=false,pipeline=true,blocked=false;
    uint32_t clock=0,command_busy=0,pipe_busy=0,tmem_busy=0,blocked_reason=0,fill_color=0;
    uint64_t blocked_word=0,color_image=0,other_mode=0,scissor=0;
    RdpCombiner combiner;
    RdpTexture texture;
    RdpConvert conversion;
    RdpBlender blender;
    RdpTmem tmem;
    RdpPacket packet;
    uint32_t depth_image=0;
    using State=std::array<uint64_t,19>;
    void reset(){*this=DpInterface{};}
    uint32_t status() const{return flags|128u|(pipeline?40u:0u)|(current!=active_end?64u:0u)|(start_valid?1024u:0u)|(end_valid?512u:0u);}
    uint32_t read(unsigned reg) const{
        switch(reg){case 0:return start;case 1:return end;case 2:return current;case 3:return status();case 4:return clock;case 5:return command_busy;case 6:return pipe_busy;case 7:return tmem_busy;default:return 0;}
    }
    bool write(unsigned reg,uint32_t value){
        if(reg>=8)return false;
        if(reg==0){if(!start_valid){start=value&0xfffff8;start_valid=true;}return true;}
        if(reg==1){end=value&0xfffff8;end_valid=true;
            if(start_valid){if(current==active_end)activate();}
            else{active_end=end;end_valid=false;pipeline=true;}
            return true;}
        if(reg==3){
            for(unsigned n=0;n<3;++n){const unsigned pair=(value>>(2*n))&3;if(pair==1)flags&=~(1u<<n);else if(pair==2)flags|=1u<<n;}
            if(value&64)tmem_busy=0;
            if(value&128)pipe_busy=0;
            if(value&256)command_busy=0;
            if(value&512)clock=0;
        }
        // Read-only writes ignored as a deterministic convention; physical bus behavior ungraded.
        return true;
    }
    void tick(std::vector<uint8_t>& ram,const std::array<uint8_t,8192>& sp,bool& interrupt,HiddenRdram* hidden=nullptr){
        if(pipeline){clock=(clock+1)&0xffffff;pipe_busy=(pipe_busy+1)&0xffffff;}
        if(current==active_end && start_valid && end_valid)activate();
        if((flags&6) || blocked)return;
        if(!packet.ready()){
            if(current==active_end)return;
            command_busy=(command_busy+1)&0xffffff;
            uint64_t fetched=0;
            for(unsigned n=0;n<8;++n){const uint32_t address=(current+n)&0xffffff;
                if(flags&1)fetched=(fetched<<8)|sp[address&4095];
                else{if(address>=ram.size()){block(0,2);return;}fetched=(fetched<<8)|ram[address];}}
            packet.push(current,fetched);current=(current+8)&0xfffff8;
            if(!packet.ready())return;
        }
        const uint64_t word=packet.words[0];
        const unsigned op=unsigned(word>>56)&63;
        bool supported=true;unsigned reason=1;
        switch(op){
        case 8:case 9:case 10:case 11:case 12:case 13:case 14:case 15:
            if(((other_mode>>52)&3)==3){reason=RdpTriangleFill::draw(packet,color_image,other_mode,scissor,fill_color,ram,hidden);supported=!reason;}
            else if(((other_mode>>52)&3)==0){reason=RdpTriangleShade::draw(packet,color_image,other_mode,scissor,combiner,ram,&texture,&tmem,&conversion,&blender,hidden);supported=!reason;}
            else supported=false;
            break;
        case 0:case 38:case 39:case 40:break; // NOP and settled load/pipe/tile barriers.
        case 41:pipeline=false;interrupt=true;break; // Actual FullSync command, never a range-end IRQ.
        case 44:conversion.command(word);break;
        case 45:scissor=word&0x00ffffffffffffffull;break;
        case 47:other_mode=word&0x00ffffffffffffffull;break;
        case 56:case 57:blender.command(op,word);break;
        case 55:fill_color=uint32_t(word);break;
        case 48:case 51:case 52:{uint32_t groups;reason=tmem.load(op,word,texture,ram,groups);supported=!reason;if(supported)tmem_busy=(tmem_busy+groups)&0xffffff;break;}
        case 50:case 53:case 61:texture.command(op,word);break;
        case 58:case 59:case 60:combiner.command(op,word);break;
        case 62:depth_image=uint32_t(word)&0x3ffffff;break;
        case 63:color_image=word&0x00ffffffffffffffull;break;
        case 54:reason=RdpFill::draw(word,color_image,other_mode,scissor,fill_color,ram,hidden);supported=!reason;break;
        default:supported=false;break;
        }
        if(!supported){block(word,reason);return;}
        if(op!=41)pipeline=true;
        blocked=false;blocked_reason=0;blocked_word=0;packet.clear();
    }
    State snapshot() const{return {start,end,current,active_end,flags,uint64_t(start_valid),uint64_t(end_valid),uint64_t(pipeline),clock,command_busy,pipe_busy,tmem_busy,uint64_t(blocked),blocked_word,color_image,other_mode,scissor,fill_color,blocked_reason};}
    bool restore(const State& s){
        for(unsigned n=0;n<4;++n)if(s[n]&~0xfffff8ull)return false;
        if(s[4]>7 || s[5]>1 || s[6]>1 || s[7]>1 || s[12]>1 || s[17]>0xffffffffu || s[18]>5)return false;
        for(unsigned n=8;n<12;++n)if(s[n]>0xffffff)return false;
        for(unsigned n=14;n<17;++n)if(s[n]>>56)return false;
        if(bool(s[12])!=bool(s[18]) || (!s[12] && s[13]))return false;
        start=uint32_t(s[0]);end=uint32_t(s[1]);current=uint32_t(s[2]);active_end=uint32_t(s[3]);flags=uint32_t(s[4]);start_valid=bool(s[5]);end_valid=bool(s[6]);pipeline=bool(s[7]);clock=uint32_t(s[8]);command_busy=uint32_t(s[9]);pipe_busy=uint32_t(s[10]);tmem_busy=uint32_t(s[11]);blocked=bool(s[12]);blocked_word=s[13];color_image=s[14];other_mode=s[15];scissor=s[16];fill_color=uint32_t(s[17]);blocked_reason=uint32_t(s[18]);return true;
    }
private:
    void activate(){if(blocked)packet.clear();current=start;active_end=end;start_valid=end_valid=false;pipeline=true;blocked=false;blocked_reason=0;blocked_word=0;}
    void block(uint64_t word,unsigned reason){blocked=true;blocked_word=word;blocked_reason=reason;}
};
}
