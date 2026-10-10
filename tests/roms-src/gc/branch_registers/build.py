#!/usr/bin/env python3
"""Original conditional branch observations; no expected answers or verdicts."""
import argparse
from pathlib import Path
CONSOLE='gc'
EXT='vgc'
def domain(short=False):
    options=(0,4,12,16,20) if short else tuple(n for n in range(32) if n in (0,1,2,3,4,5,8,9,10,11,12,13,16,17,18,19,20))
    for family in ('bc','bclr','bcctr'):
        for bo in options:
            if family=='bcctr' and not bo&4: continue
            for bi in ((0,31) if short else range(32)):
                for cr_index,cr in enumerate((0,0xffffffff) if short else (0,0xffffffff,0xa5a5a5a5,0x5a5a5a5a)):
                    values=range(4) if family=='bcctr' else ((0,1) if short else (0,1,2,3,0x7fffffff,0x80000000,0xfffffffe,0xffffffff))
                    for counter_index,counter in enumerate(values):
                        for link in range(2): yield family,bo,bi,cr_index,cr,counter_index,counter,link

def build(short=False,wrong_result=False,wrong_condition=False,wrong_link=False):
    code=bytearray()
    def emit(word): code.extend(word.to_bytes(4,'big'))
    def const(reg,value):
        emit(15<<26|reg<<21|((value>>16)&65535)); emit(24<<26|reg<<21|reg<<16|(value&65535))
    def spr(reg,number,write=False): emit(31<<26|reg<<21|(number&31)<<16|(number&992)<<6|(467 if write else 339)<<1)
    def text(value):
        for char in value.encode('ascii'): emit(14<<26|char); emit(38<<26|12<<16)
    cases=sum(1 for _ in domain(short))
    const(12,0xfff00000); const(6,0x12345678)
    text(f'VELOCE 1 {CONSOLE} branch_registers\nBEGIN branches cases={cases} words=7\n')
    for family,bo,bi,cr_index,cr,counter_index,counter,link in domain(short):
        branch_pc=0x80000000+len(code)+64
        lane=counter_index%4
        const(3,(branch_pc+8)|lane if family=='bcctr' else counter); spr(3,9,True)
        const(4,cr); emit(31<<26|4<<21|255<<12|144<<1)
        const(4,((counter_index+cr_index+bo)%8)<<29|127); spr(4,1,True)
        const(7,(branch_pc+8)|lane if family=='bclr' else 0x1234567b); spr(7,8,True)
        emit(14<<26|5<<21)
        # Discover actual guest PC through BCL + MFLR, then restore the LR input.
        emit(0x42800005); spr(9,8); spr(7,8,True)
        assert 0x80000000+len(code)==branch_pc
        encoded_bo=bo^8 if wrong_condition and not bo&16 else bo
        encoded_link=0 if wrong_link else link
        if family=='bc': emit(16<<26|encoded_bo<<21|bi<<16|8|encoded_link)
        else: emit(19<<26|encoded_bo<<21|bi<<16|(16 if family=='bclr' else 528)<<1|encoded_link)
        emit(18<<26|8); emit(14<<26|5<<21|1)
        if wrong_result: emit(14<<26|5<<21|5<<16|1)
        spr(7,8); spr(8,9); emit(31<<26|10<<21|19<<1); spr(11,1)
        text('REG r=')
        for reg in (5,7,8,10,11,9,6): emit(36<<26|reg<<21|12<<16|8)
        text('\n')
    text(f'LOG complete cases={cases}\nEND observations_complete\n')
    emit(36<<26|12<<16|4); emit(18<<26)
    header=bytearray(b'VGCP0001')
    for value in (0x80000000,0x80000000,len(code),0,0,0): header.extend(value.to_bytes(4,'big'))
    return bytes(header+code)
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('branch_registers.'+EXT))
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(build())
