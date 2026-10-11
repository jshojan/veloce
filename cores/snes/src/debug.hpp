#pragma once

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace snes {

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

// Debug print macro for SNES
#define SNES_DEBUG_PRINT(...) \
    do { if (snes::is_debug_mode()) fprintf(stderr, "[SNES] " __VA_ARGS__); } while(0)

// Component-specific debug macros
#define SNES_CPU_DEBUG(...) \
    do { if (snes::is_debug_mode()) fprintf(stderr, "[SNES/CPU] " __VA_ARGS__); } while(0)

#define SNES_PPU_DEBUG(...) \
    do { if (snes::is_debug_mode()) fprintf(stderr, "[SNES/PPU] " __VA_ARGS__); } while(0)

#define SNES_APU_DEBUG(...) \
    do { if (snes::is_debug_mode()) fprintf(stderr, "[SNES/APU] " __VA_ARGS__); } while(0)

#define SNES_DMA_DEBUG(...) \
    do { if (snes::is_debug_mode()) fprintf(stderr, "[SNES/DMA] " __VA_ARGS__); } while(0)

} // namespace snes
