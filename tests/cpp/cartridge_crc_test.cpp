#include "cores/gb/src/cartridge.hpp"
#include "cores/gba/src/cartridge.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    // The first byte exercises IEEE CRC32 table entry $F5 when seeded with
    // $FFFFFFFF. Both expected values were computed independently by zlib.
    std::vector<uint8_t> gb_rom(0x8000, 0);
    for (unsigned i = 0; i < 256; ++i) gb_rom[i] = static_cast<uint8_t>(i ^ 0x0A);
    gb::Cartridge gb_cart;
    if (!gb_cart.load(gb_rom.data(), gb_rom.size()) || gb_cart.get_crc32() != 0x03C1199A) {
        std::cerr << "GB cartridge CRC32 disagrees with IEEE reference\n";
        return 1;
    }

    std::vector<uint8_t> gba_rom(256);
    for (unsigned i = 0; i < 256; ++i) gba_rom[i] = static_cast<uint8_t>(i ^ 0x0A);
    for (unsigned i = 0xA0; i < 0xB0; ++i) gba_rom[i] = 0;  // Title / game code
    gba::Cartridge gba_cart;
    if (!gba_cart.load(gba_rom.data(), gba_rom.size(), gba::SystemType::GameBoyAdvance) ||
        gba_cart.get_crc32() != 0x9B9F9C0C) {
        std::cerr << "GBA cartridge CRC32 disagrees with IEEE reference\n";
        return 1;
    }
    std::cout << "GB/GBA cartridge identities match standard CRC32\n";
    return 0;
}
