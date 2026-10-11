#pragma once
#include "input_types.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace emu::n64_input {
// N64 input word v1: compact buttons [13:0], tag 11 [15:14],
// signed two's-complement X [23:16], Y [31:24]. Untagged values retain
// the original VirtualButton mask with a neutral stick. UI bits 14/15
// are not N64 buttons and must not be included in legacy N64 input words.
constexpr uint32_t tag = 0xc000;
constexpr std::array<VirtualButton,14> controls = {
    VirtualButton::A,VirtualButton::B,VirtualButton::Z,VirtualButton::Start,
    VirtualButton::Up,VirtualButton::Down,VirtualButton::Left,VirtualButton::Right,
    VirtualButton::L,VirtualButton::R,VirtualButton::CUp,VirtualButton::CDown,
    VirtualButton::CLeft,VirtualButton::CRight};
inline bool encoded(uint32_t word) { return (word & tag) == tag; }
inline uint32_t pack(uint32_t buttons, int x, int y) {
    uint32_t word = tag;
    for (unsigned i=0;i<controls.size();++i)
        if (is_button_pressed(buttons,controls[i])) word |= 1u << i;
    return word | (uint32_t(uint8_t(std::clamp(x,-128,127))) << 16)
                | (uint32_t(uint8_t(std::clamp(y,-128,127))) << 24);
}
inline uint32_t buttons(uint32_t word) {
    if (!encoded(word)) return word;
    uint32_t result=0;
    for (unsigned i=0;i<controls.size();++i)
        if (word & (1u<<i)) result |= button_to_mask(controls[i]);
    return result;
}
inline int axis(uint32_t word, unsigned index) {
    if (!encoded(word) || index>1) return 0;
    const unsigned value=(word >> (16+index*8)) & 255;
    return value<128 ? int(value) : int(value)-256;
}
// Frontend policy, not a physical controller calibration: independent axial
// dead zone, rescaled to +/-80. The canonical word carries integers only.
inline int quantize(float value) {
    if (!std::isfinite(value)) return 0;
    value=std::clamp(value,-1.0f,1.0f);
    const float magnitude=std::abs(value);
    if (magnitude<=0.15f) return 0;
    const int result=int(std::lround((magnitude-0.15f)*(80.0f/0.85f)));
    return value<0 ? -result : result;
}
}
