#include "nes_plugin_fixture.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <cmath>
int main(int argc,char** argv) {
 try {
    using emulator_test::require;require(argc==4,"plugin, ROM, register file required");
    std::ifstream input(argv[2],std::ios::binary);require(input.good(),"ROM missing");
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(input),{}};emulator_test::LoadedCore core(argv[1],rom);
    std::ofstream output(argv[3]);require(output.good(),"capture file unavailable");
    output<<"#VELOCE 1 core=N64 rom_crc32="<<std::hex<<std::setw(8)<<std::setfill('0')<<core.plugin->get_rom_crc32()<<" channels=abi-audio\n";
    uint64_t observations=0;std::vector<uint8_t> state;
    for(unsigned frame=0;frame<4000;++frame) {
        core.plugin->run_frame({});const auto audio=core.plugin->get_audio();require(audio.sample_rate==48000 && audio.sample_count>=0,"audio format");
        require(!audio.sample_count || audio.samples,"null audio");
        for(int n=0;n<audio.sample_count;++n) {
            output<<"REG r=";
            for(unsigned channel=0;channel<2;++channel) {const float sample=audio.samples[n*2+channel];require(std::isfinite(sample) && sample>=-1 && sample<1,"invalid sample");const int raw=int(std::lround(sample*32768));output<<std::setw(4)<<uint16_t(raw);}
            output<<'\n';++observations;
        }
        core.plugin->clear_audio_buffer();require(core.plugin->save_state(state) && state.size()>39,"snapshot");
        if(state[39]) {output<<"LOG complete pairs="<<std::dec<<observations<<"\nEND observations_complete\n";require(output.good(),"capture failed");return 0;}
    }
    throw std::runtime_error("original audio ROM did not finish");
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
