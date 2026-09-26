// Unit test for the application-owned test-result sink (plugin ABI v2).
//
// A stub IEmulatorPlugin plays a scripted ROM: it pushes channel bytes, notes,
// blobs, reset requests and terminators through the ITestSink, and the test
// drives the same per-frame contract the Application uses
// (run_frame -> after_frame -> stop?) and then checks the file byte-for-byte.
//
// No SDL, no ROMs. Registered with CTest as "test_file_sink" (labels unit;fast).

#include "core/test_file_sink.hpp"
#include "emu/emulator_plugin.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using emu::TestFileSink;
using emu::TestRunConfig;

static int g_failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                             \
    } while (0)

#define CHECK_EQ(a, b)                                                                  \
    do {                                                                                \
        auto _va = (a);                                                                 \
        auto _vb = (b);                                                                 \
        if (!(_va == _vb)) {                                                            \
            ++g_failures;                                                               \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b  \
                      << "\n    got:  [" << _va << "]\n    want: [" << _vb << "]\n";    \
        }                                                                               \
    } while (0)

// ---------------------------------------------------------------------------
// Stub plugin: a per-frame script of actions against the sink.
// ---------------------------------------------------------------------------
class StubPlugin : public emu::IEmulatorPlugin {
public:
    using Action = std::function<void(emu::ITestSink&, StubPlugin&)>;

    bool supports_sink = true;
    std::string channels = "port,blargg6000";
    std::map<uint64_t, std::vector<Action>> script;   // keyed by run_frame() call index

    emu::ITestSink* sink = nullptr;
    int set_sink_calls = 0;
    int resets = 0;
    int test_resets = 0;
    uint64_t frame = 0;
    uint64_t reset_frames[8] = {};

    emu::EmulatorInfo get_info() override {
        static const char* exts[] = {".stub", nullptr};
        emu::EmulatorInfo i{};
        i.name = "Stub Core";      // space must be sanitised in the header
        i.version = "0";
        i.author = "test";
        i.description = "stub";
        i.file_extensions = exts;
        i.native_fps = 60.0;
        i.cycles_per_second = 1000;
        i.screen_width = 1;
        i.screen_height = 1;
        return i;
    }
    bool load_rom(const uint8_t*, size_t) override { return true; }
    void unload_rom() override {}
    bool is_rom_loaded() const override { return true; }
    uint32_t get_rom_crc32() const override { return 0x00c0ffee; }
    void reset() override {
        if (resets < 8) reset_frames[resets] = frame;
        ++resets;
    }
    void run_frame(const emu::InputState&) override {
        auto it = script.find(frame);
        if (it != script.end() && sink) {
            for (auto& a : it->second) a(*sink, *this);
        }
        ++frame;
    }
    uint64_t get_cycle_count() const override { return frame * 100; }
    uint64_t get_frame_count() const override { return frame; }
    emu::FrameBuffer get_framebuffer() override { return {nullptr, 0, 0}; }
    emu::AudioBuffer get_audio() override { return {nullptr, 0, 0}; }
    void clear_audio_buffer() override {}
    uint8_t read_memory(uint16_t) override { return 0; }
    void write_memory(uint16_t, uint8_t) override {}
    bool save_state(std::vector<uint8_t>&) override { return false; }
    bool load_state(const std::vector<uint8_t>&) override { return false; }

    bool set_test_sink(emu::ITestSink* s) override {
        ++set_sink_calls;
        if (!supports_sink) return false;
        sink = s;
        return true;
    }
    const char* test_channels() const override { return channels.c_str(); }
    void on_test_reset() override { ++test_resets; }
};

// A v1-era core: never overrides the test methods.
class LegacyPlugin : public StubPlugin {};

// ---------------------------------------------------------------------------
static fs::path g_dir;

static std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct RunResult {
    uint64_t frames_run = 0;
    std::string text;
};

// Mirrors Application::run()'s headless loop for the test-session part.
static RunResult run_session(StubPlugin& plugin, TestRunConfig cfg, int budget,
                             uint32_t api_version = EMU_PLUGIN_API_VERSION) {
    TestFileSink sink(cfg);
    CHECK(sink.open());
    sink.attach(&plugin, api_version);
    uint64_t frames_run = 0;
    bool stop = false;
    emu::InputState in{};
    while (!stop && frames_run < static_cast<uint64_t>(budget)) {
        plugin.run_frame(in);
        ++frames_run;
        stop = sink.after_frame(frames_run);
    }
    sink.close(frames_run >= static_cast<uint64_t>(budget) ? TestFileSink::EndReason::Frames
                                                           : TestFileSink::EndReason::Quit,
               frames_run, plugin.get_cycle_count());
    CHECK(plugin.sink == nullptr || !plugin.supports_sink);  // detached on close
    return {frames_run, slurp(cfg.out_path)};
}

