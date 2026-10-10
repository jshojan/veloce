#!/usr/bin/env python3
"""Original PowerPC instructions; guest emits observations without expected answers."""
import argparse
from pathlib import Path
CONSOLE = "wii"
EXT = "vwi"
PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
OPS=(266,10,138,40,8,136)
def build(short=False,wrong_result=False):
    code=bytearray()
    def emit(word): code.extend(word.to_bytes(4,'big'))
    def const(reg,value):
        emit(15<<26|reg<<21|((value>>16)&65535))
        emit(24<<26|reg<<21|reg<<16|(value&65535))
    def text(value):
        for char in value.encode('ascii'):
            emit(14<<26|char); emit(38<<26|12<<16)
    def spr(reg,number,write=False): emit(31<<26|reg<<21|(number&31)<<16|(number&992)<<6|(467 if write else 339)<<1)
    values=PATTERNS[:2] if short else PATTERNS
    cases=len(OPS)*len(values)**2*8*4
    const(12,0xfff00000); const(6,0x12345678)
    text(f'VELOCE 1 {CONSOLE} integer_registers\nBEGIN arithmetic cases={cases} words=6\n')
    for xo in OPS:
        for a in values:
            for b in values:
                for flags in range(8):
                    for oe in range(2):
                        for rc in range(2):
                            const(3,a); const(4,b); const(9,flags<<29|127); spr(9,1,True)
                            const(9,0xa5a5a5a5); emit(31<<26|9<<21|255<<12|144<<1)
                            emit(31<<26|5<<21|3<<16|4<<11|oe<<10|xo<<1|rc)
                            if wrong_result: emit(14<<26|5<<21|5<<16|1)
                            emit(31<<26|7<<21|19<<1); spr(8,1)
                            text('REG r=')
                            for reg in (5,7,8,3,4,6): emit(36<<26|reg<<21|12<<16|8)
                            text('\n')
    text(f'LOG complete cases={cases}\nEND observations_complete\n')
    emit(36<<26|12<<16|4); emit(18<<26)
    header=bytearray(b'VWIP0001')
    for value in (0x80000000,0x80000000,len(code),0,0,0): header.extend(value.to_bytes(4,'big'))
    return bytes(header+code)
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('integer_registers.'+EXT))
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_bytes(build())
