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


def build(short=False,wrong_length=False):
    p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE SP DMA ROM   ";p.data[0x34:0x3c]=b"V64HB001"
    lengths=16 if short else 4096;cases=lengths*2*2*4
    p.i(15,0,30,0xbff0);p.i(15,0,24,0xa404)
    p.address(16,"cases");p.i(15,0,17,cases>>16);p.i(13,17,17,cases&65535)
    # Initialize full RAM/source and both SP banks with repeating byte ramps.
    p.i(15,0,18,0xa070);p.i(15,0,19,0xa400);p.i(9,0,20,0);p.i(9,0,21,8192)
    p.mark("init");p.i(40,18,20,0);p.r(2,rt=20,rd=8,sa=12);p.r(0,rt=8,rd=9,sa=4);p.r(0x21,9,8,9);p.r(0x21,9,20,9);p.i(40,19,9,0)
    p.i(9,18,18,1);p.i(9,19,19,1);p.i(9,20,20,1);p.branch(5,20,21,"init")
    p.puts("header");p.mark("sample")
    p.i(39,16,18,0);p.i(39,16,19,4);p.i(39,16,20,8);p.i(39,16,21,12)
    p.branch(5,20,0,"sources_ready")
    p.r(0x25,18,0,22);p.r(0x25,19,0,23);p.r(0x25,20,0,29);p.r(0x25,21,0,28)
    p.i(15,0,18,0xa070);p.i(15,0,19,0xa400);p.i(9,0,20,0);p.i(9,0,21,8192)
    p.mark("reset_sources");p.i(40,18,20,0);p.r(2,rt=20,rd=8,sa=12);p.r(0,rt=8,rd=9,sa=4);p.r(0x21,9,8,9);p.r(0x21,9,20,9);p.i(40,19,9,0)
    p.i(9,18,18,1);p.i(9,19,19,1);p.i(9,20,20,1);p.branch(5,20,21,"reset_sources")
    p.r(0x25,22,0,18);p.r(0x25,23,0,19);p.r(0x25,29,0,20);p.r(0x25,28,0,21)
    p.mark("sources_ready")
    # Reset pointers every transfer; low source/destination bits deliberately nonzero.
    p.i(43,24,19,0);p.i(15,0,8,0x70);p.i(13,8,8,7);p.i(43,24,8,4)
    if wrong_length:p.i(14,20,20,8)
    p.r(0x21,24,18,22);p.i(43,22,20,8)
    p.i(39,24,23,0) # actual bank-wrapped final pointer
    # Capture first/last transferred bytes. For SP->RAM offset pattern starts at SP offset;
    # for RAM->SP, source starts at RAM byte zero. Re-seed source ramp each sample.
    p.i(15,0,25,0xa400);p.i(12,19,8,0x1ff8);p.r(0x21,25,8,25)
    p.i(15,0,26,0xa070)
    p.branch(4,18,0,"to_sp")
    p.i(36,26,27,0);p.r(0x21,26,21,26);p.i(36,26,28,-1);p.branch(4,0,0,"captured")
    p.mark("to_sp");p.i(36,25,27,0);p.i(12,19,25,4096);p.i(12,23,26,4095);p.i(9,26,26,-1);p.i(12,26,26,4095)
    p.r(0x25,25,26,25);p.i(15,0,26,0xa400);p.r(0x21,26,25,25);p.i(36,25,28,0)
    p.mark("captured");p.puts("row")
    for reg in (27,28,23):p.r(0x25,reg,0,5);p.jump(3,"hex")
    for offset in (16,20,24):p.i(39,24,5,offset);p.jump(3,"hex")
    p.i(9,0,8,10);p.i(40,30,8,0)
    p.i(9,16,16,16);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
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


    while p.pc&3:p.data.append(0)
    p.mark("cases")
    for direction in (0,4):
        for bank in (0,4096):
            for start in (0,8,4080,4088):
                for raw in range(lengths):
                    p.word(direction);p.word(bank+start+7);p.word(raw);p.word((raw//8+1)*8)
    p.string("header",f"VELOCE 1 n64 sp_dma\nBEGIN data cases={cases} words=6\n")
    p.string("row","REG r=");p.string("complete",f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data)&3:p.data.append(0)
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("sp_dma.z64"))
    args=parser.parse_args();args.output.write_bytes(build())
