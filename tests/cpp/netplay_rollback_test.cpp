#include "rollback_session.hpp"
#include "nes_plugin_fixture.hpp"
#include <SDL.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

using Session = emu::RollbackSession;
using Result = Session::Result;
static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

class TestCore : public emu::INetplayCapable {
public:
    uint64_t state = 1;
    int discarded = 0;
    bool fail_save = false, fail_load = false;
    void run_frame_netplay(uint32_t a, uint32_t b) override { state = state * 16777619 ^ (a << 1) ^ b; }
    size_t get_max_state_size() const override { return sizeof(state); }
    size_t save_state_fast(uint8_t* out, size_t size) override {
        if (fail_save || size < sizeof(state)) return 0;
        std::memcpy(out, &state, sizeof(state));
        return sizeof(state);
    }
    bool load_state_fast(const uint8_t* in, size_t size) override {
        if (fail_load || size != sizeof(state)) return false;
        std::memcpy(&state, in, sizeof(state));
        return true;
    }
    uint64_t get_state_hash() const override { return state; }
    void discard_audio() override { ++discarded; }
};

static void edge_cases() {
    TestCore core;
    Session s(core, 0, 3, 100);
    for (int i = 0; i < 3; ++i) require(s.advance(5) == Result::Advanced, "prediction stopped too early");
    auto before = core.state;
    require(s.advance(5) == Result::Waiting && core.state == before && s.next_frame() == 103,
            "speculation limit advanced or changed the core");
    require(s.submit_remote(99, 7) == Result::TooOld, "accepted pre-session input");
    require(s.submit_remote(107, 7) == Result::TooFarAhead, "accepted unbounded future input");
    require(s.submit_remote(100, 7) == Result::Accepted && s.last_rollback_depth() == 3,
            "oldest speculative frame did not rewind");
    require(core.discarded == 3, "replay audio was not discarded once per frame");
    require(s.submit_remote(100, 7) == Result::Accepted, "duplicate confirmation rejected");
    before = core.state;
    require(s.submit_remote(100, 8) == Result::Conflict && core.state == before,
            "conflicting confirmation changed the core");
    require(s.advance(9) == Result::Advanced, "confirmation did not resume stalled session");
    require(s.submit_remote(100, 7) == Result::TooOld, "expired frame accepted");
    require(s.submit_remote(104, 1) == Result::Accepted, "future confirmation rejected");
    require(s.submit_remote(104, 2) == Result::Conflict, "future confirmation overwritten");
    core.fail_load = true;
    require(s.submit_remote(101, 4) == Result::Failed && s.failed() && !s.is_replaying(),
            "load failure did not stop session or release audio gate");
    require(s.advance(0) == Result::Failed, "failed session continued");
    TestCore broken;
    broken.fail_save = true;
    Session bad(broken, 0, 2);
    require(bad.advance(0) == Result::Failed && broken.state == 1, "save failure ran an unsaved frame");
    bool rejected = false;
    try { Session invalid(broken, 2, 0); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid session settings accepted");
}

// Deliver remote inputs 3/5 frames late, then drain the final pending messages
// in reverse order. Each peer uses a different local slot; baseline has no delay.
static void run_delayed(emu::INetplayCapable& baseline, emu::INetplayCapable& a,
                        emu::INetplayCapable& b) {
    Session peer0(a, 0, 8), peer1(b, 1, 8);
    auto* output0 = dynamic_cast<emu::IEmulatorPlugin*>(&a);
    auto* output1 = dynamic_cast<emu::IEmulatorPlugin*>(&b);
    auto* reference = dynamic_cast<emu::IEmulatorPlugin*>(&baseline);
    size_t suppressed0 = 0, suppressed1 = 0, played0 = 0, played1 = 0;
    struct AudioReset {
        emu::IEmulatorPlugin *a, *b, *reference;
        ~AudioReset() {
            for (auto* core : {a, b, reference}) if (core) core->set_audio_callback(nullptr);
        }
    } audio_reset{output0, output1, reference};
    if (reference) reference->set_audio_callback([](const float*, size_t, int) {});
    if (output0) output0->set_audio_callback([&](const float*, size_t count, int) {
        // Same gate the frontend must apply before submitting samples to audio output.
        (peer0.is_replaying() ? suppressed0 : played0) += count;
    });
    if (output1) output1->set_audio_callback([&](const float*, size_t count, int) {
        (peer1.is_replaying() ? suppressed1 : played1) += count;
    });
    auto p0 = [](int frame) { return static_cast<uint32_t>((frame * 7 + 3) & 255); };
    auto p1 = [](int frame) { return static_cast<uint32_t>((frame * 13 + 1) & 255); };
    for (int frame = 0; frame < 100; ++frame) {
        baseline.run_frame_netplay(p0(frame), p1(frame));
        if (frame >= 3) require(peer0.submit_remote(frame - 3, p1(frame - 3)) == Result::Accepted,
                               "peer0 delayed input failed");
        if (frame >= 5) require(peer1.submit_remote(frame - 5, p0(frame - 5)) == Result::Accepted,
                               "peer1 delayed input failed");
        require(peer0.advance(p0(frame)) == Result::Advanced, "peer0 stalled within window");
        require(peer1.advance(p1(frame)) == Result::Advanced, "peer1 stalled within window");
    }
    for (int frame = 99; frame >= 97; --frame)
        require(peer0.submit_remote(frame, p1(frame)) == Result::Accepted, "peer0 reordered input failed");
    for (int frame = 99; frame >= 95; --frame)
        require(peer1.submit_remote(frame, p0(frame)) == Result::Accepted, "peer1 reordered input failed");
    require(peer0.next_frame() == 100 && peer1.next_frame() == 100, "replay changed session frame");
    require(peer0.rollback_count() > 0 && peer1.rollback_count() > 0, "test did not exercise rollback");
    require(a.get_state_hash() == baseline.get_state_hash() && b.get_state_hash() == baseline.get_state_hash(),
            "delayed peers did not converge on baseline hash");
    if (output0) require(suppressed0 > 0 && played0 > 0, "peer0 streaming audio gate unexercised");
    if (output1) require(suppressed1 > 0 && played1 > 0, "peer1 streaming audio gate unexercised");
    require(!peer0.is_replaying() && !peer1.is_replaying(), "replay audio gate stuck active");
}

using emulator_test::LoadedCore;
using emulator_test::snapshot;

int main(int argc, char** argv) {
    try {
        edge_cases();
        TestCore reference, a, b;
        run_delayed(reference, a, b);
        require(a.discarded > 0 && b.discarded > 0, "delayed test did not discard replay audio");
        require(argc == 4, "expected plugin path, ROM path, and artifact directory");
        std::ifstream rom_file(argv[2], std::ios::binary);
        require(rom_file.good(), "original ROM file missing");
        std::vector<uint8_t> rom{std::istreambuf_iterator<char>(rom_file), {}};
        LoadedCore baseline(argv[1], rom), peer0(argv[1], rom), peer1(argv[1], rom), neutral(argv[1], rom);
        run_delayed(baseline.netplay(), peer0.netplay(), peer1.netplay());
        auto state = snapshot(baseline.netplay());
        require(snapshot(peer0.netplay()) == state && snapshot(peer1.netplay()) == state,
                "NES full serialized states diverged despite matching hash");
        for (int i = 0; i < 100; ++i) neutral.netplay().run_frame_netplay(0, 0);
        require(snapshot(neutral.netplay()) != state, "original ROM ignored controller inputs");
        const std::filesystem::path artifacts(argv[3]);
        std::filesystem::create_directories(artifacts);
        std::ofstream observations(artifacts / "registers.result");
        observations << "#VELOCE ROLLBACK 1 core=NES frames=100\n";
        for (auto item : {std::pair<const char*, LoadedCore*>{"baseline", &baseline},
                          {"peer0", &peer0}, {"peer1", &peer1}}) {
            auto bytes = snapshot(item.second->netplay());
            std::ofstream file(artifacts / (std::string(item.first) + ".state"), std::ios::binary);
            file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            file.flush();
            require(file.good(), "could not write full-state artifact");
            observations << "REG " << item.first << "_p0_sum=" << unsigned(item.second->plugin->read_memory(0x12))
                         << " " << item.first << "_p1_sum=" << unsigned(item.second->plugin->read_memory(0x13))
                         << " hash=" << item.second->netplay().get_state_hash() << "\n";
        }
        observations.flush();
        require(observations.good(), "could not write register observations");
        std::cout << "Rollback: window/confirmation/error cases and two delayed NES peers converge "
                     "on 100-frame baseline; full states and guest registers recorded\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
