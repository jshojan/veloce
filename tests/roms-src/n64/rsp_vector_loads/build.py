#!/usr/bin/env python3
"""Original RSP vector observations; MIT, no SDK, firmware or commercial bytes."""
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
SUITE="rsp_vector_loads"
STORE=False
def cases(short=False):
    for seed in range(2 if short else 256):
        for alignment in range(2 if short else 16):
            for element in range(2 if short else 16):
                for kind in range(12):yield seed,alignment,element,kind

def pattern(seed,index):return (seed*37+index*19+(index//2)*7)%256

def emit_hex(p):
    label="next_chunk_"+str(len(p.data))
    p.jump(3,"hex");p.i(9,22,22,1);p.i(9,0,23,12);p.branch(5,22,23,label)
    p.i(9,0,22,0);p.i(9,0,8,10);p.i(40,30,8,0);p.puts("row");p.mark(label)
def build(short=False,wrong_capture=False):
    rows=list(cases(short));words=33 if STORE else 25;p=Program();header(p,len(rows),words)
    p.address(8,"microcode");p.i(13,25,9,4096);p.i(9,0,10,70)
    p.mark("copy_code");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy_code")
    p.puts("header");p.mark("sample");p.i(39,16,12,0);p.i(39,16,14,4);p.i(12,14,13,4095);p.i(15,0,8,65535);p.i(13,8,8,65520);p.r(36,14,8,14)
    p.i(15,0,8,0xa400);p.i(13,8,8,0x1088);p.i(43,8,12,0) # Patch actual load/store slot.
    p.i(43,25,13,0x100) # RSP GPR base parameter, outside source/output.
    p.r(2,rt=14,rd=8,sa=16);p.r(0,rt=8,rd=8,sa=9);p.address(9,"vectors");p.r(33,8,9,8);p.i(13,25,9,0x400);p.i(9,0,10,128)
    p.mark("seed_vectors");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"seed_vectors")
    p.r(2,rt=14,rd=8,sa=16);p.r(0,rt=8,rd=8,sa=6);p.address(9,"memory");p.r(33,8,9,8);p.i(12,14,9,4095);p.i(9,9,9,-16);p.i(9,0,10,16)
    p.mark("seed_memory");p.r(37,25,9,11);p.i(39,8,15,0);p.i(43,11,15,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(12,9,9,4095);p.i(9,10,10,-1);p.branch(5,10,0,"seed_memory")
    p.i(15,0,9,0xa408);p.i(43,9,0,0);p.i(9,0,8,0xad);p.i(43,26,8,16)
    p.mark("wait");p.i(39,26,8,16);p.i(12,8,8,1);p.branch(4,8,0,"wait")
    p.puts("row");p.i(9,0,22,0)
    p.r(2,rt=12,rd=8,sa=16);p.i(12,8,8,24);p.r(0,rt=8,rd=8,sa=4);p.i(9,8,18,0x600);p.r(37,25,18,18);p.i(9,0,20,16)
    p.mark("group");p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    emit_hex(p);p.i(9,18,18,8);p.i(9,20,20,-1);p.branch(5,20,0,"group")
    p.r(2,rt=12,rd=18,sa=16);p.i(12,18,18,24);p.i(9,18,19,-1);p.i(12,19,19,31);p.i(9,18,18,8);p.i(12,18,18,31)
    for reg in (19,18):
        p.r(0,rt=reg,rd=21,sa=4);p.i(9,21,21,0x600);p.r(37,25,21,21)
        for off in (0,8):p.i(55,21,5,off);emit_hex(p)
    if STORE:
        p.i(12,14,18,4095);p.i(9,18,18,-16);p.i(12,18,18,4095);p.i(9,0,20,8)
        p.mark("memory_capture");p.r(37,25,18,21);p.i(55,21,5,0);emit_hex(p);p.i(9,18,18,8);p.i(12,18,18,4095);p.i(9,20,20,-1);p.branch(5,20,0,"memory_capture")
    # Actual RSP base and zero register snapshots, SP PC/status and MI pending.
    for off in (0x120,0x124):p.i(39,25,5,off);emit_hex(p)
    p.i(15,0,8,0xa408);p.i(39,8,5,0);emit_hex(p);p.i(39,26,5,16);emit_hex(p);p.i(15,0,8,0xa430);p.i(39,8,5,8);emit_hex(p)
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,8);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    finish(p,len(rows),words);p.mark("microcode")
    p.word(0x8c010100);p.word(0x34020400)
    for vt in range(32):p.word((50<<26)|(2<<21)|(vt<<16)|(4<<11)|vt)
    p.word(0) # Actual operation at byte 136.
    for vt in range(32):p.word((58<<26)|(2<<21)|(vt<<16)|(4<<11)|(32+vt))
    p.word(0xac010120);p.word(0xac000124);p.word(13)
    p.mark("table")
    for seed,align,e,kind in rows:
        origin=0xff0 if seed&1 else 0x900
        ins=((58 if STORE else 50)<<26)|(1<<21)|((seed%32)<<16)|(kind<<11)|(e<<7)
        p.word(ins);p.word(origin+align+(seed<<16))
    p.mark("vectors")
    for seed in range(256):
        for vt in range(32):
            for b in range(16):p.data.append((seed+vt*11+b*23+(b//2)*7)%256)
    p.mark("memory")
    for seed in range(256):
        for b in range(64):p.data.append(pattern(seed,b))
    p.resolve();return bytes(p.data)

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
