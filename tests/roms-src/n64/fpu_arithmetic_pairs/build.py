#!/usr/bin/env python3
"""Original FPU arithmetic register-capture program; MIT, no SDK or firmware bytes."""
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


SUITE='fpu_arithmetic_pairs'
SENTINEL=0x12345678a5b6c7d8
def integer_bits(n,fmt):
    if not n:return 0
    f,bias,eb=(23,127,8) if fmt==16 else (52,1023,11)
    mag=abs(n);exponent=mag.bit_length()-1
    return (int(n<0)<<(f+eb))+((bias+exponent)<<f)+((mag<<(f-exponent))%(1<<f))
def cases(short=False):
    if SUITE=="fpu_arithmetic_pairs":
        for fmt in (16,17):
            for a in range(-2 if short else -32,2 if short else 32):
                for b in range(-2 if short else -32,2 if short else 32):
                    for fn in range(4):
                        for rm in range(4):yield fmt,fn,integer_bits(a,fmt),integer_bits(b,fmt),rm
    else:
        for fmt,f,eb,bias in ((16,23,8,127),(17,52,11,1023)):
            one=bias<<f
            for exponent in range(8 if short else 1<<eb):
                tail=(0,1,(1<<f)-1,1<<(f-1))[exponent%4]
                for negative in range(2):
                    a=(negative<<(f+eb))+(exponent<<f)+tail
                    for fn in range(8):
                        for rm in range(4):yield fmt,fn,a,one,rm
            if short:continue
            # Special operands in both positions, sign combinations and payloads.
            specials=(0,1,(1<<f)-1,1<<f,(1<<f)+1,one,one+1,one+(1<<(f-1)),(bias+1)<<f,((1<<eb)-1)<<f,(((1<<eb)-1)<<f)+1,(((1<<eb)-1)<<f)+(1<<(f-1)))
            for a in specials:
                for b in specials:
                    for signs in range(4):
                        for fn in range(4):
                            for rm in range(4):yield fmt,fn,a+(signs%2<<(f+eb)),b+(signs//2<<(f+eb)),rm
            # All controls on exact, rounded, invalid, assist, zero-divide,
            # tiny, rescued-tiny and overflow profiles, including unary forms.
            profiles=((one,one),(one,(bias+3)<<f),(((1<<eb)-2)<<f, (bias-1)<<f),(1<<f,(bias+1)<<f),((1<<f)+1,one+1),(one,0),(0,0),(1,one),((((1<<eb)-1)<<f)+1,one),((((1<<eb)-1)<<f)+(1<<(f-1)),1),((bias+1)<<f,one),((bias+1)<<f|(1<<(f+eb)),one))
            for a,b in profiles:
                for fn in range(8):
                    for rm in range(4):
                        for enables in range(32):
                            for flush in range(2):yield fmt,fn,a,b,rm+enables*128+flush*0x1000000+0x80007c
        if not short:
            for fmt in range(18,32):
                for fn in range(8):yield fmt,fn,0x123456789abcdef0,0,0

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=Program();p.data[:4]=bytes.fromhex("80371240");p.data[8:12]=bytes.fromhex("80000400")
    p.data[0x20:0x34]=b"VELOCE FPU ARITHMET ";p.data[0x34:0x3c]=b"V64HB001";p.i(15,0,30,0xbff0)
    # Original recovery handler at the ordinary kernel vector; no firmware bytes.
    p.address(8,"handler");p.i(15,0,9,0x8000);p.i(13,9,9,0x180);p.i(9,0,10,10)
    p.mark("copy");p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,"copy")
    p.i(15,0,8,0x2400);p.word(0x40886000);p.word(0);p.word(0)
    p.address(16,"table");p.i(15,0,17,len(rows)>>16);p.i(13,17,17,len(rows)&65535);p.puts("header")
    p.mark("sample");p.i(55,16,12,8);p.i(55,16,23,16);p.i(39,16,13,4);p.i(39,16,14,0)
    p.address(8,"operation");p.i(43,8,14,0);p.i(9,0,20,0);p.i(9,0,21,0)
    p.word(0x44ac1000);p.word(0);p.word(0) # DMTC1 source f2
    p.word(0x44b73000);p.word(0);p.word(0) # DMTC1 source f6
    p.address(8,"sentinel");p.i(55,8,15,0);p.word(0x44af2000);p.word(0);p.word(0)
    p.word(0x44cdf800);p.word(0);p.word(0) # CTC1 clean control
    p.jump(3,"operation");p.word(0x442e2000);p.word(0);p.word(0) # DMFC1 actual destination
    if wrong_capture:p.i(14,14,14,1)
    p.word(0x4453f800);p.word(0);p.word(0) # CFC1 actual FCSR
    p.word(0x44361000);p.word(0);p.word(0) # DMFC1 unchanged source
    p.word(0x44383000);p.word(0);p.word(0) # DMFC1 unchanged right operand
    p.puts("row")
    for reg in (14,19,20,21,22,24):p.r(0x25,reg,0,5);p.jump(3,"hex64")
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,24);p.i(9,17,17,-1);p.branch(5,17,0,"sample")
    p.puts("complete");p.i(43,30,0,4);p.mark("halt");p.branch(4,0,0,"halt")
    p.mark("operation");p.word(0);p.r(8,rs=31);p.word(0)
    p.mark("handler");p.word(0x401b6800);p.i(12,27,21,124);p.i(9,0,20,1);p.word(0x401b7000);p.i(9,27,27,4);p.word(0x409b7000);p.word(0);p.word(0);p.word(0x42000018);p.word(0)
    p.mark("puts");p.i(36,4,8,0);p.branch(4,8,0,"puts_return");p.i(40,30,8,0);p.i(9,4,4,1);p.branch(4,0,0,"puts")
    p.mark("puts_return");p.r(8,rs=31);p.word(0)
    p.mark("hex64");p.i(9,0,6,60);p.mark("hex_loop");p.r(0x16,6,5,7);p.i(12,7,7,15);p.i(11,7,8,10);p.branch(5,8,0,"digit");p.i(9,7,7,39)
    p.mark("digit");p.i(9,7,7,48);p.i(40,30,7,0);p.i(9,6,6,-4);p.branch(1,6,1,"hex_loop");p.r(8,rs=31);p.word(0)
    p.string("header",f"VELOCE 1 n64 {SUITE}\nBEGIN data cases={len(rows)} words=6\n");p.string("row","REG r=");p.string("complete",f"LOG complete cases={len(rows)}\nEND observations_complete\n")
    while len(p.data)&7:p.data.append(0)
    p.mark("sentinel");p.data.extend(SENTINEL.to_bytes(8,"big"));p.mark("table")
    for fmt,fn,value,right,control in rows:
        ins=0x44000000|(fmt<<21)|((6 if fn<4 else 0)<<16)|(2<<11)|(4<<6)|fn;p.data.extend(ins.to_bytes(4,"big"));p.data.extend(control.to_bytes(4,"big"));p.data.extend(value.to_bytes(8,"big"));p.data.extend(right.to_bytes(8,"big"))
    p.resolve();return bytes(p.data)
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name(SUITE+".z64"));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
