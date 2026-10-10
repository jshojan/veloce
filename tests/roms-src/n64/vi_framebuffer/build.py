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


def build(short=False,wrong_pixel=False,wrong_register=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE VI COLORS    ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0);p.puts("header")
    for bits in (16,32):
        p.i(15,0,11,0xa440);p.i(43,11,0,0);p.i(15,0,16,0xa070);p.i(9,0,12,0);p.i(15,0,13,1)
        p.mark(f"ramp{bits}")
        if bits==16:
            p.r(0x25,12,0,9)
            if wrong_pixel:p.i(14,9,9,2)
            p.i(41,16,9,0)
        else:
            p.i(12,12,9,255);p.r(0,rt=9,rd=9,sa=24)
            p.i(12,12,10,0xff00);p.r(0,rt=10,rd=10,sa=8);p.r(0x25,9,10,9)
            p.r(0,rt=12,rd=10,sa=5);p.r(0,rt=12,rd=8,sa=2);p.r(0x21,10,8,10);p.r(0x21,10,12,10)
            p.i(12,10,10,255);p.r(0,rt=10,rd=10,sa=8);p.r(0x25,9,10,9)
            p.i(14,12,10,0xa5);p.i(12,10,10,255);p.r(0x25,9,10,9)
            if wrong_pixel:p.i(14,9,9,256)
            p.i(43,16,9,0)
        p.i(9,16,16,bits//8);p.i(9,12,12,1);p.branch(5,12,13,f"ramp{bits}")
        p.i(15,0,11,0xa440)
        for index,value in ((1,0x700000),(2,257 if wrong_register else 256),(3,2),(6,525),(7,3093),(8,(3093<<16)|3093),(9,256),(10,512),(12,1024),(13,1024)):
            p.i(15,0,8,value>>16);p.i(13,8,8,value&65535);p.i(43,11,8,index*4)
        p.i(43,11,0,16);p.i(15,0,10,0xa430);p.i(9,0,8,128);p.i(43,10,8,12)
        p.i(9,0,8,0x3302 if bits==16 else 0x3303);p.i(43,11,8,0)
        p.mark(f"wait{bits}");p.i(39,10,18,8);p.i(12,18,18,8);p.branch(4,18,0,f"wait{bits}")
        p.word(0x40136800);p.i(12,19,19,0x400);p.i(43,11,0,16);p.i(39,10,20,8)
        for index,reg in zip((0,1,2,7,6,9,10,12,13),range(21,30)):p.i(39,11,reg,index*4)
        p.puts("row")
        for reg in range(18,30):p.r(0x25,reg,0,5);p.jump(3,"hex")
        p.i(9,0,8,10);p.i(40,30,8,0);p.i(43,30,0,12) # Yield a completed source domain through the real ABI.
    p.puts("complete");p.i(43,30,0,4);p.mark("halt");p.branch(4,0,0,"halt")
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


    p.string("header","VELOCE 1 n64 vi_framebuffer\nBEGIN data cases=2 words=12\n")
    p.string("row","REG r=");p.string("complete","LOG complete cases=2\nEND observations_complete\n")
    while len(p.data)&3:p.data.append(0)
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("vi_framebuffer.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
