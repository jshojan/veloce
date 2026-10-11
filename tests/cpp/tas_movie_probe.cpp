#include "emu/tas_plugin.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv){if(argc!=3)return 2;auto* p=create_tas_plugin();
    if(!p->open_movie(argv[1])){destroy_tas_plugin(p);return 1;}
    std::ofstream output(argv[2]);if(!output){destroy_tas_plugin(p);return 2;}
    output<<p->get_total_frames()<<'\n';
    for(uint64_t i=0;i<p->get_total_frames();++i){auto f=p->get_frame(i);output<<f.frame_number;for(auto input:f.controller_inputs)output<<' '<<input;output<<' '<<f.has_reset<<' '<<f.has_savestate<<' '<<f.savestate_slot<<'\n';}
    destroy_tas_plugin(p);return 0;
}
