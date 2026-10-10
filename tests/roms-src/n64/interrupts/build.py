#!/usr/bin/env python3
"""Original VR4300 interrupt gating ROM. No SDK or boot assets."""
from pathlib import Path
import argparse
import hashlib


def domain():
    for software in range(4):
        for si in range(2):
            for timer in range(2):
                for mi in range(2):
                    for control in range(8):
                        for mask in range(256):
                            yield mask, control, software, si, timer, mi


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


def build(cases=65536, wrong_register=False, metadata=False):
    if not 1 <= cases <= 65536: raise ValueError("invalid count")
    p = Program()
    p.data[:4] = bytes.fromhex("80371240"); p.data[8:12] = bytes.fromhex("80000400")
    p.data[0x20:0x34] = b"VELOCE IRQ ROM      "; p.data[0x34:0x3c] = b"V64HB001"
    p.i(15,0,30,0xbff0); p.i(15,0,24,0xa480); p.i(15,0,25,0xa430)
    # Install the original handler at the ordinary general exception vector.
    p.address(10,"handler"); p.i(15,0,11,0x8000); p.i(13,11,11,0x180)
    for offset in range(0,32,4):
        p.i(39,10,8,offset); p.i(43,11,8,offset)
    p.i(15,0,8,0x40); p.i(43,24,8,0)  # DMA scratch outside code/tables: 0x400000
    p.address(16,"cases"); p.i(15,0,17,cases >> 16); p.i(13,17,17,cases & 0xffff)
    p.address(22,"resume")
    p.puts("header")
    p.mark("sample")
    p.word(0x40806000)  # MTC0 zero,Status
    p.i(43,24,0,4)  # DMA completion asserted
    p.i(39,16,8,8); p.branch(5,8,0,"si_ready")
    p.i(43,24,0,24)
    p.mark("si_ready")
    p.i(39,16,8,12); p.i(43,25,8,12)  # MI SI clear/set command
    # Arm the approximate Count clock with IE disabled, then settle the timer.
    p.i(9,0,8,-1); p.word(0x40885800)  # Compare=ffffffff, clear IP7
    p.i(9,0,8,-4); p.word(0x40884800)  # Count=fffffffc
    for _ in range(12): p.word(0)
    p.i(39,16,8,16); p.branch(5,8,0,"timer_ready")
    p.word(0x40805800)  # Compare=0, clear latched timer if not requested
    p.mark("timer_ready")
    p.i(39,16,8,4); p.word(0x40886800)  # software pending bits only
    p.i(15,0,8,0x4444); p.i(13,8,8,0x4444); p.word(0x40887000)  # EPC sentinel
    p.i(9,0,3,0); p.i(9,0,29,0)
    p.i(39,16,8,0); p.word(0x40886800 if wrong_register else 0x40886000)  # enable status
    p.mark("probe")
    p.i(9,0,3,0x1234)
    for rt,rd in ((26,13),(27,12),(28,14)): p.word(0x40000000 | rt << 16 | rd << 11)
    p.word(0x40806000)
    p.mark("resume")
    p.puts("row")
    for reg in (29,3,26,27,28): p.r(0x25,reg,0,5); p.jump(3,"hex")
    for reg,offset in ((24,24),(25,8),(25,12)):
        p.i(39,reg,5,offset); p.jump(3,"hex")
    p.i(9,0,8,10); p.i(40,30,8,0)
    p.i(9,16,16,20); p.i(9,17,17,-1); p.branch(5,17,0,"sample")
    p.puts("complete"); p.i(43,30,0,4)
    p.mark("halt"); p.branch(4,0,0,"halt")
    p.mark("handler")
    for rt,rd in ((26,13),(27,12),(28,14)): p.word(0x40000000 | rt << 16 | rd << 11)
    p.i(9,0,29,1); p.word(0x40806000); p.word(0x40967000)  # EPC <- resume
    p.word(0x42000018); p.word(0)
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



    while p.pc & 3: p.data.append(0)
    p.mark("cases")
    for index,(mask,control,software,si,timer,mi) in enumerate(domain()):
        if index == cases: break
        for word in (mask << 8 | control,software << 8,si,8 if mi else 4,timer): p.word(word)
    p.string("header",f"VELOCE 1 n64 interrupts\nBEGIN gates cases={cases} words=8\n")
    p.string("row","REG r=")
    p.string("complete",f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data) & 3: p.data.append(0)
    p.resolve()
    result=bytes(p.data)
    return (result,dict(probe=p.labels["probe"])) if metadata else result

if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output",nargs="?",type=Path,default=Path(__file__).with_name("interrupts.z64"))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom)
    print(f"{hashlib.sha256(rom).hexdigest()}  {args.output}")
