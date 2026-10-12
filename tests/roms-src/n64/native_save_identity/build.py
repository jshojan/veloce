#!/usr/bin/env python3
"""Original native-header cartridge with a synthetic 6102 CRC, no SDK bytes."""
import argparse
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
TARGET_IPL3_CRC = 0x90bb6cb5


def patch_crc(image):
    # Solve the 32-bit linear CRC system for the final four IPL3 bytes. The
    # cartridge contains our own tiny MIPS stub and no commercial IPL3 bytes.
    before = zlib.crc32(image[0x40:0x1000])
    vectors = {}
    for bit in range(32):
        trial = bytearray(image)
        trial[0xffc + bit // 8] ^= 1 << (bit % 8)
        value = zlib.crc32(trial[0x40:0x1000]) ^ before
        mask = 1 << bit
        while value:
            pivot = value.bit_length() - 1
            if pivot not in vectors:
                vectors[pivot] = (value, mask)
                break
            basis, selected = vectors[pivot]
            value ^= basis
            mask ^= selected
    target = TARGET_IPL3_CRC ^ before
    selected = 0
    while target:
        pivot = target.bit_length() - 1
        basis, mask = vectors[pivot]
        target ^= basis
        selected ^= mask
    for bit in range(32):
        if selected & (1 << bit):
            image[0xffc + bit // 8] ^= 1 << (bit % 8)
    assert zlib.crc32(image[0x40:0x1000]) == TARGET_IPL3_CRC


def build():
    image = bytearray(0x1004)
    image[:4] = bytes.fromhex('80371240')
    image[8:12] = bytes.fromhex('80000400')
    image[0x20:0x34] = b'VELOCE NATIVE MEDIA '
    image[0x3b:0x3e] = b'NB7'
    image[0x3e] = ord('E')
    # The SP stub formats a cartridge status packet in uncached RDRAM, runs
    # actual SI write/read DMA, and emits the guest-observed EEPROM type byte.
    words = [
        0x3c08a000, 0x35081000, 0x3c090103, 0xad090004,
        0x240900fe, 0xa109000a, 0x24090001, 0xa109003f,
        0x3c0ba480, 0x240c1000, 0xad6c0000, 0xad600010,
        0xad600018, 0xad600004, 0x910a0008, 0x3c0dbff0,
        0xa1aa0000, 0xada00004, 0x1000ffff, 0,
    ]
    for index, word in enumerate(words):
        at = 0x40 + index * 4
        image[at:at + 4] = word.to_bytes(4, 'big')
    patch_crc(image)
    return bytes(image)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path,
                        default=HERE / 'native_save_identity.z64')
    args = parser.parse_args()
    args.output.write_bytes(build())
