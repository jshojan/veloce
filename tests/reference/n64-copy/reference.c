// Original adapter. The pinned renderer is fetched outside the repository.
#include "n64video.c"
void msg_error(const char* text,...){(void)text;}
void msg_warning(const char* text,...){(void)text;}
void msg_debug(const char* text,...){(void)text;}
void primary_copy_initialize(const uint8_t* memory){
    tex_init_lut();rdp_init(&state[0]);
    for(unsigned i=0;i<4096;++i)state[0].tmem[i^3]=memory[i];
}
uint64_t primary_copy_fetch(uint64_t descriptor,uint64_t bounds,unsigned palette,uint16_t s,uint16_t t){
    uint32_t args[2]={(uint32_t)(descriptor>>32),(uint32_t)descriptor};rdp_set_tile(&state[0],args);
    args[0]=bounds>>32;args[1]=bounds;rdp_set_tile_size(&state[0],args);
    state[0].other_modes.en_tlut=palette;
    uint32_t high=0,low=0;fetch_qword_copy(&state[0],&high,&low,s,t,0);
    return ((uint64_t)high<<32)|low;
}
int primary_copy_draw(const uint64_t* commands,unsigned count,uint64_t image,uint64_t mode,uint64_t scissor,const uint64_t* tiles,const uint8_t* memory,unsigned threshold,uint8_t* ram,uint8_t* hidden,unsigned* history){
    _Alignas(uint32_t) static uint8_t backing[65536];static unsigned initialized=0;
    if(!initialized){config.gfx.rdram=backing;config.gfx.rdram_size=sizeof(backing);rdram_init();blender_init_lut();coverage_init_lut();combiner_init_lut();tex_init_lut();z_init_lut();initialized=1;}
    for(unsigned i=0;i<65536;++i)backing[i^3]=ram[i];
    for(unsigned i=0;i<32768;++i)rdram_hidden[i]=hidden[i];
    for(unsigned i=0;i<8;++i)rdram_hidden_old[i]=(*history>>(2*i))&3;
    rdp_init(&state[0]);state[0].stride=1;state[0].rseed=3;
    for(unsigned i=0;i<4096;++i)state[0].tmem[i^3]=memory[i];
    uint32_t args[44]={(uint32_t)(image>>32),(uint32_t)image};rdp_set_color_image(&state[0],args);
    args[0]=mode>>32;args[1]=mode;rdp_set_other_modes(&state[0],args);
    args[0]=scissor>>32;args[1]=scissor;rdp_set_scissor(&state[0],args);
    args[0]=0;args[1]=threshold;rdp_set_blend_color(&state[0],args);
    for(unsigned tile=0;tile<8;++tile){args[0]=tiles[tile*2]>>32;args[1]=(uint32_t)tiles[tile*2]|(tile<<24);rdp_set_tile(&state[0],args);args[0]=tiles[tile*2+1]>>32;args[1]=(uint32_t)tiles[tile*2+1]|(tile<<24);rdp_set_tile_size(&state[0],args);}
    for(unsigned i=0;i<count;++i){args[i*2]=commands[i]>>32;args[i*2+1]=commands[i];}
    rdp_pipeline_crashed=0;rdp_cmd(&state[0],args);
    for(unsigned i=0;i<65536;++i)ram[i]=backing[i^3];
    for(unsigned i=0;i<32768;++i)hidden[i]=rdram_hidden[i];
    *history=0;for(unsigned i=0;i<8;++i)*history|=(rdram_hidden_old[i]&3)<<(2*i);
    return rdp_pipeline_crashed;
}
