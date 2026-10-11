#include "machine.hpp"
#include <algorithm>
#include <cstring>
#include "emu/input_types.hpp"

namespace n64 {
static uint32_t crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}
static uint64_t be(const uint8_t* data, unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value = (value << 8) | data[i];
    return value;
}
bool Machine::load(const uint8_t* data, size_t size) {
    if (!data || size < 0x1004 || size > 64*1024*1024 || (size & 3)) return false;
    std::vector<uint8_t> image(data, data + size);
    const uint32_t magic = uint32_t(be(data, 4));
    if (magic == 0x37804012) {
        for (size_t i = 0; i < size; i += 2) std::swap(image[i], image[i + 1]);
    } else if (magic == 0x40123780) {
        for (size_t i = 0; i < size; i += 4) { std::swap(image[i], image[i + 3]); std::swap(image[i + 1], image[i + 2]); }
    } else if (magic != 0x80371240) return false;
    const uint32_t start = uint32_t(be(image.data() + 8, 4));
    BootMode mode=BootMode::Homebrew;
    uint32_t target = 0;
    if(!std::memcmp(image.data()+0x34,"V64HB001",8)) {
        if(size>ram_size-0x400+0x1000 || !physical(sign32(start),target) || (start&3) || target<0x400 || target>=0x400+size-0x1000) return false;
    } else {
        if(start<0x80000000u || start>=0x80800000u || (start&3)) return false;
        if(!std::memcmp(image.data()+0x34,"V64IP001",8)) mode=BootMode::OriginalIPL;
        else if((image[0x3e]=='E' || image[0x3e]=='J') && crc32(image.data()+0x40,0xfc0)==0x90bb6cb5u) mode=BootMode::Native6102;
        else return false; // Unknown native boot variants are not guessed.
    }
    crc = crc32(data, size);
    identity = 14695981039346656037ull;
    for (auto byte : image) { identity ^= byte; identity *= 1099511628211ull; }
    entry = sign32(start);
    boot_mode=mode;
    rom = std::move(image);
    reset();
    return true;
}
void Machine::reset() {
    std::fill(ram.begin(), ram.end(), 0);
    hidden.reset();
    sp.fill(0);
    if (!rom.empty() && boot_mode==BootMode::Homebrew) std::copy(rom.begin() + 0x1000, rom.end(), ram.begin() + 0x400);
    if (!rom.empty() && boot_mode!=BootMode::Homebrew) std::copy(rom.begin()+0x40,rom.begin()+0x1000,sp.begin()+0x40);
    hidden.ordinary_burst(0,ram.data(),ram.size());
    cpu.reset(rom.empty() || boot_mode==BootMode::Homebrew?entry:sign32(0xa4000040));
    if(!rom.empty() && boot_mode!=BootMode::Homebrew) {
        // Explicit HLE handoff to IPL3, not a model of the PIF ROM/CIC exchange.
        auto& s=cpu.state;s.cp0[12]=0x34000000;s.cp0[16]=3;
        s.gpr[19]=0;s.gpr[20]=1;s.gpr[21]=0;s.gpr[22]=0x3f;s.gpr[23]=1;
        s.gpr[29]=sign32(0xa4001ff0);s.gpr[31]=sign32(0xa4001550);
    }
    frames = 0; done = false; yielded = false;
    pif.fill(0);other_input_buttons.fill(0); si_address = input_buttons = 0; si_interrupt = false; mi_mask = 0;mi_mode=0;dp_interrupt=false;
    pi={};pi_interrupt=false;ri={};ri_current_loaded=false;rdram_devices.reset();sp_interface.reset();rsp.reset();dp_interface.reset();rsp_phase=0;audio_interface.reset();video_interface.reset();audio_samples={};crystal_phase=output_phase=audio_head=audio_count=audio_dropped=0;
    if(!rom.empty() && boot_mode!=BootMode::Homebrew) {
        const auto config=uint32_t(be(rom.data(),4));
        pi[5]=config&255;pi[6]=(config>>8)&255;pi[7]=(config>>16)&15;pi[8]=(config>>20)&3;
    }
}
void Machine::clock() {
    // One nominal CPU clock per interpreter attempt: physical CPI is unmodeled.
    rsp_phase+=2;if(rsp_phase>=3){rsp_phase-=3;rsp.step(sp_interface,dp_interface,sp,ram);dp_interface.tick(ram,sp,dp_interrupt,&hidden);}
    crystal_phase+=48681818;
    if(crystal_phase>=93750000) {crystal_phase-=93750000;audio_interface.tick(ram);video_interface.tick();}
    output_phase+=48000;
    if(output_phase>=93750000) {
        output_phase-=93750000;
        if(!audio_interface.control || !audio_interface.bitrate || !audio_interface.valid)return;
        if(audio_count==audio_capacity) {audio_head=(audio_head+1)%audio_capacity;--audio_count;if(audio_dropped!=~uint64_t(0))++audio_dropped;}
        const size_t at=((audio_head+audio_count)%audio_capacity)*2;
        audio_samples[at]=audio_interface.left;audio_samples[at+1]=audio_interface.right;++audio_count;
    }
}
void Machine::process_pif() {
    if (!(pif[63] & 1)) return;
    unsigned channel = 0, pos = 0;
    while (pos < 63 && channel < 5) {
        const unsigned marker = pif[pos];
        if (marker == 0xfe) break;
        if (marker == 0xff) { ++pos; continue; }
        if (marker == 0 || marker == 0xfd) { ++pos; ++channel; continue; }
        if (pos + 2 >= 63) break;
        const unsigned tx = marker & 0x3f, rx = pif[pos + 1] & 0x3f;
        if (!tx || pos + 2 + tx + rx > 63) { pif[pos + 1] = uint8_t(rx | 0x40); break; }
        const unsigned command = pif[pos + 2], response = pos + 2 + tx;
        pif[pos + 1] = uint8_t(rx);
        if (channel >= 4 || (command != 0 && command != 0xff && command != 1)) {
            pif[pos + 1] |= 0x80;
        } else if (tx != 1 || rx != (command == 1 ? 4u : 3u)) {
            pif[pos + 1] |= 0x40;
        } else if (command != 1) {
            pif[response] = 5; pif[response + 1] = 0; pif[response + 2] = 2;
        } else {
            using emu::VirtualButton;
            static const VirtualButton controls[] = {VirtualButton::A, VirtualButton::B, VirtualButton::Z,
                VirtualButton::Start, VirtualButton::Up, VirtualButton::Down, VirtualButton::Left,
                VirtualButton::Right, VirtualButton::L, VirtualButton::R, VirtualButton::CUp,
                VirtualButton::CDown, VirtualButton::CLeft, VirtualButton::CRight};
            static const uint16_t masks[] = {0x8000,0x4000,0x2000,0x1000,0x0800,0x0400,0x0200,
                0x0100,0x0020,0x0010,0x0008,0x0004,0x0002,0x0001};
            uint16_t buttons = 0;
            for (unsigned i = 0; i < 14; ++i)
                if (emu::is_button_pressed(channel?other_input_buttons[channel-1]:input_buttons, controls[i])) buttons |= masks[i];
            pif[response] = uint8_t(buttons >> 8); pif[response + 1] = uint8_t(buttons);
            pif[response + 2] = pif[response + 3] = 0; // Analog input not implemented.
        }
        pos += 2 + tx + rx; ++channel;
    }
    pif[63] &= ~1u;
}
bool Machine::physical(uint64_t address, uint32_t& result) {
    // Only canonical sign-extended 32-bit kernel unmapped segments are implemented.
    const uint32_t low = uint32_t(address);
    if (address != sign32(low) || low < 0x80000000u || low >= 0xc0000000u) return false;
    result = low & 0x1fffffffu;
    return true;
}
bool Machine::read(uint64_t address, unsigned bytes, uint64_t& value) {
    uint32_t p = 0;
    if (!physical(address, p) || (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8)) return false;
    if(p>=0x03f00000 && p<=0x03ffffff) {
        if(bytes!=4 || (p&3))return false;
        value=(uint32_t(address)>=0xa0000000u && !(mi_mode&0x200) && (p&4))?0:
            rdram_devices.read(p,ri_current_loaded && ri[3]==0x14);
        return true;
    }
    if (uint64_t(p) + bytes <= ram.size()) {
        if(uint32_t(address)>=0xa0000000u && (mi_mode&256))return hidden.ebus_read(p,bytes,value);
        value = be(ram.data() + p, bytes); return true;
    }
    if (p >= 0x04000000 && uint64_t(p) + bytes <= 0x04000000 + sp.size()) { value = be(sp.data() + p - 0x04000000, bytes); return true; }
    if (p >= 0x10000000 && uint64_t(p) + bytes <= 0x10000000 + rom.size()) { value = be(rom.data() + p - 0x10000000, bytes); return true; }
    if (p >= 0x1fc007c0 && uint64_t(p) + bytes <= 0x1fc00800) { value = be(pif.data() + p - 0x1fc007c0, bytes); return true; }
    if (bytes == 4) {
        if(p>=0x04100000 && p<=0x0410001c && !(p&3)){value=dp_interface.read((p-0x04100000)/4);return true;}
        if(p>=0x04400000 && p<=0x0440003c && !(p&3))return video_interface.read((p-0x04400000)/4,value);
        if(p>=0x04500000 && p<=0x04500014 && !(p&3)) {value=audio_interface.read((p-0x04500000)/4);return true;}
        if(p>=0x04040000 && p<=0x0404001c && !(p&3))return sp_interface.read((p-0x04040000)/4,value);
        if(p==0x04080000) {value=sp_interface.pc;return true;}
        if(p>=0x04700000 && p<=0x0470001c && !(p&3)) {
            const unsigned reg=(p-0x04700000)/4;
            if(reg==2 || reg==7) return false; // Write-only/undefined reads are not guessed.
            value=reg==6?0:ri[reg];return true; // No bus error producer yet.
        }
        if(p>=0x04600000 && p<=0x04600030 && !(p&3)) {
            const unsigned reg=(p-0x04600000)/4;value=reg==4?(pi_interrupt?8:0):pi[reg];return true;
        }
        if (p == 0x04800000) { value = si_address; return true; }
        if (p == 0x04800018) { value = si_interrupt ? 0x1000 : 0; return true; }
        if (p == 0x04300000) { value=mi_mode;return true; }
        if (p == 0x04300004) { value=0x02020102;return true; }
        if (p == 0x04300008) { value = (video_interface.interrupt?8:0)|(audio_interface.interrupt?4:0)|sp_interface.interrupt|(si_interrupt ? 2 : 0)|(pi_interrupt ? 16 : 0)|(dp_interrupt ? 32 : 0); return true; }
        if (p == 0x0430000c) { value = mi_mask; return true; }
    }
    return false;
}
bool Machine::write(uint64_t address, unsigned bytes, uint64_t value) {
    uint32_t p = 0;
    if (!physical(address, p) || (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8)) return false;
    if(p>=0x03f00000 && p<=0x03ffffff) {
        if(bytes!=4 || (p&3))return false;
        const bool uncached=uint32_t(address)>=0xa0000000u;
        const unsigned repeat=uncached && (mi_mode&128)?(mi_mode&127)+1:0;
        rdram_devices.write(p,uint32_t(value),ri_current_loaded && ri[3]==0x14,repeat);
        if(repeat)mi_mode&=~128u;
        return true;
    }
    if (p == 0x1ff00000 && bytes == 1) {
        const char c = char(value);
        if (sink) sink->write(&c, 1);
        return true;
    }
    if (p == 0x1ff00004 && bytes == 4) { done = true; if (sink) sink->finish(-1); return true; }
    if (p == 0x1ff0000c && bytes == 4) { yielded = true; return true; }
    if (p >= 0x1fc007c0 && uint64_t(p) + bytes <= 0x1fc00800) {
        for (unsigned i = 0; i < bytes; ++i) pif[p - 0x1fc007c0 + i] = uint8_t(value >> (8 * (bytes - 1 - i)));
        if (p + bytes == 0x1fc00800) process_pif();
        return true;
    }
    if (bytes == 4) {
        if(p>=0x04100000 && p<=0x0410001c && !(p&3))return dp_interface.write((p-0x04100000)/4,uint32_t(value));
        if(p>=0x04400000 && p<=0x0440003c && !(p&3))return video_interface.write((p-0x04400000)/4,uint32_t(value));
        if(p>=0x04500000 && p<=0x04500014 && !(p&3))return audio_interface.write((p-0x04500000)/4,uint32_t(value),ram.size());
        if(p>=0x04040000 && p<=0x0404001c && !(p&3))return sp_interface.write((p-0x04040000)/4,uint32_t(value),ram,sp,&hidden);
        if(p==0x04080000) {sp_interface.pc=uint32_t(value)&0xffc;rsp.redirect(sp_interface.pc);return true;}
        if(p>=0x04700000 && p<=0x0470001c && !(p&3)) return write_ri((p-0x04700000)/4,uint32_t(value));
        if(p>=0x04600000 && p<=0x04600030 && !(p&3)) return write_pi((p-0x04600000)/4,uint32_t(value));
        if (p == 0x04800000) { si_address = uint32_t(value) & 0xffffff; return true; }
        if (p == 0x04800018) { si_interrupt = false; return true; }
        if (p == 0x04300000) {
            mi_mode=(mi_mode&0x380)|(uint32_t(value)&127);
            constexpr unsigned clear[]={7,9,12},set[]={8,10,13};
            for(unsigned n=0;n<3;++n) {
                if(value&(1u<<clear[n])) mi_mode&=~(128u<<n);
                if(value&(1u<<set[n])) mi_mode|=128u<<n;
            }
            if(value&0x800) dp_interrupt=false;
            return true;
        }
        if(p==0x04300004 || p==0x04300008) return true; // Read-only writes are ignored.
        if (p == 0x0430000c) {
            for (unsigned i = 0; i < 6; ++i) {
                if (value & (1u << (2*i))) mi_mask &= ~(1u << i);
                if (value & (2u << (2*i))) mi_mask |= 1u << i;
            }
            return true;
        }
        if (p == 0x04800004 || p == 0x04800010) {
            // Functional settled DMA only: no busy state or asynchronous timing.
            if ((si_address & 7) || uint64_t(si_address) + 64 > ram.size()) return false;
            if (p == 0x04800010) { std::copy_n(ram.begin() + si_address, 64, pif.begin()); process_pif(); }
            else {std::copy(pif.begin(), pif.end(), ram.begin() + si_address);hidden.ordinary_burst(si_address,ram.data()+si_address,64);}
            si_interrupt = true; return true;
        }
    }
    uint8_t* destination = nullptr;
    if (uint64_t(p) + bytes <= ram.size()) destination = ram.data() + p;
    else if (p >= 0x04000000 && uint64_t(p) + bytes <= 0x04000000 + sp.size()) destination = sp.data() + p - 0x04000000;
    if (!destination) return false;
    if(uint64_t(p)+bytes<=ram.size()) {
        const bool test=uint32_t(address)>=0xa0000000u && (mi_mode&256);
        if(!(test?hidden.ebus_write(p,bytes,value):hidden.ordinary_write(p,bytes,value)))return false;
    }
    for (unsigned i = 0; i < bytes; ++i) destination[i] = uint8_t(value >> (8 * (bytes - 1 - i)));
    return true;
}
bool Machine::write_ri(unsigned reg,uint32_t value) {
    if(reg==6) return false; // Read-only error indication.
    if(reg==7) return true; // Any write clears errors; no producer exists yet.
    if(reg==2) {ri[2]=ri[1];ri_current_loaded=true;return true;}
    constexpr uint32_t masks[]={15,127,127,0x77,0x7ffff,15};
    ri[reg]=value&masks[reg];return true;
}
bool Machine::write_pi(unsigned reg,uint32_t value) {
    if(reg==0) {pi[0]=value&0xfffffe;return true;}
    if(reg==1) {pi[1]=value&~1u;return true;}
    if(reg==2) return false; // Save-media/peripheral write DMA is pending.
    if(reg==3) {
        const uint32_t length=(value&0xffffff)+1;
        const uint64_t source=pi[1]>=0x10000000u?uint64_t(pi[1])-0x10000000u:uint64_t(-1);
        // Defined aligned, even-length ROM reads. Reject unsupported transfers
        // before modifying memory, addresses, completion or interrupt state.
        if((pi[0]&7) || (length&1) || uint64_t(pi[0])+length>ram.size() ||
            source>rom.size() || length>rom.size()-source) return false;
        std::copy_n(rom.begin()+size_t(source),length,ram.begin()+pi[0]);
        hidden.ordinary_burst(pi[0],ram.data()+pi[0],length);
        pi[0]=(pi[0]+length+7)&0xfffff8u;pi[1]+=length;pi[3]=127;
        pi_interrupt=true;return true;
    }
    if(reg==4) {if(value&2) pi_interrupt=false;return true;}
    if(reg<=12) {
        constexpr uint32_t masks[]={255,255,15,3,255,255,15,3};
        pi[reg]=value&masks[reg-5];return true;
    }
    return false;
}
bool Machine::write_masked(uint64_t address, unsigned bytes, uint64_t value, uint8_t mask) {
    uint32_t p = 0;
    if ((bytes != 4 && bytes != 8) || (address & (bytes - 1)) || !mask ||
        (unsigned(mask) >> bytes) || !physical(address, p)) return false;
    uint8_t* destination = nullptr;
    if (uint64_t(p) + bytes <= ram.size()) destination = ram.data() + p;
    else if (p >= 0x04000000 && uint64_t(p) + bytes <= 0x04000000 + sp.size()) destination = sp.data() + p - 0x04000000;
    if (!destination) return false;
    if(uint64_t(p)+bytes<=ram.size()) {
        const bool test=uint32_t(address)>=0xa0000000u && (mi_mode&256);
        // Merge instructions issue aligned byte/halfword/word pieces. Select
        // the largest enabled aligned piece; no RAM bus read is needed.
        for(unsigned n=0;n<bytes;) {
            if(!(mask&(1u<<n))){++n;continue;}
            unsigned part=1;
            for(unsigned candidate:{2u,4u,8u})if(n+candidate<=bytes && !((p+n)&(candidate-1)) &&
                ((unsigned(mask)>>n)&((1u<<candidate)-1))==((1u<<candidate)-1))part=candidate;
            const uint64_t v=value>>(8*(bytes-n-part));
            if(test)hidden.ebus_write(p+n,part,v);else hidden.ordinary_write(p+n,part,v);
            n+=part;
        }
    }
    for (unsigned i = 0; i < bytes; ++i)
        if (mask & (1u << i)) destination[i] = uint8_t(value >> (8 * (bytes - 1 - i)));
    return true;
}
static void append(std::vector<uint8_t>& data, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) data.push_back(uint8_t(value >> (56 - 8 * i)));
}
bool Machine::save(std::vector<uint8_t>& data) const {
    if (rom.empty()) return false;
    data.assign({'V','N','6','4','S','0','2','8'});
    data.reserve(current_state_size);
    append(data, identity); append(data, crc); append(data, frames); append(data, done);
    const auto& s = cpu.state;
    for (auto value : s.gpr) append(data, value);
    for (auto value : s.cp0) append(data, value);
    for (auto value : {s.hi, s.lo, s.pc, s.next_pc, s.instructions, uint64_t(s.delay_slot)}) append(data, value);
    append(data, si_address); append(data, input_buttons); append(data, si_interrupt); append(data, mi_mask); append(data, yielded);
    data.insert(data.end(), pif.begin(), pif.end());
    data.insert(data.end(), ram.begin(), ram.end());
    data.insert(data.end(), sp.begin(), sp.end());
    for(auto reg:pi) append(data,reg);
    append(data,pi_interrupt);append(data,uint64_t(boot_mode));
    for(auto reg:ri) append(data,reg);
    append(data,ri_current_loaded);append(data,mi_mode);append(data,dp_interrupt);
    for(const auto& chip:rdram_devices.snapshot())for(auto reg:chip)append(data,reg);
    auto cache=[&](const auto& lines) {for(const auto& line:lines) {append(data,line.page);append(data,line.flags);data.insert(data.end(),line.data.begin(),line.data.end());}};
    cache(cpu.caches.instruction);cache(cpu.caches.data);
    for(auto reg:sp_interface.snapshot())append(data,reg);
    for(auto reg:audio_interface.snapshot())append(data,reg);
    for(auto reg:{crystal_phase,output_phase,audio_head,audio_count,audio_dropped})append(data,reg);
    for(auto sample:audio_samples) {data.push_back(uint8_t(sample>>8));data.push_back(uint8_t(sample));}
    append(data,s.fcr31);
    for(const auto& e:cpu.tlb.entries)for(auto v:{e.hi,uint64_t(e.mask),uint64_t(e.even),uint64_t(e.odd)})append(data,v);
    for(auto value:video_interface.snapshot())append(data,value);
    for(auto value:s.fpr)append(data,value);
    for(auto value:rsp.snapshot())append(data,value);
    append(data,rsp_phase);
    for(auto value:dp_interface.snapshot())append(data,value);
    for(auto value:rsp.vectors.snapshot())append(data,value);
    for(auto value:rsp.vectors.acc_snapshot())append(data,value);
    for(auto value:dp_interface.combiner.snapshot())append(data,value);
    append(data,dp_interface.depth_image);
    append(data,rsp.vectors.divider.snapshot());
    for(auto value:dp_interface.texture.snapshot())append(data,value);
    data.insert(data.end(),dp_interface.tmem.bytes.begin(),dp_interface.tmem.bytes.end());
    for(auto buttons:other_input_buttons)append(data,buttons);
    for(auto value:dp_interface.packet.snapshot())append(data,value);
    append(data,dp_interface.conversion.snapshot());
    for(auto value:dp_interface.blender.snapshot())append(data,value);
    const auto h=hidden.snapshot();data.insert(data.end(),h.bits.begin(),h.bits.end());
    append(data,h.history);append(data,uint64_t(h.delayed));
    append(data,dp_interface.primitive_depth.snapshot());
    return true;
}
bool Machine::restore(const std::vector<uint8_t>& data) {
    constexpr size_t prefix = 8 + (4 + 32 + 32 + 6 + 5) * 8 + 64;
    if(rom.empty() || data.size()<8) return false;
    const bool legacy=!std::memcmp(data.data(),"VN64S002",8);
    const bool previous=!std::memcmp(data.data(),"VN64S003",8);
    const bool ri_previous=!std::memcmp(data.data(),"VN64S004",8);
    const bool mi_previous=!std::memcmp(data.data(),"VN64S005",8);
    const bool devices_previous=!std::memcmp(data.data(),"VN64S006",8);
    const bool caches_previous=!std::memcmp(data.data(),"VN64S007",8);
    const bool sp_previous=!std::memcmp(data.data(),"VN64S008",8);
    const bool audio_previous=!std::memcmp(data.data(),"VN64S009",8);
    const bool fpu_previous=!std::memcmp(data.data(),"VN64S010",8);
    const bool tlb_previous=!std::memcmp(data.data(),"VN64S011",8);
    const bool video_previous=!std::memcmp(data.data(),"VN64S012",8);
    const bool scalar_previous=!std::memcmp(data.data(),"VN64S013",8);
    const bool dp_previous=!std::memcmp(data.data(),"VN64S014",8);
    const bool vector_previous=!std::memcmp(data.data(),"VN64S015",8);
    const bool accumulator_previous=!std::memcmp(data.data(),"VN64S016",8);
    const bool combiner_previous=!std::memcmp(data.data(),"VN64S017",8);
    const bool depth_previous=!std::memcmp(data.data(),"VN64S018",8);
    const bool divider_previous=!std::memcmp(data.data(),"VN64S019",8);
    const bool texture_previous=!std::memcmp(data.data(),"VN64S020",8);
    const bool tmem_previous=!std::memcmp(data.data(),"VN64S021",8);
    const bool controllers_previous=!std::memcmp(data.data(),"VN64S022",8);
    const bool packet_previous=!std::memcmp(data.data(),"VN64S023",8);
    const bool conversion_previous=!std::memcmp(data.data(),"VN64S024",8);
    const bool blender_previous=!std::memcmp(data.data(),"VN64S025",8);
    const bool hidden_previous=!std::memcmp(data.data(),"VN64S026",8);
    const bool primitive_previous=!std::memcmp(data.data(),"VN64S027",8);
    const bool current=!std::memcmp(data.data(),"VN64S028",8);
    if((!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous && !video_previous && !scalar_previous && !dp_previous && !vector_previous && !accumulator_previous && !combiner_previous && !depth_previous && !divider_previous && !texture_previous && !tmem_previous && !controllers_previous && !packet_previous && !conversion_previous && !blender_previous && !hidden_previous && !(primitive_previous || current)) || data.size()!=prefix+ram_size+sp.size()+(legacy?0:previous?120:ri_previous?176:mi_previous?192:devices_previous?480:caches_previous?41440:sp_previous?41512:audio_previous?74440:fpu_previous?74448:tlb_previous?75472:video_previous?75632:scalar_previous?75888:dp_previous?75888+rsp_state_size:vector_previous?75888+rsp_state_size+dp_state_size:accumulator_previous?75888+rsp_state_size+dp_state_size+vector_state_size:combiner_previous?75888+rsp_state_size+dp_state_size+vector_state_size+accumulator_state_size:depth_previous?75888+rsp_state_size+dp_state_size+vector_state_size+accumulator_state_size+combiner_state_size:75888+rsp_state_size+dp_state_size+vector_state_size+accumulator_state_size+combiner_state_size+depth_image_state_size+(divider_previous?0:divider_state_size+(texture_previous?0:texture_state_size+(tmem_previous?0:tmem_state_size+((packet_previous || conversion_previous || blender_previous || hidden_previous || (primitive_previous || current))?controller_state_size:0)+((conversion_previous || blender_previous || hidden_previous || (primitive_previous || current))?packet_state_size:0)+((blender_previous || hidden_previous || (primitive_previous || current))?conversion_state_size:0)+((hidden_previous || (primitive_previous || current))?blender_state_size:0)+((primitive_previous || current)?hidden_state_size:0)))))+(current?primitive_depth_state_size:0) ||
        (legacy && boot_mode!=BootMode::Homebrew)) return false;
    size_t position = 8;
    auto take = [&]() { auto v = be(data.data() + position, 8); position += 8; return v; };
    if (take() != identity || take() != crc) return false;
    const uint64_t saved_frames = take(), saved_done = take();
    CpuState s;
    for (auto& value : s.gpr) value = take();
    for (auto& value : s.cp0) value = take();
    s.hi = take(); s.lo = take(); s.pc = take(); s.next_pc = take(); s.instructions = take();
    const uint64_t saved_delay = take();
    if (saved_done > 1 || saved_delay > 1 || s.gpr[0] != 0) return false;
    const auto saved_si_address = take(), saved_input = take(), saved_irq = take(), saved_mask = take(), saved_yield = take();
    if (saved_si_address > 0xffffff || saved_input > 0xffffffff || saved_irq > 1 || saved_mask > 63 || saved_yield > 1) return false;
    s.delay_slot = saved_delay;
    const size_t memory_position=position;
    std::array<uint32_t,13> saved_pi{};uint64_t saved_pi_irq=0;
    if(!legacy) {
        position=prefix+ram_size+sp.size();
        constexpr uint32_t masks[]={0xfffffe,0xfffffffe,0xffffff,127,0,255,255,15,3,255,255,15,3};
        for(unsigned n=0;n<13;++n) {const auto v=take();if(v>0xffffffffu || (v&~uint64_t(masks[n]))) return false;saved_pi[n]=uint32_t(v);}
        saved_pi_irq=take();if(saved_pi_irq>1 || take()!=uint64_t(boot_mode)) return false;
    }
    std::array<uint32_t,6> saved_ri{};uint64_t saved_current_loaded=0;
    if(!legacy && !previous) {
        constexpr uint32_t masks[]={15,127,127,0x77,0x7ffff,15};
        for(unsigned n=0;n<6;++n) {const auto v=take();if(v&~uint64_t(masks[n])) return false;saved_ri[n]=uint32_t(v);}
        saved_current_loaded=take();if(saved_current_loaded>1 || (!saved_current_loaded && saved_ri[2])) return false;
    }
    uint64_t saved_mode=0,saved_dp=0;
    if(!legacy && !previous && !ri_previous) {saved_mode=take();saved_dp=take();if(saved_mode>1023 || saved_dp>1) return false;}
    RdramDevices saved_devices;
    if(!legacy && !previous && !ri_previous && !mi_previous) {
        RdramDevices::State device_state{};
        for(auto& chip:device_state)for(auto& reg:chip) {const auto v=take();if(v>0xffffffffu)return false;reg=uint32_t(v);}
        if(!saved_devices.restore(device_state))return false;
    }
    PrimaryCaches saved_caches;
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous) {
        auto cache=[&](auto& lines,bool instruction) {for(auto& line:lines) {
            const auto page=take(),flags=take();
            if(page>0xfffff || (flags!=0 && flags!=1 && (instruction || flags!=3)))return false;
            line.page=uint32_t(page);line.flags=uint8_t(flags);
            std::copy_n(data.begin()+position,line.data.size(),line.data.begin());position+=line.data.size();
        }return true;};
        if(!cache(saved_caches.instruction,true) || !cache(saved_caches.data,false))return false;
    } else if(!s.cp0[16]) s.cp0[16]=boot_mode==BootMode::Homebrew?2:3;
    SpInterface saved_sp;
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous) {
        SpInterface::State controls{};for(auto& reg:controls) {const auto v=take();if(v>0xffffffffu)return false;reg=uint32_t(v);}
        if(!saved_sp.restore(controls))return false;
    }
    AudioInterface saved_audio;
    uint64_t saved_crystal=0,saved_output=0,saved_head=0,saved_count=0,saved_dropped=0;
    std::array<uint16_t,audio_capacity*2> saved_samples{};
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous) {
        AudioInterface::State a{};for(auto& reg:a)reg=take();if(!saved_audio.restore(a,ram.size()))return false;
        saved_crystal=take();saved_output=take();saved_head=take();saved_count=take();saved_dropped=take();
        if(saved_crystal>=93750000 || saved_output>=93750000 || saved_head>=audio_capacity || saved_count>audio_capacity)return false;
        for(auto& sample:saved_samples) {sample=uint16_t(be(data.data()+position,2));position+=2;}
    }
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous) {
        const auto control=take();if(control & ~uint64_t(0x0183ffff))return false;s.fcr31=uint32_t(control);
    }
    Tlb saved_tlb;
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous) {
        for(auto& e:saved_tlb.entries) {
            e.hi=take();const auto mask=take(),even=take(),odd=take();
            if(mask>0xffffffffu || even>0xffffffffu || odd>0xffffffffu)return false;
            e.mask=uint32_t(mask);e.even=uint32_t(even);e.odd=uint32_t(odd);
            if(!Tlb::valid(e))return false;
        }
    }
    VideoInterface saved_video;
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous) {
        VideoInterface::State v{};for(auto& value:v)value=take();if(!saved_video.restore(v))return false;
    }
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous && !video_previous)for(auto& value:s.fpr)value=take();
    RspScalar saved_rsp;uint64_t saved_rsp_phase=0;saved_rsp.redirect(saved_sp.pc);
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous && !video_previous && !scalar_previous){
        RspScalar::State r{};for(auto& value:r)value=take();if(!saved_rsp.restore(r))return false;
        saved_rsp_phase=take();if(saved_rsp_phase>=3)return false;
    }
    DpInterface saved_dp_interface;
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous && !video_previous && !scalar_previous && !dp_previous){
        DpInterface::State d{};for(auto& value:d)value=take();if(!saved_dp_interface.restore(d))return false;
    }
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous && !video_previous && !scalar_previous && !dp_previous && !vector_previous){
        RspVector::State v{};for(auto& value:v)value=take();if(!saved_rsp.vectors.restore(v))return false;
    }
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous && !video_previous && !scalar_previous && !dp_previous && !vector_previous && !accumulator_previous){
        RspVector::AccState a{};for(auto& value:a)value=take();if(!saved_rsp.vectors.acc_restore(a))return false;
    }
    if(!legacy && !previous && !ri_previous && !mi_previous && !devices_previous && !caches_previous && !sp_previous && !audio_previous && !fpu_previous && !tlb_previous && !video_previous && !scalar_previous && !dp_previous && !vector_previous && !accumulator_previous && !combiner_previous){
        RdpCombiner::State c{};for(auto& value:c)value=take();if(!saved_dp_interface.combiner.restore(c))return false;
    }
    if(divider_previous || texture_previous || tmem_previous || (controllers_previous || packet_previous || conversion_previous || blender_previous || hidden_previous || (primitive_previous || current))){
        const uint64_t z=take();if(z>0x3ffffff)return false;saved_dp_interface.depth_image=uint32_t(z);
    }
    if((texture_previous || tmem_previous || (controllers_previous || packet_previous || conversion_previous || blender_previous || hidden_previous || (primitive_previous || current))) && !saved_rsp.vectors.divider.restore(take()))return false;
    if(tmem_previous || (controllers_previous || packet_previous || conversion_previous || blender_previous || hidden_previous || (primitive_previous || current))){RdpTexture::State t{};for(auto& value:t)value=take();if(!saved_dp_interface.texture.restore(t))return false;}
    if((controllers_previous || packet_previous || conversion_previous || blender_previous || hidden_previous || (primitive_previous || current))){std::copy_n(data.begin()+position,tmem_state_size,saved_dp_interface.tmem.bytes.begin());position+=tmem_state_size;}
    std::array<uint32_t,3> saved_other_input{};
    if(packet_previous || conversion_previous || blender_previous || hidden_previous || (primitive_previous || current))for(auto& buttons:saved_other_input){const auto value=take();if(value>0xffffffffu)return false;buttons=uint32_t(value);}
    if(conversion_previous || blender_previous || hidden_previous || (primitive_previous || current)){RdpPacket::State state{};for(auto& value:state)value=take();if(!saved_dp_interface.packet.restore(state))return false;}
    if((blender_previous || hidden_previous || (primitive_previous || current)) && !saved_dp_interface.conversion.restore(take()))return false;
    if(hidden_previous || (primitive_previous || current)){RdpBlender::State state{};for(auto& value:state)value=take();if(!saved_dp_interface.blender.restore(state))return false;}
    HiddenRdram saved_hidden(ram_size);
    if((primitive_previous || current)) {
        HiddenRdram::State h;h.bits.assign(data.begin()+position,data.begin()+position+ram_size/8);position+=ram_size/8;
        h.history=take();const uint64_t delayed=take();
        if(delayed!=~uint64_t(0) && delayed>=ram_size)return false;
        h.delayed=delayed==~uint64_t(0)?-1:int64_t(delayed);
        if(!saved_hidden.restore(h))return false;
    } else saved_hidden.ordinary_burst(0,data.data()+memory_position+64,ram_size);
    if(current && !saved_dp_interface.primitive_depth.restore(take()))return false;
    position=memory_position;
    // Validate first; rejected states leave the active machine untouched.
    hidden=std::move(saved_hidden);
    cpu.state = s;cpu.caches=saved_caches;cpu.tlb=saved_tlb; frames = saved_frames; done = saved_done;
    si_address = uint32_t(saved_si_address); input_buttons = uint32_t(saved_input);other_input_buttons=saved_other_input; si_interrupt = saved_irq;
    mi_mask = uint8_t(saved_mask); yielded = saved_yield;
    pi=saved_pi;pi_interrupt=saved_pi_irq;ri=saved_ri;ri_current_loaded=saved_current_loaded;mi_mode=uint16_t(saved_mode);dp_interrupt=saved_dp;rdram_devices=saved_devices;sp_interface=saved_sp;rsp=saved_rsp;rsp_phase=saved_rsp_phase;dp_interface=saved_dp_interface;audio_interface=saved_audio;video_interface=saved_video;audio_samples=saved_samples;crystal_phase=saved_crystal;output_phase=saved_output;audio_head=saved_head;audio_count=saved_count;audio_dropped=saved_dropped;
    std::copy_n(data.begin() + position, 64, pif.begin()); position += 64;
    std::copy_n(data.begin() + position, ram_size, ram.begin());
    std::copy_n(data.begin() + position + ram_size, sp.size(), sp.begin());
    return true;
}
}
