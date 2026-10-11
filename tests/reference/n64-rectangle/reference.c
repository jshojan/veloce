// Original functional adapter; the pinned renderer stays outside this repository.
#include "n64video.c"
void msg_error(const char* text,...){(void)text;}
void msg_warning(const char* text,...){(void)text;}
void msg_debug(const char* text,...){(void)text;}
int primary_rectangle_draw(const uint64_t* commands,unsigned count,uint64_t image,uint64_t mode,uint64_t scissor,const uint64_t* tiles,const uint8_t* memory,const uint64_t* pixel,unsigned* memory_rgb,uint8_t* ram,uint8_t* hidden,unsigned* history){
    _Alignas(uint32_t) static uint8_t backing[65536];static unsigned initialized=0;
    if(!initialized){config.gfx.rdram=backing;config.gfx.rdram_size=sizeof(backing);rdram_init();blender_init_lut();coverage_init_lut();combiner_init_lut();tex_init_lut();z_init_lut();initialized=1;}
    for(unsigned i=0;i<65536;++i)backing[i^3]=ram[i];
    for(unsigned i=0;i<32768;++i)rdram_hidden[i]=hidden[i];
    for(unsigned i=0;i<8;++i)rdram_hidden_old[i]=(*history>>(2*i))&3;
    rdp_init(&state[0]);state[0].stride=1;state[0].rseed=3;state[0].memory_color.r=*memory_rgb>>16;state[0].memory_color.g=(*memory_rgb>>8)&255;state[0].memory_color.b=*memory_rgb&255;
    for(unsigned i=0;i<4096;++i)state[0].tmem[i^3]=memory[i];
    uint32_t args[44]={(uint32_t)(image>>32),(uint32_t)image};rdp_set_color_image(&state[0],args);
    args[0]=mode>>32;args[1]=mode;rdp_set_other_modes(&state[0],args);
    args[0]=scissor>>32;args[1]=scissor;rdp_set_scissor(&state[0],args);
    args[0]=pixel[0]>>32;args[1]=pixel[0];rdp_set_combine(&state[0],args);
    args[0]=pixel[1]>>32;args[1]=pixel[1];rdp_set_prim_color(&state[0],args);
    args[0]=0;args[1]=pixel[2];rdp_set_env_color(&state[0],args);
    if(pixel[3]&(1ull<<54)){args[0]=pixel[3]>>32;args[1]=pixel[3];rdp_set_convert(&state[0],args);}
    args[0]=0;args[1]=pixel[4];rdp_set_fog_color(&state[0],args);
    args[1]=pixel[5];rdp_set_blend_color(&state[0],args);
    args[1]=pixel[6];rdp_set_mask_image(&state[0],args);
    args[1]=pixel[7];rdp_set_prim_depth(&state[0],args);
    for(unsigned tile=0;tile<8;++tile){args[0]=tiles[tile*2]>>32;args[1]=(uint32_t)tiles[tile*2]|(tile<<24);rdp_set_tile(&state[0],args);args[0]=tiles[tile*2+1]>>32;args[1]=(uint32_t)tiles[tile*2+1]|(tile<<24);rdp_set_tile_size(&state[0],args);}
    for(unsigned i=0;i<count;++i){args[i*2]=commands[i]>>32;args[i*2+1]=commands[i];}
    rdp_pipeline_crashed=0;rdp_cmd(&state[0],args);
    for(unsigned i=0;i<65536;++i)ram[i]=backing[i^3];
    for(unsigned i=0;i<32768;++i)hidden[i]=rdram_hidden[i];
    *history=0;for(unsigned i=0;i<8;++i)*history|=(rdram_hidden_old[i]&3)<<(2*i);
    *memory_rgb=(state[0].memory_color.r<<16)|(state[0].memory_color.g<<8)|state[0].memory_color.b;
    return rdp_pipeline_crashed;
}
