#!/usr/bin/env python3
"""Build an original ARM register-shift ROM using only Python byte emission."""
import hashlib
from pathlib import Path
import sys

VALUES = (0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, 0xFFFF,
          0x10000, 0x7FFFFFFF, 0x80000000, 0x80000001,
          0xFFFFFFFE, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA)
SHIFTS = ("lsl", "lsr", "asr", "ror")
GROUPS = tuple((op, value, carry, overflow) for op in SHIFTS for value in VALUES
               for carry in (0, 1) for overflow in (0, 1))


def group_name(op, value, carry, overflow):
    return f"{op}_{value:08x}_c{carry}_v{overflow}"


def immediate(value):
    for rotation in range(16):
        bits = rotation * 2
        candidate = ((value << bits) | (value >> (32 - bits if bits else 32))) & 0xFFFFFFFF
        if candidate < 256:
            return (rotation << 8) | candidate
    raise ValueError(f"not an ARM immediate: {value:08x}")


class Program:
    def __init__(self):
        self.data = bytearray(0x40000)
        self.pos = 0xC0
        self.labels = {}
        self.fixups = []

    def emit(self, word):
        if self.pos + 4 > len(self.data):
            raise ValueError("program exceeds ROM size")
        self.data[self.pos:self.pos + 4] = word.to_bytes(4, "little")
        self.pos += 4

    def mark(self, name):
        if name in self.labels:
            raise ValueError(f"duplicate label: {name}")
        self.labels[name] = self.pos

    def branch(self, name, condition=14, link=False):
        self.fixups.append((self.pos, name, condition, link))
        self.emit(0)

    def mov32(self, register, value):
        self.emit(0xE3A00000 | (register << 12) | immediate(value & 255))
        for shift in (8, 16, 24):
            chunk = value & (255 << shift)
            if chunk:
                self.emit(0xE3800000 | (register << 16) | (register << 12) | immediate(chunk))

    def halfword(self, register, base, offset=0, load=False):
        if not 0 <= offset < 256:
            raise ValueError("halfword offset out of range")
        self.emit(0xE1C000B0 | (int(load) << 20) | (base << 16) | (register << 12)
                  | ((offset & 0xF0) << 4) | (offset & 15))

    def buffer(self, text):
        data = text.encode("ascii") + b"\0"
        if len(data) % 2:
            data += b"\0"
        for offset in range(0, len(data), 2):
            self.mov32(0, int.from_bytes(data[offset:offset + 2], "little"))
            self.halfword(0, 10, offset)

    def flush(self):
        self.mov32(0, 0x103)  # mGBA info level and send bit
        self.halfword(0, 11)

    def text(self, value):
        self.buffer(value)
        self.flush()

    def resolve(self):
        for offset, name, condition, link in self.fixups:
            delta = self.labels[name] - (offset + 8)
            if delta % 4 or not -(1 << 25) <= delta < (1 << 25):
                raise ValueError(f"branch out of range: {name}")
            word = (condition << 28) | 0x0A000000 | (int(link) << 24) | ((delta // 4) & 0xFFFFFF)
            self.data[offset:offset + 4] = word.to_bytes(4, "little")

    def rom(self):
        self.resolve()
        self.data[:4] = (0xEA00002E).to_bytes(4, "little")  # B $080000C0
        # No copied Nintendo logo: this ROM targets direct cartridge-entry boot.
        self.data[0xA0:0xAC] = b"VELOCE SHIFT"
        self.data[0xAC:0xB0] = b"VSHF"
        self.data[0xB0:0xB3] = b"00\x96"
        self.data[0xBD] = (-sum(self.data[0xA0:0xBD]) - 0x19) & 255
        if len(self.data) != 0x40000:
            raise ValueError("ROM must be exactly 256 KiB")
        return bytes(self.data)


def build_channel_probe():
    """Exercise disabled output and the last byte of the 256-byte buffer."""
    p = Program()
    p.emit(0xE321F0DF)
    p.mov32(10, 0x04FFF600)
    p.mov32(11, 0x04FFF700)
    p.mov32(12, 0x04FFF780)
    p.text("DISABLED output must not reach sink")
    p.mov32(0, 0xC0DE)
    p.halfword(0, 12)
    # The final halfword writes character #255 and a NUL in byte #256.
    p.text("A" * 255)
    p.text("INFO enabled")
    p.mov32(0, 0)
    p.halfword(0, 12)
    p.text("DISABLED output must not reach sink")
    p.mov32(0, 1)
    p.halfword(0, 12, 2)
    p.mark("idle")
    p.branch("idle")
    return p.rom()


def build(amounts=256):
    if amounts not in (16, 256):
        raise ValueError("amount count must be 16 or 256")
    p = Program()
    p.mark("entry")
    p.emit(0xE321F0DF)  # MSR CPSR_c,#$DF: System ARM, IRQ/FIQ disabled
    p.mov32(10, 0x04FFF600)
    p.mov32(11, 0x04FFF700)
    p.mov32(12, 0x04FFF780)
    p.mov32(0, 0xC0DE)
    p.halfword(0, 12)
    p.text("VELOCE 1 gba arm_shifts")
    p.text(f"INFO amounts={amounts} groups={len(GROUPS)}")
    p.buffer("REG probe=")
    p.halfword(0, 12, load=True)
    p.mov32(5, 10)
    p.branch("hex", link=True)
    p.flush()

    for op, value, carry, overflow in GROUPS:
        group = group_name(op, value, carry, overflow)
        p.text(f"BEGIN {group}")
        p.mov32(1, value)  # Operand
        p.mov32(2, 0)  # Register-specified amount
        p.mov32(4, (carry << 29) | (overflow << 28))
        p.mark(group)
        p.emit(0xE128F004)  # MSR CPSR_f,r4: seed flags
        p.emit(0xE1B00211 | (SHIFTS.index(op) << 5))  # MOVS r0,r1,shift r2
        p.emit(0xE10F3000)  # MRS r3,CPSR immediately after tested instruction
        p.emit(0xE1A09000)  # Preserve observed result in r9 while printing prefix
        p.buffer("REG rp=")
        p.mov32(5, 7)
        p.emit(0xE1A00009)  # MOV r0,r9
        p.branch("hex", link=True)
        p.emit(0xE1A00003)  # MOV r0,r3
        p.branch("hex", link=True)
        p.flush()
        p.emit(0xE2822001)  # ADD r2,r2,#1
        p.emit(0xE3520000 | immediate(amounts))  # CMP r2,#limit
        p.branch(group, condition=1)  # BNE
    p.text(f"LOG complete cases={len(GROUPS) * amounts}")
    p.mov32(0, 1)
    p.halfword(0, 12, 2)  # Veloce-only $04FFF782 unknown-status completion hint
    p.mark("idle")
    p.branch("idle")

    # Hex leaf subroutine: r0 input, r5 buffer offset; clobbers r6-r8 and flags.
    p.mark("hex")
    p.mov32(6, 8)
    p.emit(0xE1A07000)  # MOV r7,r0
    p.mark("hex_loop")
    p.emit(0xE1A08E27)  # MOV r8,r7,LSR #28
    p.emit(0xE358000A)  # CMP r8,#10
    p.emit(0xE2888030)  # ADD r8,r8,#'0' (preserve CMP flags)
    p.emit(0xA2888027)  # ADDGE r8,r8,#39
    p.emit(0xE7CA8005)  # STRB r8,[r10,r5]
    p.emit(0xE2855001)  # ADD r5,r5,#1
    p.emit(0xE1A07207)  # MOV r7,r7,LSL #4
    p.emit(0xE2566001)  # SUBS r6,r6,#1
    p.branch("hex_loop", condition=1)
    p.mov32(8, 0)
    p.emit(0xE7CA8005)  # NUL terminate at next byte
    p.emit(0xE12FFF1E)  # BX LR
    return p.rom()


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("arm_shifts.gba")
    data = build()
    output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {output}")
