#!/usr/bin/env python3
"""Original VR4300 merge-memory ROM, emitted with Python; no IPL or SDK assets."""
from pathlib import Path
import argparse
import hashlib

MASK = (1 << 64) - 1
WORD = (0, 1, 2, 15, 16, 127, 128, 255, 0x7fff, 0x8000, 0x7fffffff,
        0x80000000, 0xfffffffe, 0xffffffff, 0x55555555, 0x89abcdef)
WORD = tuple(v if v < 1 << 31 else v | 0xffffffff00000000 for v in WORD)
WIDE = (0, 1, 2, 15, 0x7fffffff, 0x80000000, 0xffffffff, 0x100000000,
        0x7fffffffffffffff, 0x8000000000000000, MASK, MASK - 1,
        0x5555555555555555, 0xaaaaaaaaaaaaaaaa, 0x0123456789abcdef, 0xfedcba9876543210)
GUARD = 0x13579bdf2468ace0
# name, primary opcode, width, store, left
SPECS = (("lwl", 0x22, 4, False, True), ("lwr", 0x26, 4, False, False),
         ("ldl", 0x1a, 8, False, True), ("ldr", 0x1b, 8, False, False),
         ("swl", 0x2a, 4, True, True), ("swr", 0x2e, 4, True, False),
         ("sdl", 0x2c, 8, True, True), ("sdr", 0x2d, 8, True, False))


def groups(byte_values=256):
    if byte_values not in (16, 256):
        raise ValueError("byte domain must be 16 or 256")
    result = []
    for domain in ("boundary", "byte"):
        for op, opcode, width, store, left in SPECS:
            if domain == "boundary":
                values = WORD if width == 4 else WIDE
            else:
                values = tuple(v * ((1 << (width * 8)) - 1) // 255 for v in range(byte_values))
                if width == 4:
                    values = tuple(v if v < 1 << 31 else v | 0xffffffff00000000 for v in values)
            for lane in range(width):
                result.append((f"{op}_{domain}_lane{lane}", op, opcode, width, store, left, lane, values))
    return tuple(result)


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


def build(byte_values=256):
    domain = groups(byte_values)
    p = Program()
    p.data[:4] = bytes.fromhex("80371240")
    p.data[8:12] = bytes.fromhex("80000400")
    p.data[0x20:0x34] = b"VELOCE MERGE ROM    "
    p.data[0x34:0x3c] = b"V64HB001"
    if len(p.data) != 0x1000:
        raise ValueError("header fields resized the fixed header")
    p.i(15, 0, 30, 0xbff0)  # emulator debug register; not retail hardware
    p.puts("header")
    p.i(15, 0, 24, 0x8004)  # scratch RAM, outside code/tables
    p.address(4, "guard")
    p.i(55, 4, 26, 0)
    for name, op, opcode, width, store, left, lane, values in domain:
        table = ("word_values" if width == 4 else "wide_values") if "_boundary_" in name else ("word_bytes" if width == 4 else "wide_bytes")
        p.puts(name + "_header")
        p.address(16, table)
        p.i(9, 0, 17, len(values))
        p.mark(name + "_memory")
        p.i(55, 16, 18, 0)
        p.address(19, table)
        p.i(9, 0, 20, len(values))
        p.mark(name + "_sample")
        p.i(55, 19, 22, 0)  # observed input/result GPR
        p.i(63, 24, 26, 8)  # full eight-byte sentinel before each trial
        p.i(43 if width == 4 else 63, 24, 18, 8)
        p.i(opcode, 24, 22, 8 + lane)
        if store:
            p.i(55, 24, 21, 8)  # whole eight-byte snapshot, including word guard
        else:
            p.r(0x25, 22, 0, 21)
        p.puts("row")
        p.r(0x25, 21, 0, 5)
        p.jump(3, "hex")
        p.i(9, 0, 8, 10)
        p.i(40, 30, 8, 0)
        p.i(9, 19, 19, 8)
        p.i(9, 20, 20, -1)
        p.branch(5, 20, 0, name + "_sample")
        p.i(9, 16, 16, 8)
        p.i(9, 17, 17, -1)
        p.branch(5, 17, 0, name + "_memory")
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
    for name, values in (("word_values", WORD), ("wide_values", WIDE),
                         ("word_bytes", tuple(v * 0x01010101 if v < 128 else v * 0x01010101 | 0xffffffff00000000 for v in range(byte_values))),
                         ("wide_bytes", tuple(v * 0x0101010101010101 for v in range(byte_values))),
                         ("guard", (GUARD,))):
        p.mark(name)
        for value in values:
            p.data.extend(value.to_bytes(8, "big"))
    p.string("header", f"VELOCE 1 n64 merge_memory\nINFO boundary_patterns=16 byte_values={byte_values} groups={len(domain)}\n")
    p.string("row", "REG r=")
    for name, _, _, _, _, _, _, values in domain:
        p.string(name + "_header", f"BEGIN {name} cases={len(values) ** 2} words=1\n")
    total = sum(len(values) ** 2 for *_, values in domain)
    p.string("complete", f"LOG complete cases={total}\nEND observations_complete\n")
    while len(p.data) & 3:
        p.data.append(0)
    p.resolve()
    if len(p.data) & 3 or len(p.data) < 0x1004:
        raise ValueError("bad cartridge image size")
    return bytes(p.data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path(__file__).with_name("merge_memory.z64"))
    args = parser.parse_args()
    rom = build()
    args.output.write_bytes(rom)
    print(f"{hashlib.sha256(rom).hexdigest()}  {args.output}")
