#!/usr/bin/env python3
"""Original native-profile N64 cartridge that reads FlashRAM silicon ID."""
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
    image[0x20:0x34] = b'VELOCE NATIVE FLASH '
    image[0x3b:0x3e] = b'NCV'
    image[0x3e] = ord('E')
    # The native SP stub switches FlashRAM to ID mode and reads two silicon
    # ID words through actual PI DMA into RDRAM. Guest t6/t7 are written to
    # the debug sink as binary register values; the host formats the file.
    words = [
        0x3c08a801, 0x3c09e100, 0xad090000,
        0x3c0aa460, 0x3c0ba000, 0x356b1000, 0xad4b0000,
        0x3c0c0800, 0xad4c0004, 0x240d0007, 0xad4d000c,
        0x8d6e0000, 0x8d6f0004, 0x3c10bff0,
    ]
    if wrong_capture:
        words.append((14 << 21) | (14 << 16) | 1 | (14 << 26))  # XORI t6,t6,1
    for source in (14, 15):
        for shift in (24, 16, 8, 0):
            if shift:
                words.append((source << 16) | (9 << 11) | (shift << 6) | 2)  # SRL t1, source, shift
            words.append((40 << 26) | (16 << 21) | ((9 if shift else source) << 16))  # SB
    words.extend((0xae000004, 0x1000ffff, 0))
    for index, word in enumerate(words):
        at = 0x40 + index * 4
        image[at:at + 4] = word.to_bytes(4, 'big')
    media.patch_crc(image)
    return bytes(image)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path,
                        default=HERE / 'native_flash_identity.z64')
    args = parser.parse_args()
    args.output.write_bytes(build())
