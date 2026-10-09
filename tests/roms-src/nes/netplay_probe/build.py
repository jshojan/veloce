#!/usr/bin/env python3
"""Original NROM: repeatedly poll both controllers and accumulate observed bytes."""
import hashlib
from pathlib import Path
import sys


def build():
    code = bytearray()
    labels, fixups = {}, []

    def emit(*values):
        code.extend(values)

    def mark(name):
        labels[name] = len(code)

    def jump(opcode, name, relative=False):
        emit(opcode, 0) if relative else emit(opcode, 0, 0)
        fixups.append((len(code) - (1 if relative else 2), name, relative))

    emit(0x78, 0xD8, 0xA2, 0xFF, 0x9A)  # SEI; CLD; SP=$FF
    emit(0xA9, 0)
    for address in (0x2000, 0x2001, 0x4010, 0x4015):
        emit(0x8D, address & 255, address >> 8)
    emit(0xA9, 0x40, 0x8D, 0x17, 0x40)  # Disable frame-counter IRQ
    emit(0xA9, 0)
    for address in (0x10, 0x11, 0x12, 0x13):
        emit(0x85, address)
    mark("poll")
    emit(0xA9, 1, 0x8D, 0x16, 0x40, 0xA9, 0, 0x8D, 0x16, 0x40)
    emit(0x85, 0x10, 0x85, 0x11, 0xA2, 8)
    mark("bit")
    for port, observed in ((0x16, 0x10), (0x17, 0x11)):
        emit(0xAD, port, 0x40, 0x29, 1, 0x4A, 0x66, observed)
    emit(0xCA)
    jump(0xD0, "bit", True)
    for observed, accumulator in ((0x10, 0x12), (0x11, 0x13)):
        emit(0x18, 0xA5, accumulator, 0x65, observed, 0x85, accumulator)
    jump(0x4C, "poll")
    for offset, name, relative in fixups:
        if relative:
            delta = labels[name] - (offset + 1)
            if not -128 <= delta <= 127:
                raise ValueError("branch out of range")
            code[offset] = delta & 255
        else:
            code[offset:offset + 2] = (0xC000 + labels[name]).to_bytes(2, "little")
    if len(code) > 0x3FFA:
        raise ValueError("code overlaps vectors")
    rom = bytearray([0xEA] * 0x4000)
    rom[:len(code)] = code
    for offset in (0x3FFA, 0x3FFC, 0x3FFE):
        rom[offset:offset + 2] = (0xC000).to_bytes(2, "little")
    return b"NES\x1a" + bytes((1, 0)) + bytes(10) + bytes(rom)


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("netplay_probe.nes")
    data = build()
    output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {output}")
