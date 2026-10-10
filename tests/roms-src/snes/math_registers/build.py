#!/usr/bin/env python3
"""Emit an original LoROM that observes settled SNES hardware math registers."""
import hashlib
from pathlib import Path
import sys

DIVIDENDS = (0, 1, 2, 127, 128, 254, 255, 256, 257, 32767, 32768, 65534, 65535)


class Program:
    def __init__(self):
        self.data = bytearray([0xEA] * 0x8000)
        self.pos = 0
        self.labels = {}
        self.fixups = []

    def emit(self, *values):
        if self.pos + len(values) > 0x7FC0:
            raise ValueError("code overlaps cartridge header")
        self.data[self.pos:self.pos + len(values)] = bytes(values)
        self.pos += len(values)

    def mark(self, name):
        if name in self.labels:
            raise ValueError(f"duplicate label: {name}")
        self.labels[name] = 0x8000 + self.pos

    def absolute(self, opcode, name):
        self.emit(opcode, 0, 0)
        self.fixups.append((self.pos - 2, name, False))

    def branch(self, opcode, name):
        self.emit(opcode, 0)
        self.fixups.append((self.pos - 1, name, True))

    def text(self, value):
        for byte in value.encode("ascii"):
            self.emit(0xA9, byte, 0x8D, 0xFE, 0x21)

    def next_operand(self, address, limit, loop, done):
        self.emit(0xE6, address, 0xA5, address)
        if limit < 256:
            self.emit(0xC9, limit)
        self.branch(0xF0, done)
        self.absolute(0x4C, loop)
        self.mark(done)

    def resolve(self):
        for offset, name, relative in self.fixups:
            address = self.labels[name]
            if relative:
                delta = address - (0x8000 + offset + 1)
                if not -128 <= delta <= 127:
                    raise ValueError(f"branch out of range: {name}")
                self.data[offset] = delta & 255
            else:
                self.data[offset:offset + 2] = address.to_bytes(2, "little")


