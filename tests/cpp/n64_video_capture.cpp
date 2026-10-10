#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
int main(int argc,char** argv) {
 try {
    using emulator_test::require;require(argc==4,"plugin, ROM, pixel register file required");
    std::ifstream file(argv[2],std::ios::binary);require(file.good(),"ROM missing");
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(file),{}};emulator_test::LoadedCore core(argv[1],rom);
    std::ofstream output(argv[3]);require(output.good(),"capture file unavailable");
    output<<"#VELOCE 1 core=N64 rom_crc32="<<std::hex<<std::setw(8)<<std::setfill('0')<<core.plugin->get_rom_crc32()<<" channels=abi-video\n";
    unsigned frames=0;uint64_t pixels=0;std::vector<uint8_t> before,after;
    for(unsigned quantum=0;quantum<2000;++quantum) {
        core.plugin->run_frame({});require(core.plugin->save_state(before) && before.size()>640,"state unavailable");
        if(before[639]) {
            const auto video=core.plugin->get_framebuffer();require(video.pixels && video.width>0 && video.height>0 && video.width<=1023 && video.height<=512,"invalid pixel dimensions");
            require(core.plugin->save_state(after) && after==before,"video observation changed guest state");
            const unsigned mode=before[before.size()-1496+7]&3;
            output<<"FRAME index="<<std::dec<<frames<<" mode="<<mode<<" width="<<video.width<<" height="<<video.height<<"\n";
            const auto* bytes=reinterpret_cast<const uint8_t*>(video.pixels);
            for(size_t n=0;n<size_t(video.width)*video.height;++n) {
                output<<"REG r="<<std::hex;
                for(unsigned channel=0;channel<4;++channel)output<<std::setw(2)<<unsigned(bytes[n*4+channel]);
                output<<'\n';++pixels;
            }
            ++frames;
        }
        if(before[39]) {output<<"LOG complete frames="<<std::dec<<frames<<" pixels="<<pixels<<"\nEND observations_complete\n";require(output.good(),"capture output failed");return 0;}
    }
    throw std::runtime_error("original framebuffer cartridge did not finish");
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
