#!/usr/bin/env python3
"""Original VI framebuffer program; MIT, no SDK or firmware bytes."""
from pathlib import Path
import argparse
class Program:
    def __init__(self):
        self.data = bytearray(0x1000)
        self.labels = {}
        self.fixups = []

    @property
    def pc(self):
        return 0x80000400 + len(self.data) - 0x1000

    def mark(self, name):
        if name in self.labels:
            raise ValueError(f"duplicate label {name}")
        self.labels[name] = self.pc

    def word(self, value):
        self.data.extend(value.to_bytes(4, "big"))

    def r(self, fn, rs=0, rt=0, rd=0, sa=0):
        self.word((rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | fn)

    def i(self, op, rs, rt, imm):
        self.word((op << 26) | (rs << 21) | (rt << 16) | (imm & 0xffff))

    def address(self, rt, name):
        self.fixups.append((len(self.data), name, "address"))
        self.i(15, 0, rt, 0)
        self.i(13, rt, rt, 0)

    def branch(self, op, rs, rt, name, delay=0):
        self.fixups.append((len(self.data), name, "branch"))
        self.i(op, rs, rt, 0)
        self.word(delay)

    def jump(self, op, name):
        self.fixups.append((len(self.data), name, "jump"))
        self.word(op << 26)
        self.word(0)

    def puts(self, name):
        self.address(4, name)
        self.jump(3, "puts")

    def string(self, name, text):
        self.mark(name)
        self.data.extend(text.encode("ascii") + b"\0")

    def resolve(self):
        for position, name, kind in self.fixups:
            target = self.labels[name]
            ins = int.from_bytes(self.data[position:position + 4], "big")
            if kind == "address":
                self.data[position:position + 4] = (ins | (target >> 16)).to_bytes(4, "big")
                low = int.from_bytes(self.data[position + 4:position + 8], "big") | (target & 0xffff)
                self.data[position + 4:position + 8] = low.to_bytes(4, "big")
            elif kind == "branch":
                pc = 0x80000400 + position - 0x1000
                delta = (target - pc - 4) // 4
                if not -32768 <= delta < 32768:
                    raise ValueError("branch out of range")
                self.data[position:position + 4] = (ins | (delta & 0xffff)).to_bytes(4, "big")
            else:
                self.data[position:position + 4] = (ins | ((target >> 2) & 0x03ffffff)).to_bytes(4, "big")


def build(short=False,wrong_word=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE FPU DATA     ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0);p.puts("header")
    p.i(15,0,11,0xa060);p.i(9,0,16,0);p.i(9,0,17,0)
    limit=256 if short else 65536
    for mode in (0,1):
        p.i(15,0,8,0x2000+mode*0x400);p.word(0x40886000);p.word(0);p.word(0)
        p.i(9,0,16,0);p.i(15,0,17,1) if not short else p.i(9,0,17,256)
        p.mark(f"sample{mode}")
        p.r(0,rt=16,rd=12,sa=16);p.r(0x25,12,16,12) # P = repeated 16-bit input
        p.r(0x27,12,0,13);p.r(0x3c,rt=12,rd=14,sa=0);p.r(0x3c,rt=13,rd=15,sa=0);p.r(0x3e,rt=15,rd=15,sa=0);p.r(0x25,14,15,14)
        def cop(rs,rt,fs):p.word(0x44000000|(rs<<21)|(rt<<16)|(fs<<11));p.word(0);p.word(0)
        cop(5,14,2);cop(4,12,3);cop(1,18,2);cop(0,19,2);cop(0,20,3)
        p.i(61,11,2,0);p.i(55,11,21,0);p.i(57,11,2,8);p.i(35,11,22,8)
        cop(4,12,2);cop(0,23,2)
        # LDC1 reads a real doubleword; subsequent SWC1 verifies both halves/modes.
        p.i(53,11,2,0);cop(1,24,2)
        p.i(57,11,2,12);p.i(35,11,25,12)
        p.i(43,11,12,16);p.i(49,11,2,16);cop(0,26,2)
        if wrong_word:p.i(14,26,26,1)
        p.puts("row")
        for reg in range(18,27):p.r(0x25,reg,0,5);p.jump(3,"hex64")
        p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,1);p.branch(5,16,17,f"sample{mode}")
    p.puts("complete");p.i(43,30,0,4);p.mark("halt");p.branch(4,0,0,"halt")
    p.mark("puts");p.i(36,4,8,0);p.branch(4,8,0,"puts_return");p.i(40,30,8,0);p.i(9,4,4,1);p.branch(4,0,0,"puts")
    p.mark("puts_return");p.r(8,rs=31);p.word(0)
    p.mark("hex64");p.i(9,0,6,60);p.mark("hex_loop");p.r(0x16,6,5,7);p.i(12,7,7,15);p.i(11,7,8,10);p.branch(5,8,0,"digit");p.i(9,7,7,39)
    p.mark("digit");p.i(9,7,7,48);p.i(40,30,7,0);p.i(9,6,6,-4);p.branch(1,6,1,"hex_loop");p.r(8,rs=31);p.word(0)
    cases=limit*2;p.string("header",f"VELOCE 1 n64 fpu_data\nBEGIN data cases={cases} words=9\n");p.string("row","REG r=");p.string("complete",f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data)&3:p.data.append(0)
    p.resolve();return bytes(p.data)
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("fpu_data.z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
