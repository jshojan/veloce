#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace n64 {
// Cartridge IDs with parallel-interface SRAM in ares' Nintendo 64 media
// database. The 96 KiB Dezaemon 3D board is kept separate from 32 KiB media.
// https://github.com/ares-emulator/ares/blob/master/mia/medium/nintendo-64.cpp
inline size_t native_sram_bytes(const uint8_t* rom) {
    static constexpr std::array<std::string_view,37> ids={
        "NTE","NVB","NB5","CFZ","NFZ","NSI","NG6","NGP","NYW","NHY",
        "NIB","NPS","NPA","NP4","NJ5","NP6","NPE","NJG","CZL","NZL",
        "NKG","NMF","NRI","NUT","NUM","NOB","CPS","NPM","NRE","NAL",
        "NT3","NS4","NA2","NVP","NWL","NW2","NWX"};
    const std::string_view id{reinterpret_cast<const char*>(rom+0x3b),3};
    if(id=="CDZ")return 96*1024;
    return std::find(ids.begin(),ids.end(),id)!=ids.end()?32*1024:0;
}
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
