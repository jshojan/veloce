#!/usr/bin/env python3
"""Original three-context Wii U input polling and sparse high-RAM probe."""
from pathlib import Path
import sys

def constant(reg,value):
    return [15<<26|reg<<21|(value>>16),24<<26|reg<<21|reg<<16|(value&65535)]

def program(core,wrong_input=False):
    words=constant(12,0xffe0000c)+constant(1,0x1000+core*12)+constant(2,0)+constant(5,0)+constant(6,0x8fffffe0+core*4)
    assert len(words)==10
    words += [32<<26|3<<21|12<<16,31<<26|2<<21|2<<16|3<<11|266<<1,
              36<<26|2<<21|1<<16,36<<26|3<<21|1<<16|4,14<<26|5<<21|5<<16|1,36<<26|5<<21|1<<16|8,
              28<<26|3<<21|7<<16|1,36<<26|7<<21|6<<16,32<<26|8<<21|6<<16,
              36<<26|8<<21|1<<16|(36-core*8),18<<26|((-40)&0x03fffffc)]
    if wrong_input:words[10]=14<<26|3<<21
    return b''.join(word.to_bytes(4,'big') for word in words)

def build(wrong_input=False):
    rom=bytearray(64);rom[:8]=b'VWUP0001'
    for core in range(3):
        code=program(core,wrong_input);address=0x10000000+core*0x10000
        for n,value in enumerate((len(rom),address,address,len(code))):rom[16+core*16+n*4:20+core*16+n*4]=value.to_bytes(4,'big')
        rom.extend(code)
    return bytes(rom)

if __name__=='__main__':
    output=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('netplay_probe.vwiiu')
    output.write_bytes(build())
