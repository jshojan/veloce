// Optional adapter compiled against the pinned external source checkout.
#include "n64video.c"
uint32_t reference_combiner(unsigned a,unsigned b,unsigned c,unsigned d,unsigned* alpha){
 static int initialized=0;if(!initialized){combiner_init_lut();initialized=1;}
 *alpha=alpha_combiner_equation(a,b,c,d);
 return color_combiner_equation(a,b,c,d);
}
unsigned reference_clamp(unsigned raw){return special_9bit_clamptable[raw&511];}
