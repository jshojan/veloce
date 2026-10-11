#include <stdint.h>
#define N64VIDEO_C
#define STRICTINLINE inline
struct rdp_state {struct {struct {int getditherlevel,rgb_alpha_dither;} f;} other_modes;int32_t noise,rseed;int scfield;};
/* Unused for the ordered selector combinations exposed below. */
static uint32_t irand(int32_t* state){return (uint32_t)(*state);}
#include "dither.c"
void reference_dither_seeds(unsigned rgb,unsigned alpha,unsigned x,unsigned y,unsigned field,unsigned* out){
 struct rdp_state state={0};state.other_modes.f.getditherlevel=1;state.other_modes.f.rgb_alpha_dither=(int)(rgb*4+alpha);state.scfield=(int)field;
 int c=0,a=0;get_dither_noise(&state,(int)x,(int)y,&c,&a);out[0]=(unsigned)c;out[1]=(unsigned)a;
}
unsigned reference_dither_channel(unsigned color,unsigned seed){int r=(int)color,g=r,b=r;rgb_dither(0,&r,&g,&b,(int)seed);return (unsigned)g;}
