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
    if (!data || size < 0x1004 || size > ram_size - 0x400 + 0x1000 || (size & 3)) return false;
    std::vector<uint8_t> image(data, data + size);
    const uint32_t magic = uint32_t(be(data, 4));
    if (magic == 0x37804012) {
        for (size_t i = 0; i < size; i += 2) std::swap(image[i], image[i + 1]);
    } else if (magic == 0x40123780) {
        for (size_t i = 0; i < size; i += 4) { std::swap(image[i], image[i + 3]); std::swap(image[i + 1], image[i + 2]); }
    } else if (magic != 0x80371240) return false;
    // This opt-in profile has no Nintendo IPL bytes. Retail images are rejected.
    if (std::memcmp(image.data() + 0x34, "V64HB001", 8)) return false;
    const uint32_t start = uint32_t(be(image.data() + 8, 4));
    uint32_t target = 0;
    if (!physical(sign32(start), target) || (start & 3) || target < 0x400 || target >= 0x400 + size - 0x1000) return false;
    crc = crc32(data, size);
    identity = 14695981039346656037ull;
    for (auto byte : image) { identity ^= byte; identity *= 1099511628211ull; }
    entry = sign32(start);
    rom = std::move(image);
    reset();
    return true;
}
void Machine::reset() {
    std::fill(ram.begin(), ram.end(), 0);
    sp.fill(0);
    if (!rom.empty()) std::copy(rom.begin() + 0x1000, rom.end(), ram.begin() + 0x400);
    cpu.reset(entry);
    frames = 0; done = false; yielded = false;
    pif.fill(0); si_address = input_buttons = 0; si_interrupt = false; mi_mask = 0;
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
        if (channel != 0 || (command != 0 && command != 0xff && command != 1)) {
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
                if (emu::is_button_pressed(input_buttons, controls[i])) buttons |= masks[i];
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
    if (uint64_t(p) + bytes <= ram.size()) { value = be(ram.data() + p, bytes); return true; }
    if (p >= 0x04000000 && uint64_t(p) + bytes <= 0x04000000 + sp.size()) { value = be(sp.data() + p - 0x04000000, bytes); return true; }
    if (p >= 0x10000000 && uint64_t(p) + bytes <= 0x10000000 + rom.size()) { value = be(rom.data() + p - 0x10000000, bytes); return true; }
    if (p >= 0x1fc007c0 && uint64_t(p) + bytes <= 0x1fc00800) { value = be(pif.data() + p - 0x1fc007c0, bytes); return true; }
    if (bytes == 4) {
        if (p == 0x04800000) { value = si_address; return true; }
        if (p == 0x04800018) { value = si_interrupt ? 0x1000 : 0; return true; }
        if (p == 0x04300008) { value = si_interrupt ? 2 : 0; return true; }
        if (p == 0x0430000c) { value = mi_mask; return true; }
    }
    return false;
}
bool Machine::write(uint64_t address, unsigned bytes, uint64_t value) {
    uint32_t p = 0;
    if (!physical(address, p) || (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8)) return false;
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
        if (p == 0x04800000) { si_address = uint32_t(value) & 0xffffff; return true; }
        if (p == 0x04800018) { si_interrupt = false; return true; }
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
            else std::copy(pif.begin(), pif.end(), ram.begin() + si_address);
            si_interrupt = true; return true;
        }
    }
    uint8_t* destination = nullptr;
    if (uint64_t(p) + bytes <= ram.size()) destination = ram.data() + p;
    else if (p >= 0x04000000 && uint64_t(p) + bytes <= 0x04000000 + sp.size()) destination = sp.data() + p - 0x04000000;
    if (!destination) return false;
    for (unsigned i = 0; i < bytes; ++i) destination[i] = uint8_t(value >> (8 * (bytes - 1 - i)));
    return true;
}
bool Machine::write_masked(uint64_t address, unsigned bytes, uint64_t value, uint8_t mask) {
    uint32_t p = 0;
    if ((bytes != 4 && bytes != 8) || (address & (bytes - 1)) || !mask ||
        (unsigned(mask) >> bytes) || !physical(address, p)) return false;
    uint8_t* destination = nullptr;
    if (uint64_t(p) + bytes <= ram.size()) destination = ram.data() + p;
    else if (p >= 0x04000000 && uint64_t(p) + bytes <= 0x04000000 + sp.size()) destination = sp.data() + p - 0x04000000;
    if (!destination) return false;
    for (unsigned i = 0; i < bytes; ++i)
        if (mask & (1u << i)) destination[i] = uint8_t(value >> (8 * (bytes - 1 - i)));
    return true;
}
static void append(std::vector<uint8_t>& data, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) data.push_back(uint8_t(value >> (56 - 8 * i)));
}
bool Machine::save(std::vector<uint8_t>& data) const {
    if (rom.empty()) return false;
    data.assign({'V','N','6','4','S','0','0','2'});
    append(data, identity); append(data, crc); append(data, frames); append(data, done);
    const auto& s = cpu.state;
    for (auto value : s.gpr) append(data, value);
    for (auto value : s.cp0) append(data, value);
    for (auto value : {s.hi, s.lo, s.pc, s.next_pc, s.instructions, uint64_t(s.delay_slot)}) append(data, value);
    append(data, si_address); append(data, input_buttons); append(data, si_interrupt); append(data, mi_mask); append(data, yielded);
    data.insert(data.end(), pif.begin(), pif.end());
    data.insert(data.end(), ram.begin(), ram.end());
    data.insert(data.end(), sp.begin(), sp.end());
    return true;
}
bool Machine::restore(const std::vector<uint8_t>& data) {
    constexpr size_t prefix = 8 + (4 + 32 + 32 + 6 + 5) * 8 + 64;
    if (rom.empty() || data.size() != prefix + ram_size + sp.size() || std::memcmp(data.data(), "VN64S002", 8)) return false;
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
    // Validate first; rejected states leave the active machine untouched.
    cpu.state = s; frames = saved_frames; done = saved_done;
    si_address = uint32_t(saved_si_address); input_buttons = uint32_t(saved_input); si_interrupt = saved_irq;
    mi_mask = uint8_t(saved_mask); yielded = saved_yield;
    std::copy_n(data.begin() + position, 64, pif.begin()); position += 64;
    std::copy_n(data.begin() + position, ram_size, ram.begin());
    std::copy_n(data.begin() + position + ram_size, sp.size(), sp.begin());
    return true;
}
}
