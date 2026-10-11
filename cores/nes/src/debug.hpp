#pragma once

#include <cstdlib>
#include <cstdio>
#include <cstdint>

namespace nes {

// Single debug mode check - caches result of DEBUG environment variable
inline bool is_debug_mode() {
    static bool checked = false;
    static bool debug = false;
    if (!checked) {
        const char* env = std::getenv("DEBUG");
        debug = (env != nullptr && env[0] != '0');
        checked = true;
    }
    return debug;
}

// Debug print macro for NES
#define NES_DEBUG_PRINT(...) \
    do { if (nes::is_debug_mode()) fprintf(stderr, "[NES] " __VA_ARGS__); } while(0)

} // namespace nes
