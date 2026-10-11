#!/usr/bin/env python3
"""Original VI setup and CURRENT-to-RAM loop; MIT, no SDK/firmware bytes."""
import argparse
from pathlib import Path

def build(wrong_read=False):
    image=bytearray(0x1004)
    image[:4]=bytes.fromhex('80371240')
    image[8:12]=bytes.fromhex('80000400')
    image[0x20:0x34]=b'VELOCE VI FIELDS    '
    image[0x34:0x3c]=b'V64IP001'
    words=[]
    def i(op,rs,rt,value):words.append(op<<26|rs<<21|rt<<16|(value&65535))
    i(15,0,11,0xa440)
    i(9,0,8,3093);i(43,11,8,28)
    i(15,0,8,3093);i(13,8,8,3093);i(43,11,8,32)
    i(9,0,8,525);i(43,11,8,24)
    i(9,0,8,2);i(43,11,8,0)
    i(15,0,10,0xa001)
    i(35,11,9,8 if wrong_read else 16)
    i(43,10,9,0)
    i(4,0,0,-3);words.append(0)
    for n,word in enumerate(words):image[0x40+n*4:0x44+n*4]=word.to_bytes(4,'big')
    return bytes(image)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('vi_field_schedule.z64'))
    a=p.parse_args();a.output.write_bytes(build())
