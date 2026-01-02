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

// Blargg test result detection
// Blargg's tests write results to specific memory addresses:
// - $6000: Status code (0x00=pass, 0x01-0x7F=fail, 0x80=running, 0x81=needs reset)
// - $6001-$6003: Signature bytes (0xDE 0xB0 0x61)
// - $6004+: Result text (null-terminated ASCII string)
struct BlarggTestState {
    static constexpr uint16_t STATUS_ADDR = 0x6000;
    static constexpr uint16_t SIGNATURE_START = 0x6001;
    static constexpr uint16_t RESULT_TEXT_START = 0x6004;
    static constexpr uint16_t RESULT_BUFFER_SIZE = 256;

    // Expected signature bytes: 0xDE 0xB0 0x61 (or 0xDE 0xB0 0xG1 in some tests)
    static constexpr uint8_t SIGNATURE_0 = 0xDE;
    static constexpr uint8_t SIGNATURE_1 = 0xB0;
    static constexpr uint8_t SIGNATURE_2 = 0x61;  // Also accept 0x47 ('G') as alternate

    // Status codes
    static constexpr uint8_t STATUS_PASS = 0x00;
    static constexpr uint8_t STATUS_RUNNING = 0x80;
    static constexpr uint8_t STATUS_NEEDS_RESET = 0x81;

    bool detected = false;          // Have we detected a Blargg test?
    bool completed = false;         // Has the test completed?
    bool passed = false;            // Did the test pass?
    uint8_t status_code = 0x80;     // Current status code (0x80 = running)
    uint64_t frame_count = 0;       // Frame count when completed
    char result_text[RESULT_BUFFER_SIZE] = {0};  // Result text from test

    // Memory buffer for $6000-$60FF region to track test output
    uint8_t test_memory[256] = {0};

    void reset() {
        detected = false;
        completed = false;
        passed = false;
        status_code = STATUS_RUNNING;
        frame_count = 0;
        std::memset(result_text, 0, sizeof(result_text));
        std::memset(test_memory, 0, sizeof(test_memory));
    }

    // Called when memory in $6000-$60FF is written
    void on_memory_write(uint16_t offset, uint8_t value) {
        if (offset >= 256) return;

        test_memory[offset] = value;

        // Check for signature after writing to $6001-$6003
        if (offset >= 1 && offset <= 3) {
            check_signature();
        }

        // Check for status update
        if (offset == 0 && detected) {
            update_status();
        }

        // Capture result text
        if (offset >= 4 && offset < 256 && detected) {
            // Result text is at $6004+
            size_t text_offset = offset - 4;
            if (text_offset < RESULT_BUFFER_SIZE - 1) {
                result_text[text_offset] = (value >= 0x20 && value < 0x7F) ? value : 0;
            }
        }
    }

    void check_signature() {
        // Check if signature bytes match
        if (test_memory[1] == SIGNATURE_0 &&
            test_memory[2] == SIGNATURE_1 &&
            (test_memory[3] == SIGNATURE_2 || test_memory[3] == 0x47)) {
            if (!detected && is_debug_mode()) {
                fprintf(stderr, "[SNES] Blargg test ROM detected (signature at $6001)\n");
            }
            detected = true;
        }
    }

    void update_status() {
        if (!detected) return;

        uint8_t new_status = test_memory[0];

        // Only report changes when test completes (status goes from 0x80 to something else)
        if (status_code == STATUS_RUNNING && new_status != STATUS_RUNNING) {
            completed = true;
            status_code = new_status;
            passed = (new_status == STATUS_PASS);

            // Capture any result text that's already been written
            for (size_t i = 4; i < 256 && (i - 4) < RESULT_BUFFER_SIZE - 1; i++) {
                uint8_t c = test_memory[i];
                if (c == 0) break;
                result_text[i - 4] = (c >= 0x20 && c < 0x7F) ? c : 0;
            }
        }

        status_code = new_status;
    }

    void report(uint64_t current_frame) const {
        if (!detected) return;

        if (is_debug_mode()) {
            fprintf(stderr, "\n=== BLARGG TEST RESULT ===\n");
            fprintf(stderr, "BLARGG_STATUS: 0x%02X\n", status_code);

            if (completed) {
                if (passed) {
                    fprintf(stderr, "Status code: 0 (PASSED)\n");
                } else {
                    fprintf(stderr, "Status code: %u (FAILED)\n", status_code);
                }
            } else if (status_code == STATUS_RUNNING) {
                fprintf(stderr, "Status: Test still running\n");
            } else if (status_code == STATUS_NEEDS_RESET) {
                fprintf(stderr, "Status: Test needs reset\n");
            }

            if (result_text[0] != 0) {
                fprintf(stderr, "BLARGG_RESULT: %s\n", result_text);
            }

            fprintf(stderr, "Frame: %llu\n", static_cast<unsigned long long>(current_frame));
            fprintf(stderr, "==========================\n");
        }
    }

    // Check if we should early-exit (test completed)
    bool should_exit() const {
        return detected && completed;
    }
};

// Legacy test result tracking (for non-Blargg tests)
struct TestResult {
    bool detected = false;      // Whether we've detected a test ROM result
    bool passed = false;        // Whether the test passed
    uint8_t status_code = 0;    // Status code from test
    uint64_t frame_count = 0;   // Frame count when result was detected

    void report() const {
        if (!detected) return;

        fprintf(stderr, "\n=== SNES TEST ROM RESULT ===\n");
        if (passed) {
            fprintf(stderr, "Status code: 0 (PASSED)\n");
        } else {
            fprintf(stderr, "Status code: %u (FAILED)\n", status_code);
        }
        fprintf(stderr, "Frames: %llu\n", static_cast<unsigned long long>(frame_count));
        fprintf(stderr, "============================\n");
    }
};

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

// Debug print for test results
#define SNES_TEST_PASSED() \
    do { if (snes::is_debug_mode()) fprintf(stderr, "Status code: 0 (PASSED)\n"); } while(0)

#define SNES_TEST_FAILED(status) \
    do { if (snes::is_debug_mode()) fprintf(stderr, "Status code: %u (FAILED)\n", (status)); } while(0)

} // namespace snes
