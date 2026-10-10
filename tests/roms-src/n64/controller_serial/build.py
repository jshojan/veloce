#!/usr/bin/env python3
"""Original SI/PIF digital controller ROM; no SDK or IPL assets."""
from pathlib import Path
import argparse
import hashlib

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


def build(cases=16384, wrong_command=False):
    if not 1 <= cases <= 16384: raise ValueError("invalid case count")
    p = Program()
    p.data[:4] = bytes.fromhex("80371240")
    p.data[8:12] = bytes.fromhex("80000400")
    p.data[0x20:0x34] = b"VELOCE SI ROM       "
    p.data[0x34:0x3c] = b"V64HB001"
    p.i(15, 0, 30, 0xbff0)
    p.i(15, 0, 24, 0xa480)  # SI
    p.i(15, 0, 25, 0xa430)  # MI
    p.i(9, 0, 17, cases)
    p.i(9, 0, 8, 8); p.i(43, 25, 8, 12)  # enable SI mask
    p.puts("header")
    p.mark("sample")
    # Reset command buffers in guest RAM every case. The guest never computes button results.
    for packet, scratch in (("read_packet", 0x20000), ("identify_packet", 0x20040)):
        p.address(16, packet)
        p.i(15, 0, 18, 0x8002); p.i(13, 18, 18, scratch & 0xffff)
        for offset in range(0, 64, 8):
            p.i(55, 16, 19, offset); p.i(63, 18, 19, offset)
        p.i(15, 0, 8, 2); p.i(13, 8, 8, scratch & 0xffff)
        p.i(43, 24, 8, 0)  # DRAM address
        p.i(43, 24, 0, 16)  # DRAM -> PIF
        p.i(43, 24, 0, 24)  # acknowledge completion
        p.i(43, 24, 0, 4)  # PIF -> DRAM
    p.puts("row")
    p.i(15, 0, 18, 0x8002)
    for offset in (0, 8, 64):
        p.i(55, 18, 5, offset); p.jump(3, "hex")
    for reg, offset in ((24,24),(25,8),(25,12)):
        p.i(39, reg, 5, offset); p.jump(3, "hex")
    p.i(43, 24, 0, 24)  # clear SI interrupt and MI pending source
    for reg, offset in ((24,24),(25,8)):
        p.i(39, reg, 5, offset); p.jump(3, "hex")
    p.i(9, 0, 8, 10); p.i(40, 30, 8, 0)
    p.i(43, 30, 0, 12)  # emulator-only yield: one sample per host frame
    p.i(9, 17, 17, -1)
    p.branch(5, 17, 0, "sample")
    p.puts("complete"); p.i(43, 30, 0, 4)
    p.mark("halt"); p.branch(4, 0, 0, "halt")
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


    while p.pc & 7: p.data.append(0)
    p.mark("read_packet")
    packet = bytearray(64)
    packet[:15] = bytes([1,4,0 if wrong_command else 1,0,0,0,0,1,4,1,0xaa,0xaa,0xaa,0xaa,0xfe])
    packet[63] = 1; p.data.extend(packet)
    p.mark("identify_packet")
    packet = bytearray(64); packet[:7] = bytes([1,3,0,0,0,0,0xfe]); packet[63] = 1
    p.data.extend(packet)
    p.string("header", f"VELOCE 1 n64 controller_serial\nBEGIN digital cases={cases} words=8\n")
    p.string("row", "REG r=")
    p.string("complete", f"LOG complete cases={cases}\nEND observations_complete\n")
    while len(p.data) & 3: p.data.append(0)
    p.resolve()
    return bytes(p.data)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path(__file__).with_name("controller_serial.z64"))
    args = parser.parse_args(); rom = build(); args.output.write_bytes(rom)
    print(f"{hashlib.sha256(rom).hexdigest()}  {args.output}")
