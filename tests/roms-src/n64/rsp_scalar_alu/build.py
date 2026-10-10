#!/usr/bin/env python3
"""Original SP observations; MIT, no SDK, firmware or commercial bytes."""
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


SUITE="rsp_scalar_alu"
FUNCTIONS=(32,33,34,35,36,37,38,39,42,43)
PATTERNS=(0,0x7fffffff,0x80000000)
def cases(short=False):
    if SUITE=="rsp_scalar_alu":
        for a in range(4 if short else 256):
            for b in range(4 if short else 256):
                for fn in FUNCTIONS:yield (2<<21)|(3<<16)|(4<<11)|fn,a*0x01010101,b*0x01010101
        if not short:
            for raw in range(256):
                for amount in range(32):
                    for fn in (0,2,3,4,6,7):yield ((0 if fn<4 else 2)<<21)|(3<<16)|(4<<11)|((amount if fn<4 else 0)<<6)|fn,amount*0x01010101,raw*0x01010101
    else:
        for pattern in PATTERNS:
            for raw in range(8 if short else 65536):
                for op in range(8,16):yield (op<<26)|((0 if op==15 else 2)<<21)|(4<<16)|raw,pattern,0

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400");p.data[0x20:0x34]=b"VELOCE RSP SCALAR   ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0);p.i(15,0,24,0xa400);p.i(15,0,25,0xa404);p.i(15,0,26,0xa408)
    p.address(8,"microcode");p.i(13,24,9,4096);p.i(9,0,10,8)
    p.mark("copy");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy")
    p.address(16,"table");p.i(15,0,17,len(rows)>>16);p.i(13,17,17,len(rows)&65535);p.puts("header")
    p.mark("sample");p.i(39,16,12,0)
    if SUITE=="rsp_scalar_alu":
        p.i(39,16,13,4);p.i(12,13,14,255);p.r(2,rt=13,rd=13,sa=8);p.i(12,13,13,255)
        for reg in (13,14):
            p.r(0,rt=reg,rd=8,sa=8);p.r(37,reg,8,reg);p.r(0,rt=reg,rd=8,sa=16);p.r(37,reg,8,reg)
    else:
        p.r(2,rt=12,rd=8,sa=21);p.i(12,8,8,3);p.r(0,rt=8,rd=8,sa=2);p.address(9,"patterns");p.r(33,8,9,8);p.i(39,8,13,0);p.i(9,0,14,0)
        # Remove the packed pattern selector and reconstruct architected rs=2.
        p.i(15,0,8,0xfc1f);p.i(13,8,8,65535);p.r(36,12,8,12);p.r(2,rt=12,rd=8,sa=26);p.i(9,0,9,15);p.branch(4,8,9,"lui");p.i(15,0,8,0x40);p.r(37,12,8,12);p.mark("lui")
    p.i(43,24,13,0);p.i(43,24,14,4);p.i(43,24,12,4104);p.i(43,26,0,0)
    p.i(9,0,8,0xad);p.i(43,25,8,16)
    p.mark("wait");p.i(39,25,8,16);p.i(12,8,8,1);p.branch(4,8,0,"wait")
    for offset,reg in ((8,18),(12,19),(16,20),(20,21)):p.i(39,24,reg,offset)
    if wrong_capture:p.i(14,18,18,1)
    p.i(39,26,22,0);p.i(39,25,23,16);p.i(15,0,8,0xa430);p.i(39,8,27,8)
    p.puts("row")
    for reg in (18,19,20,21,22,23,27):p.r(37,reg,0,5);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,8 if SUITE=="rsp_scalar_alu" else 4);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    p.puts("complete");p.i(43,30,0,4);p.mark("halt");p.branch(4,0,0,"halt")
    p.mark("puts");p.i(36,4,8,0);p.branch(4,8,0,"puts_return");p.i(40,30,8,0);p.i(9,4,4,1);p.branch(4,0,0,"puts")
    p.mark("puts_return");p.r(8,rs=31);p.word(0)
    p.mark("hex");p.i(9,0,6,28);p.mark("hex_loop");p.r(0x16,6,5,7);p.i(12,7,7,15);p.i(11,7,8,10);p.branch(5,8,0,"digit");p.i(9,7,7,39)
    p.mark("digit");p.i(9,7,7,48);p.i(40,30,7,0);p.i(9,6,6,-4);p.branch(1,6,1,"hex_loop");p.r(8,rs=31);p.word(0)
    p.string("header",f"VELOCE 1 n64 {SUITE}\nBEGIN data cases={len(rows)} words=7\n");p.string("row","REG r=");p.string("complete",f"LOG complete cases={len(rows)}\nEND observations_complete\n")
    while p.pc&3:p.data.append(0)
    p.mark("microcode")
    for word in (0x8c020000,0x8c030004,0,0xac040008,0xac02000c,0xac030010,0xac000014,13):p.word(word)
    p.mark("patterns")
    for word in PATTERNS:p.word(word)
    p.mark("table")
    for ins,a,b in rows:
        if SUITE=="rsp_scalar_alu":
            p.word(ins);p.word(((a//0x01010101)&255)*256+((b//0x01010101)&255))
        else:p.word((ins&~(31<<21))|(PATTERNS.index(a)<<21))
    p.resolve();return bytes(p.data)
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
