// Adapter only; compile against separately fetched pinned zbuffer.c.
#include <stdint.h>
#include <stdlib.h>
#define N64VIDEO_C
#define STRICTINLINE inline
#define INLINE inline
#define PAIRREAD16(word,hidden,address) do{(void)(address);word=source_word;hidden=source_hidden;}while(0)
static uint16_t source_word;
static uint8_t source_hidden;
static void rdram_write_pair16(unsigned a,unsigned v,unsigned h,int color){(void)a;(void)v;(void)h;if(color)abort();}
static void msg_error(const char* message){(void)message;abort();}
static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
struct rdp_state {
 struct {int z_compare_en,force_blend,antialias_en,z_mode;struct {int realblendershiftersneeded,interpixelblendershiftersneeded;} f;} other_modes;
 int blshifta,blshiftb,pastblshifta,pastblshiftb,pastrawdzmem;
 unsigned zb_address;
};
#include "zbuffer.c"
void reference_init(void){z_init_lut();}
unsigned reference_compress(unsigned z){return z_com_table[z&0x3ffff];}
unsigned reference_decompress(unsigned word){return z_decompress(word);}
unsigned reference_delta(unsigned delta){return dz_compress(delta);}
unsigned reference_highest(unsigned delta){return deltaz_comparator_lut[delta];}
void reference_compare(unsigned enabled,unsigned mode,unsigned aa,unsigned force,unsigned z,unsigned delta,unsigned word,unsigned hidden,unsigned coverage,unsigned memory,unsigned* out){
 struct rdp_state s={0};s.other_modes.z_compare_en=enabled;s.other_modes.z_mode=mode;s.other_modes.antialias_en=aa;s.other_modes.force_blend=force;s.other_modes.f.realblendershiftersneeded=s.other_modes.f.interpixelblendershiftersneeded=1;
 source_word=(uint16_t)word;source_hidden=(uint8_t)hidden;
 uint32_t blend=0,wrap=0,cvg=coverage;
 out[0]=z_compare(&s,0,z,(uint16_t)delta,dz_compress(delta),&blend,&wrap,&cvg,memory);
 out[1]=blend;out[2]=wrap!=0;out[3]=cvg;out[4]=(unsigned)s.blshifta;out[5]=(unsigned)s.blshiftb;out[6]=(unsigned)s.pastrawdzmem;
}
