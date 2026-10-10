#!/usr/bin/env python3
"""Original aligned PI cartridge-read observations; MIT, no SDK/boot assets."""
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


def build(short=False,wrong_length=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE PI ROM       ";p.data[0x34:0x3c]=b"V64HB001"
    cases=256 if short else 524288
    p.i(15,0,30,0xbff0);p.i(15,0,24,0xa460);p.i(15,0,25,0xa430)
    p.address(16,"cases");p.i(15,0,17,cases>>16);p.i(13,17,17,cases&65535)
    p.puts("header")
    p.mark("sample")
    p.i(39,16,18,0);p.i(39,16,19,4);p.i(39,16,20,8)
    p.i(12,20,8,2);p.r(0,rt=8,rd=8,sa=2);p.i(15,0,21,0x8070);p.r(0x25,21,8,21)
    p.i(12,20,8,1);p.i(9,0,9,0x100);p.r(4,8,9,8);p.i(43,25,8,12)
    p.i(9,0,8,0xa55a);p.i(41,21,8,-2)
    p.r(0x21,21,18,22);p.i(41,22,8,0)
    p.i(15,0,8,0x70);p.i(12,20,9,2);p.r(0,rt=9,rd=9,sa=2);p.r(0x25,8,9,8)
    p.i(43,24,8,0);p.i(43,24,19,4)
    p.i(9,18,8,1 if wrong_length else -1);p.i(43,24,8,12)
    p.puts("row")
    for reg,offset in ((21,0),(22,-2),(21,-2),(22,0)):
        p.i(37,reg,5,offset);p.jump(3,"hex")
    for reg,offset in ((24,0),(24,4),(24,12),(24,16),(25,8),(25,12)):
        p.i(39,reg,5,offset);p.jump(3,"hex")
    p.word(0x40056800);p.jump(3,"hex") # MFC0 a1,Cause
    p.i(9,0,8,2);p.i(43,24,8,16)
    for reg,offset in ((24,16),(25,8)):
        p.i(39,reg,5,offset);p.jump(3,"hex")
    p.word(0x40056800);p.jump(3,"hex")
    for reg in (18,19):p.r(0x25,reg,0,5);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0)
    p.i(9,16,16,12);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    p.puts("complete");p.i(43,30,0,4)
    p.mark("halt");p.branch(4,0,0,"halt")
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
    p.mark("cases")
    fix=[]
    for offset in (0,2,65534,65536):
        for dram in range(2):
            for mi in range(2):
                for count in range(1,17 if short else 32769):
                    p.word(count*2);fix.append((len(p.data),offset));p.word(0);p.word(dram*2+mi)
    p.string("header",f"VELOCE 1 n64 pi_dma\nBEGIN aligned cases={cases} words=16\n")
    p.string("row","REG r=");p.string("complete",f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data)&7:p.data.append(0)
    source=len(p.data)
    p.data.extend((n*37+(n>>8)*11+0x5b)&255 for n in range(196608))
    for pos,offset in fix:p.data[pos:pos+4]=(0x10000000+source+offset).to_bytes(4,"big")
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("pi_dma.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
