#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace n64 {
// Three-byte cartridge IDs at header $3B-$3D. These are the known 2 KiB
// EEPROM titles in ares' Nintendo 64 media table. Unknown 6102 carts keep the
// existing 512-byte fallback; a 2 KiB save can still select the larger media.
// https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp
inline size_t native_eeprom_bytes(const uint8_t* rom) {
    static constexpr std::array<std::string_view,23> ids={
        "NB7","NGT","NFU","NCW","NCZ","ND6","NDO","ND2","N3D","NMX",
        "NGC","NIM","NNB","NMV","NM8","NEV","NPP","NUB","NPD","NRZ",
        "NR7","NEP","NYS"};
    const std::string_view id{reinterpret_cast<const char*>(rom+0x3b),3};
    return std::find(ids.begin(),ids.end(),id)!=ids.end()?2048:512;
}
}
