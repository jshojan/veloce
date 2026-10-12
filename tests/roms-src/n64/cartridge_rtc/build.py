#!/usr/bin/env python3
"""Original native-profile N64 Joybus RTC status, write and read cartridge."""
import argparse
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('native_media', HERE.parent / 'native_save_identity/build.py')
media = importlib.util.module_from_spec(spec)
spec.loader.exec_module(media)


def build(wrong_capture=False):
    image = bytearray(0x1004)
    image[:4] = bytes.fromhex('80371240')
    image[8:12] = bytes.fromhex('80000400')
    image[0x20:0x34] = b'VELOCE NATIVE RTC   '
    image[0x3b:0x3e] = b'NAF'
    image[0x3e] = ord('J')
    code = []

    def i(op, rs, rt, imm):
        code.append((op << 26) | (rs << 21) | (rt << 16) | (imm & 0xffff))

    def lui(rt, imm): i(15, 0, rt, imm)
    def ori(rt, rs, imm): i(13, rs, rt, imm)
    def sw(rt, base, off): i(43, base, rt, off)
    def sb(rt, base, off): i(40, base, rt, off)
    def lbu(rt, base, off): i(36, base, rt, off)

    # t0=uncached PIF packet in RDRAM; t1=SI, t2=debug stream.
    lui(8, 0xa000);ori(8, 8, 0x1000)
    lui(9, 0xa480);lui(10, 0xbff0)
    i(9, 0, 11, 0x1000)
    sw(11, 9, 0)

    def dma():
        sw(0, 9, 16)  # RDRAM -> PIF, format
        sw(0, 9, 4)   # PIF -> RDRAM, poll

    def packet(word, terminator):
        lui(11, word >> 16);ori(11, 11, word & 0xffff)
        sw(11, 8, 4)
        i(9, 0, 11, 0xfe);sb(11, 8, terminator)
        i(9, 0, 11, 1);sb(11, 8, 63)
        dma()

    # Cartridge RTC status: three returned bytes at offsets 7-9.
    packet(0x01030600, 10)
    for at in range(7, 10):
        lbu(12, 8, at)
        if wrong_capture and at == 8:ori(12, 12, 1)
        sb(12, 10, 0)

    # Set 2024-02-28 Wednesday 23:59:59 using block-2 BCD bytes.
    lui(11, 0x0a01);ori(11, 11, 0x0802);sw(11, 8, 4)
    lui(11, 0x5959);ori(11, 11, 0xa328);sw(11, 8, 8)
    lui(11, 0x0302);ori(11, 11, 0x2401);sw(11, 8, 12)
    i(9, 0, 11, 0xfe);sb(11, 8, 17)
    i(9, 0, 11, 1);sb(11, 8, 63)
    dma()
    lbu(12, 8, 16);sb(12, 10, 0)  # write status

    # Read block 2; eight BCD bytes and one status byte.
    packet(0x02090702, 17)
    for at in range(8, 17):
        lbu(12, 8, at);sb(12, 10, 0)
    sw(0, 10, 4)
    code.extend((0x1000ffff, 0))
    for index, word in enumerate(code):
        at = 0x40 + index * 4
        image[at:at + 4] = word.to_bytes(4, 'big')
    media.patch_crc(image)
    return bytes(image)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path,
                        default=HERE / 'cartridge_rtc.z64')
    args = parser.parse_args()
    args.output.write_bytes(build())