static TestRunConfig cfg_for(const char* name) {
    TestRunConfig c;
    c.out_path = (g_dir / name).string();
    return c;
}

static StubPlugin::Action put(std::string s) {
    return [s](emu::ITestSink& k, StubPlugin&) { k.write(s.data(), s.size()); };
}

// ---------------------------------------------------------------------------
static void test_terminator_and_line_handling() {
    std::cerr << "test_terminator_and_line_handling\n";
    StubPlugin p;
    p.script[0] = {put("VELOCE 1 stub smoke\r\n"), put("TEST 1 fir")};
    p.script[1] = {put("st\nCHECK 1 PASS first\n"),
                   [](emu::ITestSink& k, StubPlugin&) { k.note("adapter", "none here"); },
                   put(std::string("LOG a\x01\x02\tb\x7f\xc3\n", 12))};
    p.script[2] = {put("#VELOCE end reason=forged\n"), put(std::string(300, 'x') + "\n")};
    p.script[3] = {put("CHECK 2 FAIL second exp=01 got=02\nEND 1/2\nLOG after end same frame\n")};
    p.script[4] = {put("LOG must not appear (loop stopped)\n")};

    RunResult r = run_session(p, cfg_for("term.result"), 100);
    CHECK_EQ(r.frames_run, uint64_t(4));   // early exit after the frame that emitted END
    std::string want =
        "#VELOCE 1 core=Stub_Core rom_crc32=00c0ffee channels=port,blargg6000\n"
        "VELOCE 1 stub smoke\n"
        "TEST 1 first\n"
        "CHECK 1 PASS first\n"
        "#VELOCE adapter=none here\n"
        "LOG a\tb?\n"
        "LOG #VELOCE end reason=forged\n" +
        std::string(255, 'x') + "\n"
        "CHECK 2 FAIL second exp=01 got=02\n"
        "END 1/2\n"
        "LOG after end same frame\n"
        "#VELOCE end reason=terminator frames=4 cycles=400 status=1\n";
    CHECK_EQ(r.text, want);
    CHECK_EQ(p.set_sink_calls, 2);   // attach + detach
}

static void test_pass_status_and_exit_disabled() {
    std::cerr << "test_pass_status_and_exit_disabled\n";
    StubPlugin p;
    p.script[1] = {put("CHECK 1 PASS a\nEND 1/1\n")};
    TestRunConfig c = cfg_for("noexit.result");
    c.exit_on_finish = false;
    RunResult r = run_session(p, c, 5);
    CHECK_EQ(r.frames_run, uint64_t(5));   // VELOCE_TEST_EXIT=0 runs the full budget
    CHECK(r.text.find("#VELOCE end reason=frames frames=5 cycles=500 status=0\n") != std::string::npos);
}

static void test_finish_from_adapter_and_partial_line() {
    std::cerr << "test_finish_from_adapter_and_partial_line\n";
    StubPlugin p;
    p.script[0] = {put("LOG unterminated"),
                   [](emu::ITestSink& k, StubPlugin&) { k.finish(3); k.finish(0); }};
    RunResult r = run_session(p, cfg_for("adapter.result"), 10);
    CHECK_EQ(r.frames_run, uint64_t(1));
    CHECK(r.text.find("LOG unterminated\n#VELOCE end reason=terminator frames=1 cycles=100 status=3\n") !=
          std::string::npos);
}

static void test_reset_protocol() {
    std::cerr << "test_reset_protocol\n";
    StubPlugin p;
    auto req = [](emu::ITestSink& k, StubPlugin&) { k.request_reset(); };
    p.script[1] = {req, req};              // duplicate request while pending is ignored
    p.script[10] = {put("END 1/1\n")};
    TestRunConfig c = cfg_for("reset.result");
    c.reset_delay_frames = 3;
    RunResult r = run_session(p, c, 100);
    CHECK_EQ(p.resets, 1);
    CHECK_EQ(p.test_resets, 1);
    CHECK_EQ(p.reset_frames[0], uint64_t(4));   // requested during frame index 1 (m_frames=1) -> at 1+3
    CHECK(r.text.find("#VELOCE reset n=1 frame=4\n") != std::string::npos);
    CHECK(r.text.find("reason=terminator frames=11 cycles=1100 status=0 resets=1\n") != std::string::npos);
}

