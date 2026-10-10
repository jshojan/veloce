#include "machine.hpp"
#include <algorithm>
#include <cstring>

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
    frames = 0; done = false;
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
    uint8_t* destination = nullptr;
    if (uint64_t(p) + bytes <= ram.size()) destination = ram.data() + p;
    else if (p >= 0x04000000 && uint64_t(p) + bytes <= 0x04000000 + sp.size()) destination = sp.data() + p - 0x04000000;
    if (!destination) return false;
    for (unsigned i = 0; i < bytes; ++i) destination[i] = uint8_t(value >> (8 * (bytes - 1 - i)));
    return true;
}
static void append(std::vector<uint8_t>& data, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) data.push_back(uint8_t(value >> (56 - 8 * i)));
}
bool Machine::save(std::vector<uint8_t>& data) const {
    if (rom.empty()) return false;
    data.assign({'V','N','6','4','S','0','0','1'});
    append(data, identity); append(data, crc); append(data, frames); append(data, done);
    const auto& s = cpu.state;
    for (auto value : s.gpr) append(data, value);
    for (auto value : s.cp0) append(data, value);
    for (auto value : {s.hi, s.lo, s.pc, s.next_pc, s.instructions, uint64_t(s.delay_slot)}) append(data, value);
    data.insert(data.end(), ram.begin(), ram.end());
    data.insert(data.end(), sp.begin(), sp.end());
    return true;
}
bool Machine::restore(const std::vector<uint8_t>& data) {
    constexpr size_t prefix = 8 + (4 + 32 + 32 + 6) * 8;
    if (rom.empty() || data.size() != prefix + ram_size + sp.size() || std::memcmp(data.data(), "VN64S001", 8)) return false;
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
    s.delay_slot = saved_delay;
    // Validate first; rejected states leave the active machine untouched.
    cpu.state = s; frames = saved_frames; done = saved_done;
    std::copy_n(data.begin() + position, ram_size, ram.begin());
    std::copy_n(data.begin() + position + ram_size, sp.size(), sp.begin());
    return true;
}
}
