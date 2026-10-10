#include "bus.hpp"
#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static uint16_t quotient(snes::Bus& bus) { return bus.read_cpu_io(0x4214) | (bus.read_cpu_io(0x4215) << 8); }
static uint16_t product(snes::Bus& bus) { return bus.read_cpu_io(0x4216) | (bus.read_cpu_io(0x4217) << 8); }
static void edges(snes::Bus& bus, unsigned count) { while (count--) bus.clock_math(); }
static void multiply(snes::Bus& bus, unsigned a, unsigned b) {
    bus.write_cpu_io(0x4202, a); bus.write_cpu_io(0x4203, b);
}
static void divide(snes::Bus& bus, unsigned a, unsigned b) {
    bus.write_cpu_io(0x4204, a & 255); bus.write_cpu_io(0x4205, a >> 8); bus.write_cpu_io(0x4206, b);
}
int main(int argc, char** argv) {
    try {
        snes::Bus bus;
        size_t observations = 0;
        for (unsigned a = 0; a < 256; ++a) for (unsigned b = 0; b < 256; ++b) {
            multiply(bus, a, b);
            for (unsigned phase = 0; phase <= 8; ++phase) {
                unsigned scale = 1u << phase;
                require(quotient(bus) == (b * 256 + a) / scale && product(bus) == b * (a % scale),
                        "multiply pipeline phase mismatch");
                ++observations;
                bus.clock_math();
            }
        }
        const unsigned dividends[] = {0,1,2,127,128,254,255,256,257,32767,32768,65534,65535};
        for (auto a : dividends) for (unsigned b = 0; b < 256; ++b) {
            multiply(bus, 0, 0xA5); edges(bus, 8);
            divide(bus, a, b);
            for (unsigned phase = 0; phase <= 16; ++phase) {
                unsigned scale = 1u << (16 - phase);
                unsigned prefix = b ? a / (b * scale) : (1u << phase) - 1;
                unsigned remainder = b ? a % (b * scale) : a;
                require(quotient(bus) == ((0xA5u << phase) & 65535u) + prefix && product(bus) == remainder,
                        "divide pipeline phase mismatch");
                ++observations;
                bus.clock_math();
            }
        }
        // Preserve latches, shift state, phase, and outputs through every save
        // point. Resume both buses, including a later operation using saved A.
        for (bool dividing : {false, true}) for (unsigned phase = 0; phase <= (dividing ? 16u : 8u); ++phase) {
            bus.cancel_math();
            multiply(bus, 0, 0xA5); edges(bus, 8);
            if (dividing) divide(bus, 0xFEDC, 0x81); else multiply(bus, 0xEF, 0x93);
            edges(bus, phase);
            std::vector<uint8_t> state;
            bus.save_state(state);
            snes::Bus clone;
            const uint8_t* pointer = state.data(); size_t remaining = state.size();
            clone.load_state(pointer, remaining);
            require(remaining == 0, "bus state did not consume its own serialization");
            for (unsigned clock = 0; clock < 20; ++clock) {
                require(quotient(bus) == quotient(clone) && product(bus) == product(clone), "pending math state diverged");
                bus.clock_math(); clone.clock_math();
            }
            bus.write_cpu_io(0x4203, 7); clone.write_cpu_io(0x4203, 7);
            edges(bus, 8); edges(clone, 8);
            require(quotient(bus) == quotient(clone) && product(bus) == product(clone), "operand latch not restored");
        }
        bus.cancel_math();
        multiply(bus, 255, 255); edges(bus, 3);
        bus.write_cpu_io(0x4203, 1); // Clear product, ignore replacement trigger.
        edges(bus, 5);
        require(quotient(bus) == 255 && product(bus) == 255 * 248, "busy multiply trigger mishandled");
        multiply(bus, 0, 0xA5); edges(bus, 8);
        divide(bus, 65535, 255); edges(bus, 5);
        bus.write_cpu_io(0x4204, 0x34); bus.write_cpu_io(0x4205, 0x12);
        bus.write_cpu_io(0x4206, 1); // Replace shared remainder, keep old divisor/phase.
        edges(bus, 11);
        require(quotient(bus) == 0x12 && product(bus) == 0x46, "busy division trigger mishandled");
        multiply(bus, 255, 255); edges(bus, 3);
        bus.write_cpu_io(0x4204, 0x34); bus.write_cpu_io(0x4205, 0x12);
        bus.write_cpu_io(0x4206, 1);
        edges(bus, 5);
        require(quotient(bus) == 255 && product(bus) == static_cast<uint16_t>(0x1234 + 255 * 248),
                "busy division trigger replaced multiplication");
        multiply(bus, 0, 0xA5); edges(bus, 8);
        divide(bus, 65535, 255); edges(bus, 5);
        bus.write_cpu_io(0x4203, 1); edges(bus, 11);
        require(quotient(bus) == 0 && product(bus) == 0, "busy multiply trigger replaced division");
        multiply(bus, 255, 255); edges(bus, 2);
        bus.cancel_math();
        auto q = quotient(bus), p = product(bus);
        edges(bus, 40);
        require(quotient(bus) == q && product(bus) == p, "cancelled pipeline continued");
        std::vector<uint8_t> state; bus.save_state(state); state.pop_back();
        const uint8_t* pointer = state.data(); size_t remaining = state.size();
        bool rejected = false;
        try { bus.load_state(pointer, remaining); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "truncated bus state accepted");
        require(argc == 3, "expected SNES shared core and original ROM paths");
        std::ifstream rom_file(argv[2], std::ios::binary);
        std::vector<uint8_t> rom{std::istreambuf_iterator<char>(rom_file), {}};
        emulator_test::LoadedCore fixture(argv[1], rom);
        auto& core = *fixture.plugin;
        std::vector<uint8_t> saved, baseline, restored;
        for (unsigned i = 0; i < 2; ++i) core.run_frame({0});
        require(core.save_state(saved) && saved.size() > 24 &&
                std::string(saved.begin(), saved.begin() + 8) == "VSNES002", "missing math state format version");
        auto frame = core.get_frame_count();
        auto bad = saved; bad.pop_back();
        require(!core.load_state(bad) && core.get_frame_count() == frame, "truncated plugin state accepted/mutated core");
        bad.assign(saved.begin() + 8, saved.end());
        require(!core.load_state(bad) && core.get_frame_count() == frame, "legacy plugin state accepted/mutated core");
        bad = saved; bad[7] = '1';
        require(!core.load_state(bad), "wrong state version accepted");
        for (unsigned i = 0; i < 5; ++i) core.run_frame({0});
        require(core.save_state(baseline) && core.load_state(saved), "plugin state roundtrip failed");
        for (unsigned i = 0; i < 5; ++i) core.run_frame({0});
        require(core.save_state(restored) && restored == baseline, "full SNES state diverged after resume");
        std::cout << "SNES math pipeline: " << observations << " phase snapshots, save/resume, busy writes, and cancellation passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << "\n"; return 1; }
}
