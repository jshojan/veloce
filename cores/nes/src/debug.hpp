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

// Test result tracking for automated test ROMs
// Blargg-style test ROMs use signature 0xDE 0xB0 0x61 at $6001-$6003
// Status code at $6000:
// - 0x80: Test running
// - 0x00: Test passed
// - 0x01+: Test failed with error code
struct TestResult {
    bool detected = false;      // Whether we've detected a test ROM result
    bool passed = false;        // Whether the test passed (status == 0)
    uint8_t status_code = 0;    // Status code from $6000
    uint64_t frame_count = 0;   // Frame count when result was detected

    void report() const {
        if (!detected) return;

        fprintf(stderr, "\n=== NES TEST ROM RESULT ===\n");
        if (passed) {
            fprintf(stderr, "Status code: 0 (PASSED)\n");
        } else {
            fprintf(stderr, "Status code: %u (FAILED)\n", status_code);
        }
        fprintf(stderr, "Frames: %llu\n", static_cast<unsigned long long>(frame_count));
        fprintf(stderr, "===========================\n");
    }
};

// Debug print macro for NES
#define NES_DEBUG_PRINT(...) \
    do { if (nes::is_debug_mode()) fprintf(stderr, "[NES] " __VA_ARGS__); } while(0)

// Debug print for test results
#define NES_TEST_PASSED() \
    do { if (nes::is_debug_mode()) fprintf(stderr, "Status code: 0 (PASSED)\n"); } while(0)

#define NES_TEST_FAILED(status) \
    do { if (nes::is_debug_mode()) fprintf(stderr, "Status code: %u (FAILED)\n", (status)); } while(0)

} // namespace nes
