#include "pif_joybus.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

static void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

static n64::PifJoybus::Ram packet(n64::PifJoybus& device, unsigned command,
                                  unsigned tx, unsigned rx, unsigned block,
                                  const std::array<uint8_t, 8>& payload = {}) {
    n64::PifJoybus::Ram ram{};
    ram.fill(0xa5);
    for (unsigned port = 0; port < 4; ++port) ram[port] = 0;
    ram[4] = uint8_t(tx);
    ram[5] = uint8_t(rx);
    ram[6] = uint8_t(command);
    ram[7] = uint8_t(block);
    for (unsigned lane = 0; lane < 8; ++lane) ram[8 + lane] = payload[lane];
    ram[4 + 2 + tx + rx] = 0xfe;
    ram[63] = 1;
    device.format(ram);
    device.poll(ram, {});
    return ram;
}

int main() {
    n64::PifJoybus device;
    device.configure_eeprom(2048);
    const auto blank = device.eeprom_data();
    require(device.eeprom_bytes() == 2048, "EEPROM extent");
    for (auto byte : blank) require(byte == 0xff, "erased EEPROM byte");
    for (unsigned command : {0u, 255u}) {
        const auto status = packet(device, command, 1, 3, 0);
        require(status[5] == 3 && status[7] == 0 && status[8] == 0xc0 &&
                status[9] == 0, "16 Kbit cartridge identity");
    }
    uint64_t cases = 0;
    for (unsigned block = 0; block < 256; ++block) {
        std::array<uint8_t, 8> bytes{};
        for (unsigned lane = 0; lane < 8; ++lane)
            bytes[lane] = uint8_t(block * 37 + lane * 103);
        const auto written = packet(device, 5, 10, 1, block, bytes);
        require(written[5] == 1 && written[16] == 0, "EEPROM write reply");
        const auto read = packet(device, 4, 2, 8, block);
        require(read[5] == 8, "EEPROM read reply length");
        for (unsigned lane = 0; lane < 8; ++lane)
            require(read[8 + lane] == bytes[lane], "EEPROM block isolation");
        ++cases;
    }
    const auto stored = device.eeprom_data();
    for (unsigned command : {4u, 5u})
        for (unsigned tx = 1; tx <= 11; ++tx)
            for (unsigned rx = 1; rx <= 9; ++rx) {
                if ((command == 4 && tx == 2 && rx == 8) ||
                    (command == 5 && tx == 10 && rx == 1)) continue;
                const auto malformed = packet(device, command, tx, rx, 0);
                require(malformed[5] == (rx | 64) && device.eeprom_data() == stored,
                        "malformed EEPROM packet mutated storage");
                ++cases;
            }
    device.reset();
    require(device.eeprom_data() == stored, "soft reset erased battery storage");
    device.erase_eeprom();
    require(device.eeprom_data() == blank, "new cartridge did not erase EEPROM");
    std::cout << cases << " 16 Kbit EEPROM status/read/write/length/reset cases PASS\n";
}
