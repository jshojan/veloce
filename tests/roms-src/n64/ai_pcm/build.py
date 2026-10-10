#!/usr/bin/env python3
"""Original AI register/sample program; MIT, no SDK or firmware bytes."""
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


def build(short=False,wrong_length=False,wrong_sample=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE AI ROM       ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0);p.i(15,0,24,0xa450);p.i(15,0,25,0xa430)
    samples=256 if short else 65536;length=samples*2
    p.i(9,0,8,32);p.i(43,25,8,12);p.i(9,0,8,1023);p.i(43,24,8,16);p.i(9,0,8,15);p.i(43,24,8,20)
    p.address(16,"samples");p.i(15,0,17,0xa070);p.i(13,17,17,8);p.i(15,0,18,(samples*4)>>16);p.i(13,18,18,(samples*4)&65535)
    p.mark("copy");p.i(39,16,8,0);p.i(43,17,8,0);p.i(9,16,16,4);p.i(9,17,17,4);p.i(9,18,18,-4);p.branch(5,18,0,"copy")
    p.puts("header");p.i(9,0,8,1);p.i(43,24,8,8)
    p.i(15,0,8,0x70);p.i(13,8,8,8);p.i(43,24,8,0)
    p.i(15,0,9,length>>16);p.i(13,9,9,length&65535)
    if wrong_length:p.i(14,9,9,8)
    p.i(43,24,9,4);p.i(39,24,19,4);p.i(39,24,20,12);p.i(15,0,18,0xc200);p.i(13,18,18,1);p.r(0x24,20,18,20);p.i(39,25,21,8)
    p.i(43,24,0,12);p.i(15,0,8,(0x700008+length)>>16);p.i(13,8,8,(0x700008+length)&65535);p.i(43,24,8,0)
    p.i(15,0,9,length>>16);p.i(13,9,9,length&65535);p.i(43,24,9,4)
    p.i(39,24,22,4);p.i(39,24,23,12);p.r(0x24,23,18,23);p.i(39,25,26,8)
    p.mark("play");p.i(39,24,8,12);p.i(15,0,9,0x4000);p.r(0x24,8,9,8);p.branch(5,8,0,"play")
    p.i(9,0,8,4096);p.mark("settle");p.i(9,8,8,-1);p.branch(5,8,0,"settle")
    p.i(39,24,27,4);p.i(39,24,28,12);p.r(0x24,28,18,28);p.i(39,25,29,8)
    p.puts("row")
    for reg in (19,20,21,22,23,26,27,28,29):p.r(0x25,reg,0,5);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.puts("complete");p.i(43,30,0,4);p.mark("halt");p.branch(4,0,0,"halt")
    # Leaf output routines preserve all observed source/result registers.
    p.mark("puts")
    p.i(36, 4, 8, 0)
    p.branch(4, 8, 0, "puts_return")
    p.i(40, 30, 8, 0)
    p.i(9, 4, 4, 1)
    p.branch(4, 0, 0, "puts")
    p.mark("puts_return")
    p.r(8, rs=31); p.word(0)
    p.mark("hex")
    p.i(9, 0, 6, 28)
    p.mark("hex_loop")
    p.r(0x16, 6, 5, 7)  # DSRLV a3,a1,a2
    p.i(12, 7, 7, 15)
    p.i(11, 7, 8, 10)
    p.branch(5, 8, 0, "digit")
    p.i(9, 7, 7, 39)
    p.mark("digit")
    p.i(9, 7, 7, 48)
    p.i(40, 30, 7, 0)
    p.i(9, 6, 6, -4)
    p.branch(1, 6, 1, "hex_loop")  # BGEZ
    p.r(8, rs=31); p.word(0)


    while p.pc&3:p.data.append(0)
    p.mark("samples")
    for n in range(samples):p.word((n<<16)|(n^(65534 if wrong_sample else 65535)))
    p.string("header",f"VELOCE 1 n64 ai_pcm\nBEGIN data cases=1 words=9\n")
    p.string("row","REG r=");p.string("complete","LOG complete cases=1\nEND observations_complete\n")
    while len(p.data)&3:p.data.append(0)
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("ai_pcm.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
