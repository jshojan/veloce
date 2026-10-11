#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original two-port PowerPC SI transaction probe; no SDK or expected answers."""
from pathlib import Path
import argparse
CONSOLE="wii"
def constant(r,v):return [15<<26|r<<21|(v>>16),24<<26|r<<21|r<<16|(v&65535)]
def store(r,base,offset):return 36<<26|r<<21|base<<16|offset
def load(r,base,offset):return 32<<26|r<<21|base<<16|offset
def add(d,a,b):return 31<<26|d<<21|a<<16|b<<11|266<<1
def build(wrong_host=False,wrong_client=False):
    words=constant(12,0xcc006400 if CONSOLE=="gc" else 0xcd006400)+constant(1,0x80001000)
    words += [14<<26|2<<21,14<<26|3<<21]+constant(10,0x40030000)+constant(9,0xc0030801)+[14<<26|6<<21]
    assert len(words)==11
    words += [store(10,12,0x80),store(9,12,0x34),load(4,12,0x80),add(2,2,4),store(2,1,0),store(4,1,8),
              24<<26|9<<21|8<<16|2,store(10,12,0x80),store(8,12,0x34),load(5,12,0x80),add(3,3,5),store(3,1,4),store(5,1,12),
              14<<26|6<<21|6<<16|1,store(6,1,16),31<<26|4<<21|7<<16|5<<11|316<<1,store(7,1,20),
              load(7,12,0x84),store(7,1,24),load(7,12,0x38),store(7,1,28),18<<26|((-84)&0x03fffffc)]
    if wrong_host:words[13]=14<<26|4<<21
    if wrong_client:words[20]=14<<26|5<<21
    code=b''.join(w.to_bytes(4,'big') for w in words)
    return (b'VGCP0001' if CONSOLE=='gc' else b'VWIP0001')+b''.join(v.to_bytes(4,'big') for v in (0x80000000,0x80000000,len(code),0,0,0))+code
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('netplay_probe.'+('vgc' if CONSOLE=='gc' else 'vwi')));a=p.parse_args();a.output.write_bytes(build())
