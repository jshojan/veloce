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
SUITE="dp_fill_rectangles"
def cases(short=False):
    for raw in range(4 if short else 65536):
        for size in (2,3):
            for source in range(2):
                for layout in range(2):yield raw,size,source,layout

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=Program();header(p,len(rows),13);p.i(15,0,27,0xa070);p.i(15,0,28,0xa430)
    p.address(8,"commands");p.i(13,27,9,256);p.i(9,0,10,14)
    p.mark("copy");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy")
    p.puts("header");p.mark("sample");p.i(39,16,12,0)
    p.i(9,0,8,0x800);p.i(43,28,8,0) # Acknowledge last actual FullSync IRQ.
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8);p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,10)
    p.mark("clear");p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,"clear")
    p.r(2,rt=12,rd=8,sa=16);p.i(12,8,8,1);p.r(0,rt=8,rd=8,sa=19);p.i(15,0,9,0x3f10);p.i(13,9,9,3);p.r(37,8,9,8);p.i(43,27,8,256)
    p.i(12,12,8,65535);p.r(0,rt=8,rd=9,sa=16);p.i(14,8,8,65535);p.r(37,8,9,8);p.i(43,27,8,284)
    p.r(2,rt=12,rd=8,sa=18);p.i(12,8,8,1);p.branch(4,8,0,"whole")
    p.i(15,0,8,0x2d00);p.i(13,8,8,0x4004);p.i(43,27,8,264);p.i(15,0,8,0);p.i(13,8,8,0xc00c);p.i(43,27,8,268);p.branch(4,0,0,"scissor_done")
    p.mark("whole");p.i(15,0,8,0x2d00);p.i(43,27,8,264);p.i(15,0,8,1);p.i(13,8,8,16);p.i(43,27,8,268)
    p.mark("scissor_done");p.i(13,27,18,256);p.i(13,27,19,312);p.i(9,0,20,0x15) # DRAM, unfreeze, unflush.
    p.r(2,rt=12,rd=8,sa=17);p.i(12,8,8,1);p.branch(4,8,0,"source_done")
    p.i(13,27,8,256);p.i(9,0,9,0xff0);p.i(9,0,10,14)
    p.mark("dmem_copy");p.r(37,25,9,11);p.i(39,8,13,0);p.i(43,11,13,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(12,9,9,4095);p.i(9,10,10,-1);p.branch(5,10,0,"dmem_copy")
    p.i(9,0,18,0xff0);p.i(9,0,19,0x1028);p.i(9,0,20,0x16) # DMEM, unfreeze, unflush.
    p.mark("source_done");p.i(43,24,20,12);p.i(43,24,18,0);p.i(43,24,19,4)
    p.mark("wait");p.i(39,24,8,8);p.i(12,19,9,65535);p.i(15,0,10,255);p.i(13,10,10,65535);p.r(36,19,10,9);p.branch(5,8,9,"wait")
    p.puts("row");p.r(37,27,0,18);p.i(9,0,20,8)
    p.mark("pixels");p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    p.jump(3,"hex");p.i(9,18,18,8);p.i(9,20,20,-1);p.branch(5,20,0,"pixels")
    p.i(15,0,8,0xa06f);p.i(13,8,8,0xfff8);p.i(55,8,5,0);p.jump(3,"hex");p.i(55,27,5,64);p.jump(3,"hex")
    for addr,off in ((24,8),(24,12),(28,8)):p.i(39,addr,5,off);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,4);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    finish(p,len(rows),13);p.mark("commands")
    for word in (0x3f10000300700000,0x2d00000000010010,0x2f30000000000000,0x3700000000000000,0x3600c00c00000000,0x2700000000000000,0x2900000000000000):p.data.extend(word.to_bytes(8,"big"))
    p.mark("table")
    for raw,size,source,layout in rows:p.word(raw+((size-2)<<16)+(source<<17)+(layout<<18))
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
