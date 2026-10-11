#!/usr/bin/env python3
"""Original Primary data cache observations; MIT, no SDK/boot assets."""
from pathlib import Path
import argparse
class Program:
    def __init__(self):
        self.data = bytearray(0x1000)
        self.labels = {}
        self.fixups = []

    @property
    def pc(self):
        return 0xa0000400 + len(self.data) - 0x1000

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
                pc = 0xa0000400 + position - 0x1000
                delta = (target - pc - 4) // 4
                if not -32768 <= delta < 32768:
                    raise ValueError("branch out of range")
                self.data[position:position + 4] = (ins | (delta & 0xffff)).to_bytes(4, "big")
            else:
                self.data[position:position + 4] = (ins | ((target >> 2) & 0x03ffffff)).to_bytes(4, "big")


def build(short=False,wrong_writeback=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("a0000400")
    p.data[0x20:0x34]=b"VELOCE CACHE ROM    ";p.data[0x34:0x3c]=b"V64HB001"
    cases=64 if short else 2097152
    p.i(15,0,30,0xbff0);p.i(13,0,8,0x8003);p.word(0x40888000) # Preserve Config.BE=1/EP=0 and set K0 cached, code stays KSEG1
    p.i(9,0,16,0);p.i(15,0,17,cases>>16);p.i(13,17,17,cases&65535)
    p.puts("header");p.mark("sample")
    p.r(2,rt=16,rd=18,sa=4 if short else 12);p.i(12,18,18,3 if short else 511)
    p.r(0,rt=18,rd=18,sa=4);p.i(15,0,19,0x8070);p.r(0x25,19,18,19)
    p.r(2,rt=16,rd=20,sa=2 if short else 8);p.i(12,20,20,3 if short else 15);p.r(0x21,19,20,19)
    p.i(15,0,20,0x2000);p.r(0x25,19,20,20) # KSEG1 alias
    p.i(12,16,21,3 if short else 255);p.i(14,21,22,255);p.i(9,21,23,17);p.i(12,23,23,255)
    p.i(47,19,17,0);p.i(40,20,21,0);p.i(36,19,24,0)
    p.i(40,20,22,0);p.i(36,19,25,0);p.i(36,20,26,0)
    p.i(40,19,23,0);p.i(36,19,27,0);p.i(36,20,28,0)
    p.i(47,19,17 if wrong_writeback else 25,0);p.i(36,20,29,0)
    p.i(47,19,5,0);p.word(0x4012e000) # TagLo -> s2
    p.puts("row")
    for reg in (25,26,27,28,29,18):p.r(0x25,reg,0,5);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,1);p.branch(5,16,17,"sample")
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

    p.string("header",f"VELOCE 1 n64 cache_coherence\nBEGIN data cases={cases} words=6\n")
    p.string("row","REG r=");p.string("complete",f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data)&3:p.data.append(0)
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("cache_coherence.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
