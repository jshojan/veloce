#pragma once

#include <cstdlib>
#include <cstdio>

namespace gba {

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
// The jsmolka gba-tests ROMs use R12 to indicate test results:
// - R12 = 0: All tests passed
// - R12 = N (N > 0): Failed at test #N
//
// Tests end with an infinite loop (B .) after displaying results
struct TestResult {
    bool detected = false;      // Whether we've detected a test ROM result
    bool passed = false;        // Whether the test passed (R12 == 0)
    uint32_t failed_test = 0;   // Test number that failed (if any)
    uint64_t cycle_count = 0;   // Cycle count when result was detected

    void report() const {
        if (!detected) return;

        fprintf(stderr, "\n=== GBA TEST ROM RESULT ===\n");
        if (passed) {
            fprintf(stderr, "[GBA] PASSED - All tests completed successfully\n");
        } else {
            fprintf(stderr, "[GBA] FAILED - Failed at test #%u\n", failed_test);
        }
        fprintf(stderr, "Cycles: %llu\n", static_cast<unsigned long long>(cycle_count));
        fprintf(stderr, "===========================\n");
    }
};

// Debug print macro for GBA
#define GBA_DEBUG_PRINT(...) \
    do { if (gba::is_debug_mode()) fprintf(stderr, "[GBA] " __VA_ARGS__); } while(0)

// Debug print for test results
#define GBA_TEST_PASSED() \
    do { if (gba::is_debug_mode()) fprintf(stderr, "[GBA] PASSED - All tests completed successfully\n"); } while(0)

#define GBA_TEST_FAILED(test_num) \
    do { if (gba::is_debug_mode()) fprintf(stderr, "[GBA] FAILED - Failed at test #%u\n", (test_num)); } while(0)

} // namespace gba
