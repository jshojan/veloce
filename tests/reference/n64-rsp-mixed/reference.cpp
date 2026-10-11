// Original adapter; compile with the unchanged, externally fetched primary sources.
#include "rsp_op.hpp"
#include <cstdint>
extern "C" void primary_rsp_mixed(unsigned fn,unsigned element,unsigned vs,unsigned vt,unsigned vd,uint8_t* bank,uint64_t* accumulator){
 RSP::CPUState state{};
 for(unsigned reg=0;reg<32;++reg)for(unsigned lane=0;lane<8;++lane)state.cp2.regs[reg].e[lane]=unsigned(bank[reg*16+lane*2])*256+bank[reg*16+lane*2+1];
 for(unsigned lane=0;lane<8;++lane){state.cp2.acc.e[lane]=accumulator[lane]>>32;state.cp2.acc.e[lane+8]=accumulator[lane]>>16;state.cp2.acc.e[lane+16]=accumulator[lane];}
 switch(fn){case 5:RSP_VMUDM(&state,vd,vs,vt,element);break;case 6:RSP_VMUDN(&state,vd,vs,vt,element);break;case 13:RSP_VMADM(&state,vd,vs,vt,element);break;case 14:RSP_VMADN(&state,vd,vs,vt,element);break;}
 for(unsigned reg=0;reg<32;++reg)for(unsigned lane=0;lane<8;++lane){bank[reg*16+lane*2]=uint8_t(state.cp2.regs[reg].e[lane]>>8);bank[reg*16+lane*2+1]=uint8_t(state.cp2.regs[reg].e[lane]);}
 for(unsigned lane=0;lane<8;++lane)accumulator[lane]=(uint64_t(state.cp2.acc.e[lane])<<32)|(uint64_t(state.cp2.acc.e[lane+8])<<16)|state.cp2.acc.e[lane+16];
}
