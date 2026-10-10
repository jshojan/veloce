#!/usr/bin/env python3
"""Emit an original 32 KiB SM83 unary corpus using only Python."""
from pathlib import Path
import argparse
import hashlib

OPS = (("inc", (0x3C,)), ("dec", (0x3D,)), ("daa", (0x27,)),
       ("cpl", (0x2F,)), ("scf", (0x37,)), ("ccf", (0x3F,)),
       ("rlca", (0x07,)), ("rrca", (0x0F,)), ("rla", (0x17,)), ("rra", (0x1F,)),
       ("rlc", (0xCB, 0x07)), ("rrc", (0xCB, 0x0F)),
       ("rl", (0xCB, 0x17)), ("rr", (0xCB, 0x1F)),
       ("sla", (0xCB, 0x27)), ("sra", (0xCB, 0x2F)),
       ("swap", (0xCB, 0x37)), ("srl", (0xCB, 0x3F)))
GROUPS = tuple((op, flags) for op, _ in OPS for flags in range(0, 256, 16))


def group_name(op, flags):
    return f"{op}_f{flags:02x}"


class Rom:
    def __init__(self):
        self.data = bytearray(0x8000)
        self.pos = 0x150
        self.labels = {}
        self.fixups = []

    def mark(self, name):
        if name in self.labels:
            raise ValueError(f"duplicate label: {name}")
        self.labels[name] = self.pos

    def emit(self, *values):
        for value in values:
            if self.pos >= 0x8000:
                raise ValueError("ROM exceeds unbanked image")
            self.data[self.pos] = value
            self.pos += 1

    def address(self, opcode, name):
        self.emit(opcode, 0, 0)
        self.fixups.append((self.pos - 2, name, False))

    def relative(self, opcode, name):
        self.emit(opcode, 0)
        self.fixups.append((self.pos - 1, name, True))

    def string(self, name, text):
        self.mark(name)
        self.emit(*text.encode("ascii"), 0)

    def puts(self, name):
        self.address(0x21, name)  # LD HL, string
        self.address(0xCD, "puts")

    def resolve(self):
        for offset, name, relative in self.fixups:
            address = self.labels[name]
            if relative:
                delta = address - offset - 1
                if not -128 <= delta <= 127:
                    raise ValueError(f"JR out of range: {name}")
                self.data[offset] = delta & 255
            else:
                self.data[offset:offset + 2] = address.to_bytes(2, "little")


def build(operands=256):
    if operands not in (16, 256):
        raise ValueError("supported operand domains: 16 or 256")
    row_cases = min(32, operands)
    rom = Rom()
    rom.data[0x100:0x104] = bytes((0xC3, 0x50, 0x01, 0))
    rom.data[0x134:0x144] = b"VELOCE UNARY ROM"
    rom.data[0x147:0x14A] = bytes((0, 0, 0))  # ROM only; 32 KiB; no external RAM
    # No interrupts, LCD, or sound; stack in WRAM. Serial is the debug channel.
    rom.emit(0xF3, 0x31, 0xFE, 0xDF, 0xAF, 0xE0, 0x40, 0xE0, 0x26, 0xEA, 0xFF, 0xFF)
    rom.puts("header")
    for op, opcode in OPS:
        for flags in range(0, 256, 16):
            name = group_name(op, flags)
            rom.puts(name + "_begin")
            rom.emit(0x16, 0)  # D=input accumulator
            rom.mark(name + "_lhs")
            rom.emit(0x7A, 0xE6, row_cases - 1)  # A=D; AND row mask
            rom.relative(0x20, name + "_sample")
            rom.puts("row")
            rom.mark(name + "_sample")
            # Load the actual A/F inputs through POP AF. Instructions between
            # restoration and observation do not modify flags (except the stimulus).
            rom.emit(0x42, 0x0E, flags, 0xC5, 0xF1, *opcode, 0xF5, 0xC1)
            # B/C now contain the observed A/F. Output routines preserve B/C/D/E.
            rom.emit(0x78)
            rom.address(0xCD, "hex")
            rom.emit(0x79)
            rom.address(0xCD, "hex")
            rom.emit(0x14, 0x7A, 0xE6, row_cases - 1)  # INC D; row complete?
            rom.relative(0x20, name + "_continue")
            rom.emit(0x3E, 10)
            rom.address(0xCD, "putc")
            rom.mark(name + "_continue")
            rom.emit(0x7A)  # LD A,D
            rom.emit(0xB7) if operands == 256 else rom.emit(0xFE, operands)
            rom.relative(0x20, name + "_lhs")
    rom.puts("complete")
    rom.mark("halt")
    rom.relative(0x18, "halt")

    rom.mark("puts")
    rom.emit(0x2A, 0xB7, 0xC8)  # A=(HL+); OR A; RET Z
    rom.address(0xCD, "putc")
    rom.relative(0x18, "puts")
    rom.mark("putc")
    # The established emulator channel observes SB when SC=$81 is written.
    # Do not wait for physical serial completion: link timing is not measured.
    rom.emit(0xE0, 0x01, 0x3E, 0x81, 0xE0, 0x02, 0xC9)
    rom.mark("hex")
    rom.emit(0xF5, 0xCB, 0x37, 0xE6, 0x0F)
    rom.address(0xCD, "nibble")
    rom.emit(0xF1, 0xE6, 0x0F)
    rom.address(0xCD, "nibble")
    rom.emit(0xC9)
    rom.mark("nibble")
    rom.emit(0xFE, 10)
    rom.relative(0x38, "digit")
    rom.emit(0xC6, 0x57)  # 10 -> 'a'
    rom.address(0xC3, "putc")
    rom.mark("digit")
    rom.emit(0xC6, 0x30)
    rom.address(0xC3, "putc")
    rom.string("header", f"VELOCE 1 gb unary_exhaustive\nINFO operands={operands} groups={len(GROUPS)}\n")
    rom.string("row", "REG af=")
    for op, flags in GROUPS:
        name = group_name(op, flags)
        rom.string(name + "_begin", f"BEGIN {name}\n")
    # Completion declares no pass score. Only the host grades the observations.
    rom.string("complete", f"LOG complete cases={len(GROUPS) * operands}\nEND observations_complete\n")
    rom.resolve()
    rom.data[0x14D] = (-sum(rom.data[0x134:0x14D]) - 25) & 255
    checksum = (sum(rom.data[:0x14E]) + sum(rom.data[0x150:])) & 0xFFFF
    rom.data[0x14E:0x150] = checksum.to_bytes(2, "big")
    if len(rom.data) != 0x8000:
        raise ValueError("unbanked ROM must be exactly 32 KiB")
    return bytes(rom.data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=Path(__file__).with_name("unary_exhaustive.gb"))
    args = parser.parse_args()
    data = build()
    args.output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {args.output}")
