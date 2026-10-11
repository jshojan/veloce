#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original V810 software-clocked pad reads and actual WRAM observations."""
from pathlib import Path
import argparse
ALIASES=(0x02000000,0x02000040,0x02ffffc0,0x82000000)
def build(width=1,alias=0,wrong_capture=False,missing_clock=False):
    if width not in (1,2,4) or not 0<=alias<4:raise ValueError('invalid domain')
    code=bytearray()
    def reg(op,a=0,b=0):code.extend((op<<10|b<<5|a).to_bytes(2,'little'))
    def wide(op,a,b,imm):reg(op,a,b);code.extend((imm&65535).to_bytes(2,'little'))
    def const(r,v):wide(0x2f,0,r,v>>16);wide(0x2c,r,r,v&65535)
    out={1:0x3c,2:0x3d,4:0x3f}[width];read={1:0x38,2:0x39,4:0x3b}[width]
    const(12,ALIASES[alias]);const(13,0x05001000)
    const(6,0xb1);wide(out,12,6,0x28) # Latch and software clock low on the wire.
    for n in range(16):
        const(6,0x91);wide(out,12,6,0x28)
        const(6,0x81)
        if missing_clock and n==15:reg(0);reg(0)
        else:wide(out,12,6,0x28)
    for n,offset in enumerate((0x10,0x14,0x28)):
        if wrong_capture and n==0:reg(0x10,0,6);reg(0)
        else:wide(read,12,6,offset)
        wide(0x37,13,6,n*4)
    wide(out,12,0,0x10);wide(out,12,0,0x14)
    for n,offset in enumerate((0x10,0x14),3):wide(read,12,6,offset);wide(0x37,13,6,n*4)
    const(6,0x81);wide(out,12,6,0x28);wide(read,12,6,0x28);wide(0x37,13,6,20)
    wide(0x37,13,0,24);const(6,0x89abcdef);wide(0x37,13,6,28);reg(0x1a)
    size=1024
    while len(code)+0x200>size:size*=2
    rom=code+bytes(size-len(code));rom=bytearray(rom);rom[size-0x200:size-0x1f8]=b'VVBHB001'
    rom[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little')
    return bytes(rom)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('controller_serial.vb'));a=p.parse_args();a.output.write_bytes(build())
