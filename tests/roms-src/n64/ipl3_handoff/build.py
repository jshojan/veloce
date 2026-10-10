#!/usr/bin/env python3
"""Original small IPL3-handoff register probe; MIT, no firmware bytes."""
from pathlib import Path
import argparse
class Program:
    def __init__(self):
        self.data = bytearray(0x40)
        self.labels = {}
        self.fixups = []

    @property
    def pc(self):
        return 0xa4000040 + len(self.data) - 0x40

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
                pc = 0xa4000040 + position - 0x40
                delta = (target - pc - 4) // 4
                if not -32768 <= delta < 32768:
                    raise ValueError("branch out of range")
                self.data[position:position + 4] = (ins | (delta & 0xffff)).to_bytes(4, "big")
            else:
                self.data[position:position + 4] = (ins | ((target >> 2) & 0x03ffffff)).to_bytes(4, "big")


def build(wrong_seed=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE IPL ROM      ";p.data[0x34:0x3c]=b"V64IP001"
    p.r(0x25,31,0,25);p.word(0x40186000) # preserve original RA and CP0 Status
    if wrong_seed:p.i(9,22,22,1)
    p.i(15,0,30,0xbff0);p.i(15,0,24,0xa460)
    # Preserve status before overwriting r24 with the PI address.
    # Status is re-read immediately before output; no exception occurs here.
    p.puts("header");p.puts("row")
    for reg in (19,20,21,22,23,29,25):p.r(0x25,reg,0,5);p.jump(3,"hex")
    p.word(0x40056000);p.jump(3,"hex")
    for offset in (20,24,28,32):p.i(39,24,5,offset);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0)
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
    p.i(9, 0, 6, 60)
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

    p.string("header","VELOCE 1 n64 ipl3_handoff\nBEGIN cold cases=1 words=12\n")
    p.string("row","REG r=");p.string("complete","LOG complete cases=1\nEND observations_complete\n")
    assert len(p.data)<=0x1000
    p.data.extend(bytes(0x1004-len(p.data)));p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("ipl3_handoff.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
