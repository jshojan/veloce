#!/usr/bin/env python3
"""Original GameCube communication-RAM controller observations; MIT."""
from pathlib import Path
import argparse
CONSOLE="wii"

def build(cases=4096,wrong_command=False):
    if not 1<=cases<=4096:raise ValueError("invalid domain")
    words=[];attempts=0
    def emit(v):
        nonlocal attempts
        words.append(v);attempts+=1
    def const(r,v):emit(15<<26|r<<21|(v>>16));emit(24<<26|r<<21|r<<16|(v&65535))
    def text(s):
        for c in s.encode():emit(14<<26|c);emit(38<<26|12<<16)
    def store(r,offset):emit(36<<26|r<<21|11<<16|offset)
    def read(r,offset):emit(32<<26|r<<21|11<<16|offset)
    def output(r):emit(36<<26|r<<21|12<<16|8)
    const(12,0xfff00000);const(11,0xcc006400 if CONSOLE=="gc" else 0xcd006400)
    text(f"VELOCE 1 {CONSOLE} controller_serial\nBEGIN digital cases={cases} words=16\n")
    for sample in range(cases):
        # Clear error latches, submit mode-3 direct read, then capture actual RAM.
        const(10,0x0f0f0f0f);store(10,0x38)
        const(10,0x000300a5 if wrong_command else 0x400300a5);store(10,0x80)
        const(10,0xdecafbad);store(10,0x84)
        const(10,0xc0030801);store(10,0x34)
        text("REG r=")
        for offset in (0x80,0x84,0x34,0x38):read(5,offset);output(5)
        # Clear TCINT; verify its readback before the next communication.
        const(10,0x80030800);store(10,0x34);read(5,0x34);output(5)
        # Identify the one connected port. Fourth byte is untouched, not an ID byte.
        const(10,0x00aabbcc);store(10,0x80)
        const(10,0xc0010301);store(10,0x34)
        for offset in (0x80,0x34,0x38):read(5,offset);output(5)
        # Ports 1-3 are unplugged: receive storage stays intact and NOREP latches.
        for port in range(1,4):
            const(10,0x400300a5);store(10,0x80)
            const(10,0xc0030801|port<<1);store(10,0x34)
            for offset in (0x80,0x38):read(5,offset);output(5)
        # Error acknowledgement and zero-write preservation are observed too.
        const(10,0x0f0f0f0f);store(10,0x38);read(5,0x38);output(5)
        const(10,0x80030806);store(10,0x34);read(5,0x34);output(5)
        text("\n")
        if sample+1<cases:
            # Each sample begins at a frontend quantum, including first-frame header.
            # One bdnz attempt per remaining step; no expected response is in the ROM.
            remaining=100000-(attempts-sample*100000)-3
            assert remaining>0
            const(9,remaining);emit(31<<26|9<<21|9<<16|467<<1) # mtctr r9 (SPR 9)
            emit(16<<26|16<<21) # bdnz to itself
            # Static length differs from execution length after this counted branch.
            # Track virtual attempt positions separately with an explicit frame base.
            attempts+=remaining-1
    text(f"LOG complete cases={cases}\nEND observations_complete\n")
    emit(36<<26|12<<16|4);emit(18<<26)
    code=b"".join(v.to_bytes(4,"big") for v in words)
    magic=b"VGCP0001" if CONSOLE=="gc" else b"VWIP0001"
    return magic+b"".join(v.to_bytes(4,"big") for v in (0x80000000,0x80000000,len(code),0,0,0))+code

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("controller_serial."+("vgc" if CONSOLE=="gc" else "vwi")))
    args=parser.parse_args();args.output.write_bytes(build())
