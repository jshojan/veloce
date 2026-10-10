#!/usr/bin/env python3
"""Original RSP vector transfer observations; MIT, no SDK, firmware or commercial bytes."""
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
    p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400");p.data[0x20:0x34]=b"VELOCE RSP VECTOR   ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0);p.i(15,0,24,0xa410);p.i(15,0,25,0xa400);p.i(15,0,26,0xa404)
    p.address(16,"table");p.i(15,0,17,count>>16);p.i(13,17,17,count&65535)
def finish(p,count,words):
    p.puts("complete");p.i(43,30,0,4);p.mark("halt");p.branch(4,0,0,"halt")
    output_routines(p);p.string("header",f"VELOCE 1 n64 {SUITE}\nBEGIN data cases={count} words={words}\n");p.string("row","REG r=");p.string("complete",f"LOG complete cases={count}\nEND observations_complete\n")
    while p.pc&7:p.data.append(0)
SUITE="rsp_vector_transfers"
def cases(short=False):
    for raw in range(8 if short else 65536):
        for e in range(2 if short else 16):yield raw,e
def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=Program();header(p,len(rows),10)
    p.address(8,"microcode");p.i(13,25,9,4096);p.i(9,0,10,20)
    p.mark("copy_code");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy_code")
    p.address(8,"seed");p.i(13,25,9,0x400);p.i(9,0,10,4)
    p.mark("copy_seed");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy_seed")
    p.puts("header");p.mark("sample");p.i(39,16,12,0);p.r(2,rt=12,rd=13,sa=4);p.i(15,0,8,0xa5a5);p.r(37,8,13,13);p.i(43,25,13,0x100)
    p.i(12,12,8,15);p.r(0,rt=8,rd=8,sa=7);p.i(15,0,9,0xa400);p.i(13,9,9,0x100c)
    for hi,lo in ((0x4882,0x2800),(0x4804,0x2800),(0x4800,0x2800)):
        p.i(15,0,10,hi);p.i(13,10,10,lo);p.r(37,8,10,10);p.i(43,9,10,0);p.i(9,9,9,4)
    p.i(15,0,9,0xa408);p.i(43,9,0,0);p.i(9,0,8,0xad);p.i(43,26,8,16)
    p.mark("wait");p.i(39,26,8,16);p.i(12,8,8,1);p.branch(4,8,0,"wait")
    p.puts("row")
    for off in (0x600,0x608):
        p.i(55,25,5,off)
        if wrong_capture and off==0x600:p.i(14,5,5,1)
        p.jump(3,"hex")
    for off in (0x620,0x624,0x628,0x62c,0x630):p.i(39,25,5,off);p.jump(3,"hex")
    p.i(15,0,8,0xa408);p.i(39,8,5,0);p.jump(3,"hex");p.i(39,26,5,16);p.jump(3,"hex");p.i(15,0,8,0xa430);p.i(39,8,5,8);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,4);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    finish(p,len(rows),10);p.mark("microcode")
    for word in (0x8c020100,0x34030400,0xc8652000,0x48822800,0x48042800,0x48002800,0xe8652020,0x48c20000,0x00402827,0x48c50800,0x48c21000,0x48460000,0xac060620,0x48470800,0xac070624,0x48481000,0xac080628,0xac04062c,0xac000630,13):p.word(word)
    p.mark("seed");p.data.extend(bytes(range(0x90,0xa0)));p.mark("table")
    for raw,e in rows:p.word(raw*16+e)
    p.resolve();return bytes(p.data)
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