def build(operands=256):
    if operands not in (16, 256):
        raise ValueError("operand count must be 16 or 256")
    p = Program()
    p.mark("entry")
    # Reset enters emulation mode: eight-bit A/X/Y, DP/DBR=0.
    p.emit(0x78, 0xD8, 0xA2, 0xFF, 0x9A)  # SEI; CLD; LDX #$FF; TXS
    p.emit(0xA9, 0x80, 0x8D, 0x00, 0x21)  # Forced blank
    p.emit(0xA9, 0)
    for address in (0x4200, 0x420B, 0x420C):
        p.emit(0x8D, address & 255, address >> 8)
    p.text("VELOCE 1 snes math_registers\n")
    p.text(f"INFO operands={operands} dividends={len(DIVIDENDS)}\nREG probe=")
    p.emit(0xAD, 0xFF, 0x21)
    p.absolute(0x20, "hex")
    p.text("\nREG map=")
    p.emit(0xAF, 0xFF, 0x21, 0x80)  # LDA long: mapped I/O mirror
    p.absolute(0x20, "hex")
    # These offsets in WRAM banks must stay ordinary memory, never host I/O.
    for bank, offset, value in ((0x7E, 0xFF, 0x5A), (0x7F, 0xFE, 0xA5)):
        p.emit(0xA9, value, 0x8F, offset, 0x21, bank, 0xAF, offset, 0x21, bank)
        p.absolute(0x20, "hex")
    p.text("\nBEGIN multiply\n")
    # Seed RDDIV=$FFFF so multiplication must replace both quotient bytes.
    p.emit(0xA9, 0xFF, 0x8D, 0x04, 0x42, 0x8D, 0x05, 0x42,
           0xA9, 1, 0x8D, 0x06, 0x42, *([0xEA] * 16))
    p.emit(0xA9, 0, 0x85, 0, 0x85, 1)
    p.mark("mul_loop")
    p.emit(0xA5, 0, 0x8D, 0x02, 0x42, 0xA5, 1, 0x8D, 0x03, 0x42)
    p.emit(*([0xEA] * 8))  # 16 CPU cycles, beyond the eight-cycle multiplication
    # Capture both bytes before printing changes register/open-bus state.
    p.emit(0xAD, 0x16, 0x42, 0x85, 2, 0xAD, 0x17, 0x42, 0x85, 3)
    p.emit(0xAD, 0x14, 0x42, 0x85, 4, 0xAD, 0x15, 0x42, 0x85, 5)
    p.text("REG mul=")
    for address in (3, 2, 5, 4):  # Product then multiplication's RDDIV side effect
        p.emit(0xA5, address)
        p.absolute(0x20, "hex")
    p.text("\n")
    p.next_operand(1, operands, "mul_loop", "mul_rhs_done")
    p.emit(0xA9, 0, 0x85, 1)
    p.next_operand(0, operands, "mul_loop", "mul_done")

    for dividend in DIVIDENDS:
        prefix = f"div_{dividend}"
        p.text(f"BEGIN {prefix}\n")
        p.emit(0xA9, dividend & 255, 0x8D, 0x04, 0x42)
        p.emit(0xA9, dividend >> 8, 0x8D, 0x05, 0x42, 0xA9, 0, 0x85, 1)
        p.mark(prefix + "_loop")
        p.emit(0xA5, 1, 0x8D, 0x06, 0x42)
        p.emit(*([0xEA] * 16))  # 32 CPU cycles, beyond the 16-cycle division
        for register, address in ((0x14, 2), (0x15, 3), (0x16, 4), (0x17, 5)):
            p.emit(0xAD, register, 0x42, 0x85, address)
        p.text("REG div=")
        for address in (3, 2, 5, 4):  # Quotient then remainder
            p.emit(0xA5, address)
            p.absolute(0x20, "hex")
        p.text("\n")
        p.next_operand(1, operands, prefix + "_loop", prefix + "_done")
    p.text(f"LOG complete cases={operands * operands + len(DIVIDENDS) * operands}\n")
    p.emit(0xA9, 1, 0x8D, 0xFF, 0x21)
    p.mark("idle")
    p.absolute(0x4C, "idle")
    p.mark("hex")
    p.emit(0x48, 0x4A, 0x4A, 0x4A, 0x4A, 0xAA)
    p.absolute(0xBD, "digits")
    p.emit(0x8D, 0xFE, 0x21, 0x68, 0x29, 0x0F, 0xAA)
    p.absolute(0xBD, "digits")
    p.emit(0x8D, 0xFE, 0x21, 0x60)
    p.mark("digits")
    p.emit(*b"0123456789abcdef")
    p.resolve()
    p.data[0x7FC0:0x7FE0] = bytes(32)
    p.data[0x7FC0:0x7FD5] = b"VELOCE MATH REGISTERS"
    p.data[0x7FD5:0x7FDA] = bytes((0x20, 0, 5, 0, 1))  # LoROM, ROM-only, 32 KiB, NTSC
    for vector in (0x7FE4, 0x7FE6, 0x7FE8, 0x7FEA, 0x7FEE,
                   0x7FF4, 0x7FF8, 0x7FFA, 0x7FFC, 0x7FFE):
        p.data[vector:vector + 2] = p.labels["entry"].to_bytes(2, "little")
    # Checksum/complement together always contribute 510 to the byte sum.
    checksum = (sum(p.data) + 510) & 65535
    p.data[0x7FDC:0x7FDE] = (checksum ^ 65535).to_bytes(2, "little")
    p.data[0x7FDE:0x7FE0] = checksum.to_bytes(2, "little")
    if len(p.data) != 0x8000:
        raise ValueError("LoROM image must be exactly 32 KiB")
    return bytes(p.data)


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("math_registers.sfc")
    data = build()
    output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {output}")
