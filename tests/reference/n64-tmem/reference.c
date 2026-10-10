// Temporary external-reference adapter; compile against the pinned checkout.
#include "n64video.c"

int veloce_reference_load(const uint8_t* ram,uint64_t image,uint64_t descriptor,uint64_t command,uint8_t* output){
 _Alignas(uint32_t) static uint8_t backing[65536];static int initialized=0;
 for(unsigned i=0;i<65536;++i)backing[i^3]=ram[i];
 if(!initialized){config.gfx.rdram=backing;config.gfx.rdram_size=65536;rdram_init();initialized=1;}
 struct rdp_state* s=&state[0];memset(s,0,sizeof(*s));memset(s->tmem,0xa5,4096);
 uint32_t args[2]={(uint32_t)(image>>32),(uint32_t)image};rdp_set_texture_image(s,args);
 args[0]=(uint32_t)(descriptor>>32);args[1]=(uint32_t)descriptor;rdp_set_tile(s,args);
 args[0]=(uint32_t)(command>>32);args[1]=(uint32_t)command;rdp_pipeline_crashed=0;
 switch((command>>56)&63){case 48:rdp_load_tlut(s,args);break;case 51:rdp_load_block(s,args);break;case 52:rdp_load_tile(s,args);break;default:return -1;}
 for(unsigned i=0;i<4096;++i)output[i]=s->tmem[i^3];return rdp_pipeline_crashed;
}
