#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original fixed-loop AArch64 two-input netplay probe, native uncompressed NSO0."""
import argparse,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
def build(wrong_host=False,wrong_client=False):
    # Eight setup attempts and a fourteen-attempt loop. No SDK or expected results.
    words=[0xd2bfffe0,0xd2820001,0x52800002,0x52800003,0x52800006,0x5299bde7,0x72b13567,0xb9001c27]
    # Host/client reads at attempts 8/9; each frame supplies 100,000 attempts.
    words += [0xb9400c04,0xb9401405,0x0b040042,0x0b050063,0xb9000022,0xb9000423,0xb9000824,0xb9000c25,
              0x110004c6,0xb9001026,0x4a050087,0xb9001427,0xb900183f,0x17fffff3]
    if wrong_host:words[8]=0x52800004
    if wrong_client:words[9]=0x52800005
    code=b''.join(struct.pack('<I',w) for w in words);header=bytearray(256);header[:4]=b'NSO0'
    for n,(offset,address,data) in enumerate(((256,0,code),(256+len(code),0x2000,b''),(256+len(code),0x1000,b'\0'*32))):struct.pack_into('<III',header,16+n*16,offset,address,len(data))
    return bytes(header+code+b'\0'*32)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/'netplay_probe.nso');args=parser.parse_args();args.output.write_bytes(build())
