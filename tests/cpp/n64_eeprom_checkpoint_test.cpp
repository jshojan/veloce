#include "nes_plugin_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>

using emulator_test::require;

int main(int argc, char** argv) {
    require(argc == 3 || argc == 4, "core and original EEPROM cartridge required");
    const size_t battery_bytes = argc == 4 ? 2048 : 512;
    std::ifstream stream(argv[2], std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(stream), {}};
    emulator_test::LoadedCore core(argv[1], rom);
    auto& net = core.netplay();
    require(core.plugin->has_battery_save(), "marked cartridge EEPROM capability");
    const auto blank = core.plugin->get_battery_save_data();
    require(blank.size() == battery_bytes && std::all_of(blank.begin(), blank.end(),
            [](uint8_t value) { return value == 0xff; }), "erased battery bytes");
    for (unsigned frame = 0; frame < 5; ++frame) net.run_frame_netplay(0, 0);
    const auto checkpoint = emulator_test::snapshot(net);
    uint64_t profile = 0;
    for (auto at = checkpoint.end() - 8; at != checkpoint.end(); ++at)
        profile = (profile << 8) | *at;
    require(profile == battery_bytes, "S033 EEPROM media size missing");
    auto invalid_profile = checkpoint;
    invalid_profile.back() ^= 1;
    require(!net.load_state_fast(invalid_profile.data(), invalid_profile.size()) &&
            emulator_test::snapshot(net) == checkpoint,
            "invalid S033 media size changed active state");
    const auto battery = core.plugin->get_battery_save_data();
    require(battery.size() == battery_bytes && battery != blank,
            "guest did not write EEPROM before checkpoint");
    for (unsigned frame = 0; frame < 5; ++frame) net.run_frame_netplay(0, 0);
    const auto later = emulator_test::snapshot(net);
    const auto later_battery = core.plugin->get_battery_save_data();
    require(later != checkpoint && later_battery != battery,
            "guest did not advance EEPROM after checkpoint");
    require(net.load_state_fast(checkpoint.data(), checkpoint.size()) &&
            core.plugin->get_battery_save_data() == battery,
            "rollback did not restore EEPROM");
    for (unsigned frame = 0; frame < 5; ++frame) net.run_frame_netplay(0, 0);
    require(emulator_test::snapshot(net) == later &&
            core.plugin->get_battery_save_data() == later_battery,
            "guest EEPROM writes were not deterministic after rollback");
    require(!core.plugin->set_battery_save_data({1}) &&
            core.plugin->get_battery_save_data() == later_battery,
            "malformed battery setter mutated EEPROM");
    core.plugin->reset();
    require(core.plugin->get_battery_save_data() == later_battery,
            "soft reset erased EEPROM");
    core.fresh_load(rom);
    require(core.plugin->get_battery_save_data() == blank,
            "new cartridge load retained previous EEPROM");
    std::cout << "N64 original guest EEPROM checkpoint, rollback, battery API and reset PASS\n";
}
