#!/usr/bin/env python3
"""Original TLB translation program; MIT, no SDK or firmware bytes."""
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


def build(short=False,wrong_asid=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE TLB ROM      ";p.data[0x34:0x3c]=b"V64HB001"
    p.i(15,0,30,0xbff0)
    p.address(8,"handler");p.i(15,0,9,0x8000);p.i(13,9,9,0x180);p.i(9,0,10,22)
    p.mark("copy");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy")
    p.i(15,0,8,0x8000);p.i(15,0,9,0x0800);p.i(13,9,9,0x60)
    for offset in (0,128):p.i(43,8,9,offset);p.i(43,8,0,offset+4)
    p.address(16,"cases");cases=256 if short else 114688
    p.i(15,0,17,cases>>16);p.i(13,17,17,cases&65535)
    p.i(15,0,14,0xdead);p.i(13,14,14,0xbeef);p.puts("header");p.mark("sample")
    p.i(39,16,8,0);p.word(0x40882800);p.i(39,16,8,4)
    if wrong_asid:p.i(14,8,8,1)
    p.word(0x40885000);p.i(9,0,8,32);p.word(0x40880000)
    p.i(39,16,8,8);p.word(0x40881000);p.i(39,16,8,12);p.word(0x40881800)
    p.word(0);p.word(0x42000002);p.word(0) # TLBWI, safe gaps
    p.i(39,16,8,16);p.word(0x40885000);p.word(0);p.word(0x42000008);p.word(0)
    p.word(0x40190000);p.word(0) # Index -> r25; failure low bits undefined and unscored
    p.i(15,0,8,0x8000);p.r(0x24,25,8,9);p.branch(4,9,0,"probe_ok");p.r(0x25,8,0,25);p.mark("probe_ok")
    # Seed two distinct physical words independently of the virtual access under test.
    p.i(15,0,8,0xa000);p.i(15,0,9,0x1234);p.i(13,9,9,0x5678);p.i(43,8,9,32)
    p.i(15,0,8,0xa040);p.i(15,0,9,0x89ab);p.i(13,9,9,0xcdef);p.i(43,8,9,32)
    for reg in (19,20,21,22,23):p.i(9,0,reg,0)
    p.i(39,16,18,20);p.i(9,0,15,0);p.i(39,18,19,0) # Actual guest load
    p.i(9,0,15,1);p.i(43,18,14,0) # Actual guest store
    p.i(35,16,8,24);p.i(39,8,24,0) # Backing read observes successful store
    p.i(9,0,8,32);p.word(0x40880000);p.word(0);p.word(0x42000001);p.word(0)
    for reg,cp in ((26,2),(27,3),(28,5),(29,10)):p.word(0x40000000|(reg<<16)|(cp<<11));p.word(0)
    p.puts("row")
    for reg in (19,20,21,22,23,24,25,26,27,28,29):p.r(0x25,reg,0,5);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,28);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
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
    # 22 original words copied to the common vector; refill vectors jump there.
    handler_start=len(p.data)
    p.word(0x40086800);p.i(12,8,8,124);p.word(0x40094000)
    p.branch(5,15,0,"store_fault");p.r(0x25,8,0,20);p.r(0x25,9,0,21);p.branch(4,0,0,"recover")
    p.mark("store_fault");p.r(0x25,8,0,22);p.r(0x25,9,0,23)
    p.mark("recover");p.word(0x400a7000);p.i(9,10,10,4);p.word(0x408a7000);p.word(0);p.word(0x42000018);p.word(0)
    while len(p.data)-handler_start<22*4:p.word(0)
    assert len(p.data)-handler_start==22*4
    p.mark("cases")
    for shift in (0,) if short else range(0,13,2):
        page=4096<<shift;mask=page*2-8192
        for asid in range(4 if short else 256):
            for pattern in range(32):
                v=pattern&1;d=(pattern>>1)&1;g0=(pattern>>2)&1;g1=(pattern>>3)&1;query=asid^((pattern>>4)&1)
                for half in range(2):
                    odd_base=0x400000 & ~(page-1)
                    for value in (mask,0x20000000+asid,v*2+d*4+16+g0,0x10000+v*2+d*4+16+g1,
                                  0x20000000+query,0x20000020+half*page,0xa0000020+half*odd_base):p.word(value)
    p.string("header",f"VELOCE 1 n64 tlb_translation\nBEGIN data cases={cases} words=11\n")
    p.string("row","REG r=");p.string("complete",f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data)&3:p.data.append(0)
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("tlb_translation.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
