#!/usr/bin/env python3
"""Original VR4300 integer ROM, emitted with Python; no IPL or SDK assets."""
from pathlib import Path
import argparse
import hashlib

MASK = (1 << 64) - 1
WORD = (0, 1, 2, 15, 16, 127, 128, 255, 0x7fff, 0x8000, 0x7fffffff,
        0x80000000, 0xfffffffe, 0xffffffff, 0x55555555, 0xaaaaaaaa)
WORD = tuple(v if v < 1 << 31 else v | 0xffffffff00000000 for v in WORD)
WIDE = (0, 1, 2, 15, 0x7fffffff, 0x80000000, 0xffffffff, 0x100000000,
        0x7fffffffffffffff, 0x8000000000000000, MASK, MASK - 1,
        0x5555555555555555, 0xaaaaaaaaaaaaaaaa, 0x0123456789abcdef, 0xfedcba9876543210)
# name, SPECIAL function, domain, observation kind
OPS = (("addu", 0x21, WORD, "binary"), ("subu", 0x23, WORD, "binary"),
       ("daddu", 0x2d, WIDE, "binary"), ("dsubu", 0x2f, WIDE, "binary"),
       ("and", 0x24, WIDE, "binary"), ("or", 0x25, WIDE, "binary"),
       ("xor", 0x26, WIDE, "binary"), ("nor", 0x27, WIDE, "binary"),
       ("slt", 0x2a, WIDE, "binary"), ("sltu", 0x2b, WIDE, "binary"),
       ("sllv", 0x04, WORD, "shift"), ("srlv", 0x06, WORD, "shift"),
       ("srav", 0x07, WORD, "shift"), ("dsllv", 0x14, WIDE, "shift"),
       ("dsrlv", 0x16, WIDE, "shift"), ("dsrav", 0x17, WIDE, "shift"),
       ("mult", 0x18, WORD, "hilo"), ("multu", 0x19, WORD, "hilo"),
       ("div", 0x1a, WORD, "hilo"), ("divu", 0x1b, WORD, "hilo"))


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


def build():
    p = Program()
    p.data[:4] = bytes.fromhex("80371240")
    p.data[8:12] = bytes.fromhex("80000400")
    p.data[0x20:0x34] = b"VELOCE INTEGER ROM  "
    p.data[0x34:0x3c] = b"V64HB001"
    p.i(15, 0, 30, 0xbff0)  # emulator debug register; not retail hardware
    p.puts("header")
    for name, fn, values, kind in OPS:
        table = "word_values" if values == WORD else "wide_values"
        p.puts(name + "_header")
        p.address(16, table)  # source pointer
        p.i(9, 0, 17, 16)  # source remaining
        p.mark(name + "_source")
        p.i(55, 16, 18, 0)  # LD source
        if kind == "shift":
            p.i(9, 0, 19, 0)  # amount
        else:
            p.address(19, table)  # second source pointer
        p.i(9, 0, 20, 256 if kind == "shift" else 16)
        p.mark(name + "_sample")
        if kind == "shift":
            p.r(fn, 19, 18, 21)
        else:
            p.i(55, 19, 22, 0)
            p.r(fn, 18, 22, 21 if kind == "binary" else 0)
            if kind == "hilo":
                p.r(0x12, rd=21)  # MFLO
                p.r(0x10, rd=23)  # MFHI
        p.puts("row")
        p.r(0x25, 21, 0, 5)  # pass observed GPR in a1
        p.jump(3, "hex")
        if kind == "hilo":
            p.r(0x25, 23, 0, 5)
            p.jump(3, "hex")
        p.i(9, 0, 8, 10)
        p.i(40, 30, 8, 0)  # newline
        p.i(9, 19, 19, 1 if kind == "shift" else 8)
        p.i(9, 20, 20, -1)
        p.branch(5, 20, 0, name + "_sample")
        p.i(9, 16, 16, 8)
        p.i(9, 17, 17, -1)
        p.branch(5, 17, 0, name + "_source")
    p.puts("complete")
    p.i(43, 30, 0, 4)  # ungraded channel completion
    p.mark("halt")
    p.branch(4, 0, 0, "halt")

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

    while p.pc & 7:
        p.data.append(0)
    for name, values in (("word_values", WORD), ("wide_values", WIDE)):
        p.mark(name)
        for value in values:
            p.data.extend(value.to_bytes(8, "big"))
    p.string("header", f"VELOCE 1 n64 integer_registers\nINFO patterns=16 shift_amounts=256 groups={len(OPS)}\n")
    p.string("row", "REG r=")
    for name, _, _, kind in OPS:
        count = 16 * (256 if kind == "shift" else 16)
        p.string(name + "_header", f"BEGIN {name} cases={count} words={2 if kind == 'hilo' else 1}\n")
    total = sum(16 * (256 if kind == "shift" else 16) for _, _, _, kind in OPS)
    p.string("complete", f"LOG complete cases={total}\nEND observations_complete\n")
    while len(p.data) & 3:
        p.data.append(0)
    p.resolve()
    return bytes(p.data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path(__file__).with_name("integer_registers.z64"))
    args = parser.parse_args()
    rom = build()
    args.output.write_bytes(rom)
    print(f"{hashlib.sha256(rom).hexdigest()}  {args.output}")
