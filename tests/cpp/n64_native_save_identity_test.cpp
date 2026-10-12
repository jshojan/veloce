#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using emulator_test::require;

struct Sink final : emu::ITestSink {
    std::string text;
    bool done = false;
    void write(const char* bytes, size_t size) override { text.append(bytes, size); }
    void finish(int32_t status) override { require(status == -1, "native stub termination"); done = true; }
    void note(const char*, const char*) override { require(false, "unexpected guest note"); }
    void blob(const char*, const uint8_t*, size_t) override { require(false, "unexpected guest blob"); }
    void request_reset() override { require(false, "unexpected guest reset"); }
};

int main(int argc, char** argv) {
    require(argc == 4, "plugin, original native cartridge and register file required");
    std::ifstream stream(argv[2], std::ios::binary);
    const std::vector<uint8_t> image{std::istreambuf_iterator<char>(stream), {}};
    require(image.size() == 0x1004, "original native cartridge extent");
    emulator_test::LoadedCore core(argv[1], image);
    require(core.plugin->has_battery_save() &&
            core.plugin->get_battery_save_data().size() == 2048,
            "NB7 native header did not select 16 Kbit");
    const std::filesystem::path artifact(argv[3]);
    std::filesystem::create_directories(artifact.parent_path());
    std::ofstream registers(artifact, std::ios::trunc);
    require(bool(registers), "could not open native guest register file");
    unsigned cases = 0;
    auto observe = [&](const std::string& label, uint8_t type) {
        Sink sink;
        require(core.plugin->set_test_sink(&sink), "native guest test channel");
        core.plugin->run_frame({});
        require(sink.done && sink.text.size() == 1 &&
                uint8_t(sink.text[0]) == type,
                "native guest SI status did not match selected EEPROM");
        const auto guest_r10 = uint8_t(sink.text[0]);
        registers << "REG case=" << label << " r10=0x" << std::hex
                  << unsigned(guest_r10) << std::dec << " battery_bytes="
                  << core.plugin->get_battery_save_data().size() << '\n';
        require(bool(registers), "native guest register file write failed");
        ++cases;
        core.plugin->set_test_sink(nullptr);
    };
    observe("NB7-base", 0xc0);

    constexpr std::array<const char*, 23> ids = {
        "NB7", "NGT", "NFU", "NCW", "NCZ", "ND6", "NDO", "ND2", "N3D", "NMX",
        "NGC", "NIM", "NNB", "NMV", "NM8", "NEV", "NPP", "NUB", "NPD", "NRZ",
        "NR7", "NEP", "NYS"};
    for (const char* id : ids) {
        auto variant = image;
        for (unsigned n = 0; n < 3; ++n) variant.at(0x3b + n) = uint8_t(id[n]);
        core.fresh_load(variant);
        const auto battery = core.plugin->get_battery_save_data();
        require(battery.size() == 2048 &&
                std::all_of(battery.begin(), battery.end(), [](uint8_t byte) { return byte == 0xff; }),
                "known 16 Kbit native ID did not select erased 2 KiB media");
        require(!core.plugin->set_battery_save_data(std::vector<uint8_t>(512)),
                "known 16 Kbit ID accepted a 512-byte battery file");
        observe(id, 0xc0);
    }
    for (const char* id : {"NSM", "NKT", "NZZ", "NB8"}) {
        auto variant = image;
        for (unsigned n = 0; n < 3; ++n) variant.at(0x3b + n) = uint8_t(id[n]);
        core.fresh_load(variant);
        require(core.plugin->get_battery_save_data().size() == 512,
                "non-16 Kbit ID changed the fallback profile");
        observe(id, 0x80);
    }
    for (unsigned kind = 0; kind < 2; ++kind) {
        auto variant = image;
        if (kind == 0)
            for (size_t at = 0; at < variant.size(); at += 2)
                std::swap(variant[at], variant[at + 1]);
        else
            for (size_t at = 0; at < variant.size(); at += 4) {
                std::swap(variant[at], variant[at + 3]);
                std::swap(variant[at + 1], variant[at + 2]);
            }
        core.fresh_load(variant);
        require(core.plugin->get_battery_save_data().size() == 2048,
                "byte-swapped native header lost media identity");
        observe(kind == 0 ? "NB7-v64" : "NB7-n64", 0xc0);
    }
    registers.flush();
    require(cases == 30 && bool(registers), "native register file incomplete");
    std::cout << "23 native 16 Kbit IDs, four 4 Kbit controls and two endian forms returned correct guest SI EEPROM identity PASS\n";
}