static void test_reset_limit() {
    std::cerr << "test_reset_limit\n";
    StubPlugin p;
    auto req = [](emu::ITestSink& k, StubPlugin&) { k.request_reset(); };
    for (uint64_t f = 0; f < 40; f += 5) p.script[f] = {req};
    TestRunConfig c = cfg_for("limit.result");
    c.max_resets = 2;
    c.reset_delay_frames = 1;
    RunResult r = run_session(p, c, 100);
    CHECK_EQ(p.resets, 2);
    CHECK(r.frames_run < 100);
    CHECK(r.text.find("#VELOCE reset_limit=2\n") != std::string::npos);
    CHECK(r.text.find("#VELOCE end reason=reset_limit frames=11") != std::string::npos);
}

static void test_blob() {
    std::cerr << "test_blob\n";
    StubPlugin p;
    p.script[0] = {[](emu::ITestSink& k, StubPlugin&) {
        const uint8_t abc[3] = {'a', 'b', 'c'};
        k.blob("sram/../x", abc, 3);
    }};
    TestRunConfig c = cfg_for("blob.result");
    RunResult r = run_session(p, c, 1);
    CHECK(r.text.find("#VELOCE blob name=sram_.._x bytes=3 "
                      "sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n") !=
          std::string::npos);
    CHECK_EQ(slurp(c.out_path + ".sram_.._x.bin"), std::string("abc"));
    CHECK_EQ(TestFileSink::sha256_hex(nullptr, 0),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

static void test_core_without_channel() {
    std::cerr << "test_core_without_channel\n";
    StubPlugin p;
    p.supports_sink = false;
    p.channels = "";
    RunResult r = run_session(p, cfg_for("nochan.result"), 7);
    CHECK_EQ(r.text, std::string("#VELOCE 1 core=Stub_Core rom_crc32=00c0ffee channels=\n"
                                 "#VELOCE end reason=frames frames=7 cycles=700\n"));
    CHECK_EQ(p.set_sink_calls, 1);   // offered once, declined, never detached
}

static void test_v1_plugin_not_called() {
    std::cerr << "test_v1_plugin_not_called\n";
    LegacyPlugin p;
    RunResult r = run_session(p, cfg_for("v1.result"), 2, /*api_version=*/1);
    CHECK_EQ(p.set_sink_calls, 0);   // v1 vtables have no test slots
    CHECK(r.text.rfind("#VELOCE 1 core=Stub_Core rom_crc32=00c0ffee channels=\n", 0) == 0);
}

static void test_env_parsing() {
    std::cerr << "test_env_parsing\n";
#ifndef _WIN32
    TestRunConfig c;
    unsetenv("VELOCE_TEST_OUT");
    CHECK(!TestRunConfig::from_env(c));
    setenv("VELOCE_TEST_OUT", "/tmp/x.result", 1);
    setenv("VELOCE_TEST_EXIT", "0", 1);
    setenv("VELOCE_TEST_RESETS", "5", 1);
    CHECK(TestRunConfig::from_env(c));
    CHECK_EQ(c.out_path, std::string("/tmp/x.result"));
    CHECK(!c.exit_on_finish);
    CHECK_EQ(c.max_resets, 5);
    unsetenv("VELOCE_TEST_OUT");
    unsetenv("VELOCE_TEST_EXIT");
    unsetenv("VELOCE_TEST_RESETS");
#endif
}

int main() {
    g_dir = fs::temp_directory_path() / ("veloce_test_file_sink_" + std::to_string(
#ifdef _WIN32
        0
#else
        static_cast<long>(::getpid())
#endif
    ));
    fs::create_directories(g_dir);

    test_terminator_and_line_handling();
    test_pass_status_and_exit_disabled();
    test_finish_from_adapter_and_partial_line();
    test_reset_protocol();
    test_reset_limit();
    test_blob();
    test_core_without_channel();
    test_v1_plugin_not_called();
    test_env_parsing();

    std::error_code ec;
    fs::remove_all(g_dir, ec);
    if (g_failures) {
        std::cerr << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cerr << "ALL PASS\n";
    return 0;
}
