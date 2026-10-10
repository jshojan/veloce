#!/usr/bin/env python3
"""Original VR4300 doubleword multiply/divide ROM, emitted with Python; no IPL or SDK assets."""
from pathlib import Path
import argparse
import hashlib

MASK = (1 << 64) - 1
BYTE_UNSIGNED = tuple(range(256))
BYTE_SIGNED = tuple(v if v < 128 else v | 0xffffffffffffff00 for v in range(256))
WIDE = (0, 1, 2, 15, 0x7fffffff, 0x80000000, 0xffffffff, 0x100000000,
        0x7fffffffffffffff, 0x8000000000000000, MASK, MASK - 1,
        0x5555555555555555, 0xaaaaaaaaaaaaaaaa, 0x0123456789abcdef, 0xfedcba9876543210)
# group name, SPECIAL function, explicit doubleword input domain
OPS = tuple((name + "_boundary", fn, WIDE) for name, fn in
            (("dmult", 0x1c), ("dmultu", 0x1d), ("ddiv", 0x1e), ("ddivu", 0x1f))) + tuple(
    (name + "_byte", fn, BYTE_UNSIGNED if name.endswith("u") else BYTE_SIGNED)
    for name, fn in (("dmult", 0x1c), ("dmultu", 0x1d), ("ddiv", 0x1e), ("ddivu", 0x1f)))
TOTAL = sum(len(values) ** 2 for _, _, values in OPS)


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
    p.data[0x20:0x34] = b"VELOCE MULDIV64 ROM "
    p.data[0x34:0x3c] = b"V64HB001"
    if len(p.data) != 0x1000:
        raise ValueError("header fields resized the fixed header")
    p.i(15, 0, 30, 0xbff0)  # emulator debug register; not retail hardware
    p.puts("header")
    for name, fn, values in OPS:
        table = "wide_values" if values == WIDE else "unsigned_byte_values" if values == BYTE_UNSIGNED else "signed_byte_values"
        p.puts(name + "_header")
        p.address(16, table)
        p.i(9, 0, 17, len(values))
        p.mark(name + "_source")
        p.i(55, 16, 18, 0)
        p.address(19, table)
        p.i(9, 0, 20, len(values))
        p.mark(name + "_sample")
        p.i(55, 19, 22, 0)
        p.r(fn, 18, 22)
        p.r(0x12, rd=21)  # MFLO
        p.r(0x10, rd=23)  # MFHI
        p.puts("row")
        p.r(0x25, 21, 0, 5)  # pass observed GPR in a1
        p.jump(3, "hex")
        p.r(0x25, 23, 0, 5)
        p.jump(3, "hex")
        p.i(9, 0, 8, 10)
        p.i(40, 30, 8, 0)  # newline
        p.i(9, 19, 19, 8)
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
    for name, values in (("wide_values", WIDE), ("signed_byte_values", BYTE_SIGNED), ("unsigned_byte_values", BYTE_UNSIGNED)):
        p.mark(name)
        for value in values:
            p.data.extend(value.to_bytes(8, "big"))
    p.string("header", f"VELOCE 1 n64 muldiv64\nINFO boundary_patterns=16 byte_operands=256 groups={len(OPS)}\n")
    p.string("row", "REG r=")
    for name, _, values in OPS:
        p.string(name + "_header", f"BEGIN {name} cases={len(values) ** 2} words=2\n")
    p.string("complete", f"LOG complete cases={TOTAL}\nEND observations_complete\n")
    while len(p.data) & 3:
        p.data.append(0)
    p.resolve()
    if len(p.data) & 3 or len(p.data) < 0x1004:
        raise ValueError("bad cartridge image size")
    return bytes(p.data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path(__file__).with_name("muldiv64.z64"))
    args = parser.parse_args()
    rom = build()
    args.output.write_bytes(rom)
    print(f"{hashlib.sha256(rom).hexdigest()}  {args.output}")
