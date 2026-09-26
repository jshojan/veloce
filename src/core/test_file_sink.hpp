#pragma once

// Application-owned writer for the VELOCE-RESULT/1 test-result file.
//
// The core pushes channel bytes / notes / blobs through emu::ITestSink; this
// class turns them into a line-flushed file the test harness reads back:
//
//   #VELOCE 1 core=NES rom_crc32=1a2b3c4d channels=port,blargg6000
//   VELOCE 1 nes cpu.instr            <- ROM-emitted lines, verbatim
//   CHECK 1 PASS basics
//   END 1/1
//   #VELOCE end reason=terminator frames=7 cycles=2497314 status=0
//
// It also drives the run-control side of the protocol: early exit on the
// terminator (VELOCE_TEST_EXIT) and the blargg "press reset" handshake
// (VELOCE_TEST_RESETS). The file format is specified in
// docs/testing/VELOCE-RESULT.md.
//
// Kept free of SDL / Application so it can be unit-tested with a stub plugin
// (tests/cpp/test_file_sink_test.cpp).

#include "emu/emulator_plugin.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace emu {

struct TestRunConfig {
    std::string out_path;          // VELOCE_TEST_OUT
    bool exit_on_finish = true;    // VELOCE_TEST_EXIT (default 1)
    int max_resets = 3;            // VELOCE_TEST_RESETS (default 3)
    int reset_delay_frames = 3;    // frames between request_reset() and reset()

    // Fill from the environment. Returns false when VELOCE_TEST_OUT is unset
    // or empty (no test session).
    static bool from_env(TestRunConfig& out);
};

class TestFileSink final : public ITestSink {
public:
    // Longest line written for one channel line (mGBA's string buffer is 256).
    static constexpr size_t kMaxLineBytes = 255;

    enum class EndReason { Terminator, Frames, ResetLimit, Quit };

    explicit TestFileSink(TestRunConfig config);
    ~TestFileSink() override;

    TestFileSink(const TestFileSink&) = delete;
    TestFileSink& operator=(const TestFileSink&) = delete;

    // Create/truncate the output file. Returns false (with errno intact) on
    // failure.
    bool open();
    bool is_open() const { return m_file != nullptr; }

    // Write the header and hand the sink to the plugin. The plugin's test
    // methods are only called when plugin_api_version is at least
    // EMU_PLUGIN_API_VERSION_TEST_SINK (older vtables do not have them).
    void attach(IEmulatorPlugin* plugin, uint32_t plugin_api_version);

    // Call once after every completed run_frame() with the number of frames
    // run so far (including this one). Performs a pending reset when due.
    // Returns true when the frame loop should stop.
    bool after_frame(uint64_t frames_run);

    // Detach from the plugin, flush a partial line, write the trailer and
    // close the file. `fallback` is used when neither the terminator (with
    // exit enabled) nor the reset limit ended the run. Idempotent.
    void close(EndReason fallback, uint64_t frames_run, uint64_t cycles);

    // Detach from the plugin without closing (plugin about to be destroyed).
    void detach();

    bool finished() const { return m_finished; }
    int32_t status() const { return m_status; }
    int resets_done() const { return m_resets_done; }
    bool reset_limit_hit() const { return m_reset_limit_hit; }
    bool sink_accepted() const { return m_sink_accepted; }
    const std::string& path() const { return m_config.out_path; }

    // ITestSink
    void write(const char* bytes, size_t n) override;
    void finish(int32_t status_code) override;
    void note(const char* key, const char* value) override;
    void blob(const char* name, const uint8_t* bytes, size_t n) override;
    void request_reset() override;

    static const char* reason_name(EndReason reason);
    // Lowercase hex SHA-256 of a buffer (used for blob lines; exposed for tests).
    static std::string sha256_hex(const uint8_t* data, size_t n);

private:
    void emit_line(const std::string& line);   // one ROM channel line
    void emit_meta(const std::string& line);   // one "#VELOCE ..." line
    void flush_partial();

    TestRunConfig m_config;
    FILE* m_file = nullptr;
    IEmulatorPlugin* m_plugin = nullptr;
    bool m_plugin_has_test_api = false;
    bool m_sink_accepted = false;
    bool m_closed = false;

    std::string m_line;           // pending (unterminated) channel line
    bool m_line_truncated = false;

    bool m_finished = false;
    int32_t m_status = -1;

    uint64_t m_frames = 0;        // frames completed, as reported by after_frame()
    bool m_reset_pending = false;
    uint64_t m_reset_at = 0;
    int m_resets_done = 0;
    bool m_reset_limit_hit = false;
};

} // namespace emu
