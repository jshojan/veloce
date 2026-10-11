// Adapter only. Compile against separately fetched, pinned rdram.c.
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define N64VIDEO_C
#define STRICTINLINE inline
#define RDRAM_MAX_SIZE 32
#define BYTE_ADDR_XOR 3
#define WORD_ADDR_XOR 1
static uint32_t storage[8];
static struct {struct {unsigned rdram_size;void* rdram;} gfx;} config={{32,storage}};
#include "rdram.c"
static int delayed=-1;
void reference_reset(unsigned pattern,unsigned history) {
    rdram_init();delayed=-1;
    for(unsigned n=0;n<16;++n)rdram_hidden[n]=(pattern+n)%4;
    for(unsigned n=0;n<8;++n)rdram_hidden_old[n]=(history+n)%4;
}
unsigned reference_hidden(unsigned address){return rdram_hidden[address/2];}
unsigned reference_history(unsigned lane){return rdram_hidden_old[lane];}
int reference_delayed(void){return delayed;}
void reference_byte(unsigned address,unsigned value,int flip){rdram_write_pair8(address,(uint8_t)value,flip,&delayed);}
void reference_half(unsigned address,unsigned value,int color){rdram_write_pair16(address/2,0,(uint8_t)value,color);}
void reference_word(unsigned address,unsigned first,unsigned second){rdram_write_pair32(address/4,0,(uint8_t)first,(uint8_t)second);}
void reference_finish(void){rdram_complete_delayed_hbwrites(delayed);delayed=-1;}
