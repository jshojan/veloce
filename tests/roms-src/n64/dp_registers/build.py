#!/usr/bin/env python3
"""Original DP observations; MIT, no SDK, firmware or commercial bytes."""
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



def output_routines(p):
    p.mark("puts");p.i(36,4,8,0);p.branch(4,8,0,"puts_return");p.i(40,30,8,0);p.i(9,4,4,1);p.branch(4,0,0,"puts")
    p.mark("puts_return");p.r(8,rs=31);p.word(0)
    p.mark("hex");p.i(9,0,6,60);p.mark("hex_loop");p.r(0x16,6,5,7);p.i(12,7,7,15);p.i(11,7,8,10);p.branch(5,8,0,"digit");p.i(9,7,7,39)
    p.mark("digit");p.i(9,7,7,48);p.i(40,30,7,0);p.i(9,6,6,-4);p.branch(1,6,1,"hex_loop");p.r(8,rs=31);p.word(0)
def header(p,count,words):
    p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400");p.data[0x20:0x34]=b"VELOCE DP ORIGINAL  ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0);p.i(15,0,24,0xa410);p.i(15,0,25,0xa400);p.i(15,0,26,0xa404)
    p.address(16,"table");p.i(15,0,17,count>>16);p.i(13,17,17,count&65535)
def finish(p,count,words):
    p.puts("complete");p.i(43,30,0,4);p.mark("halt");p.branch(4,0,0,"halt")
    output_routines(p);p.string("header",f"VELOCE 1 n64 {SUITE}\nBEGIN data cases={count} words={words}\n");p.string("row","REG r=");p.string("complete",f"LOG complete cases={count}\nEND observations_complete\n")
    while p.pc&7:p.data.append(0)
SUITE="dp_registers"
def cases(short=False):
    for raw in range(8 if short else 65536):
        a=raw*65537
        for value in (a,a^0xffffffff,(raw<<8)|0xa5000000,raw|0xff000000):yield value

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=Program();header(p,len(rows),10)
    p.address(8,"microcode");p.i(13,25,9,4096);p.i(9,0,10,18)
    p.mark("copy");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy")
    p.puts("header");p.mark("sample");p.i(9,0,8,0x19);p.i(43,24,8,12);p.i(43,24,0,0);p.i(43,24,0,4)
    p.i(39,16,12,0);p.i(43,25,12,0);p.i(9,0,8,0x1238);p.i(43,25,8,4)
    p.i(15,0,9,0xa408);p.i(43,9,0,0);p.i(9,0,8,0xad);p.i(43,26,8,16)
    p.mark("wait");p.i(39,26,8,16);p.i(12,8,8,1);p.branch(4,8,0,"wait")
    p.puts("row")
    for offset in (16,20,24,28,32,36):
        p.i(39,25,5,offset)
        if wrong_capture and offset==16:p.i(14,5,5,1)
        p.jump(3,"hex")
    for offset in (0,4,8,12):p.i(39,24,5,offset);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,4);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    finish(p,len(rows),10);p.mark("microcode")
    # Actual RSP MTC0/MFC0 and stores to DMEM; CPU then captures shared MMIO.
    for word in (0x8c020000,0x8c030004,0x40824000,0x40044000,0xac040010,0x40045000,0xac040014,0x40045800,0xac040018,0x40834000,0x40044000,0xac04001c,0x40824800,0x40045000,0xac040020,13):p.word(word)
    # Status after END is captured by CPU too; original RSP stores it before BREAK.
    at=p.labels["microcode"]-0x80000400+0x1000
    p.data[at+15*4:at+16*4]=b"" # replace last BREAK with three words
    p.word(0x40045800);p.word(0xac040024);p.word(13)
    p.mark("table")
    for value in rows:p.word(value)
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
