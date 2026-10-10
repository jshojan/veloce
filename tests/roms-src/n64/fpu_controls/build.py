#!/usr/bin/env python3
"""Original FPU control program; MIT, no SDK or firmware bytes."""
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


def build(short=False,wrong_control=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE FPU CONTROL  ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0)
    # Original exception handler is copied to the ordinary kernel vector.
    p.address(8,"handler");p.i(15,0,9,0x8000);p.i(13,9,9,0x180);p.i(9,0,10,14)
    p.mark("copy");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy")
    p.i(15,0,8,0x2000);p.word(0x40886000);p.word(0)
    cases=128 if short else 8192
    p.address(16,"cases");p.i(9,0,17,cases);p.puts("header");p.mark("sample")
    p.i(9,0,19,0);p.i(9,0,20,0);p.i(39,16,18,0)
    if wrong_control:p.i(14,18,18,1)
    p.word(0x44d2f800);p.word(0) # CTC1 r18,FCR31 plus safe gap
    p.word(0x4456f800);p.word(0) # CFC1 r22,FCR31 plus safe gap
    p.branch(5,20,0,"captured");p.r(0x25,22,0,19);p.mark("captured")
    for rt,reg in enumerate((23,24,25,26)):
        p.i(9,0,reg,0);p.i(17,8,rt,2);p.i(9,reg,reg,1);p.i(9,reg,reg,2)
    p.word(0x44d20000);p.word(0) # FCR0 ignores write
    p.word(0x445b0000);p.word(0) # CFC1 r27,FCR0
    p.puts("row")
    for reg in (19,22,20,23,24,25,26,27):p.r(0x25,reg,0,5);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,4);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
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


    p.mark("handler")
    for word in (0x40146800,0x3294007c,0x4453f800,0,0x3c08fffc,0x35080fff,
                 0x02684024,0x44c8f800,0,0x40097000,0x25290004,0x40897000,0x42000018,0):p.word(word)
    p.mark("cases")
    for cause in range(1 if short else 64):
        for enable in range(32):
            for rm in range(4):
                flags=(cause^enable)&31
                p.word(0xfe7c0000 | ((rm&1)<<24) | ((rm>>1)<<23) | (cause<<12) | (enable<<7) | (flags<<2) | rm)
    p.string("header",f"VELOCE 1 n64 fpu_controls\nBEGIN data cases={cases} words=8\n")
    p.string("row","REG r=");p.string("complete",f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data)&3:p.data.append(0)
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("fpu_controls.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
