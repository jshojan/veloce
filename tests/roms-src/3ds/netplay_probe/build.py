#!/usr/bin/env python3
"""Original three-context HID polling guest for 3DS host/viewer replay."""
from pathlib import Path
import sys

def immediate(value):
    for rotation in range(16):
        shift=rotation*2
        candidate=((value<<shift)|(value>>(32-shift if shift else 32)))&0xffffffff
        if candidate<256: return rotation*256+candidate
    raise ValueError('unencodable immediate')

def constant(reg,value):
    words=[0xe3a00000|reg<<12|immediate(value&255)]
    for shift in (8,16,24):
        if value&(255<<shift): words.append(0xe3800000|reg<<16|reg<<12|immediate(value&(255<<shift)))
    return words

def program(core,wrong_input=False):
    # All contexts publish distinct counters from the same input latch.
    words=constant(0,0x10146000)+constant(1,0x20001000+core*12)+constant(2,0)+constant(5,0)
    # This deliberately fixed layout has nine setup attempts and a seven-attempt loop.
    assert len(words)==9
    words += [0xe1d030b0,0xe0822003,0xe5812000,0xe5813004,0xe2855001,0xe5815008,0xeafffff8]
    if wrong_input: words[9]=0xe3a03000  # Executed mutant replaces HID load with zero.
    return b''.join(word.to_bytes(4,'little') for word in words)

def build(wrong_input=False):
    rom=bytearray(64);rom[:8]=b'V3DSP001'
    for core in range(3):
        code=program(core,wrong_input);address=0x08000000 if core==2 else 0x20000000+core*0x10000
        for n,value in enumerate((len(rom),address,address,len(code))):
            rom[16+core*16+n*4:20+core*16+n*4]=value.to_bytes(4,'little')
        rom.extend(code)
    return bytes(rom)

if __name__=='__main__':
    output=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('netplay_probe.v3ds')
    output.write_bytes(build())
