#!/usr/bin/env python3
"""Original mapped-user/supervisor doubleword instruction corpus; MIT, no SDK."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'integer_traps/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
FUNCTIONS=(20,22,23,28,29,30,31,44,45,46,47,56,58,59,60,62,63)
MAJORS=(24,25,26,27,39,44,45,52,55,60,63)
STATUSES=(0x10000008,0x100000a8,0x10000010,0x100000d0)
def specials():
    for fn in FUNCTIONS:
        for sa in range(32) if fn>=56 else (0,):
            yield ((0 if fn>=56 else 4)<<21)|(5<<16)|((0 if 28<=fn<=31 else 6)<<11)|(sa<<6)|fn

def build(short=False,wrong_noop=False):
    p=asm.Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('80000400');p.data[0x20:0x34]=b'VELOCE DW PRIVILEGE'.ljust(20,b' ');p.data[0x34:0x3c]=b'V64HB001';assert len(p.data)==4096
    p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.i(9,0,28,0)
    p.address(8,'handler');p.constant(9,0xa0000180);p.i(9,0,10,6)
    p.mark('copy');p.i(35,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    # One uncached TLB pair maps low executable VA 0x30000 to original RAM code.
    for value,reg in ((0,0),(0x30000,10),(0,5),(0xc17,2),(0xc57,3)):
        p.constant(8,value);p.word((16<<26)|(4<<21)|(8<<16)|(reg<<11))
    p.word(0x42000002)
    p.constant(8,128);p.word(0x40886000)
    p.address(3,'operands');p.i(55,3,4,0);p.i(55,3,5,8);p.i(55,3,8,16);p.r(17,8);p.i(55,3,8,24);p.r(19,8)
    p.address(12,'specials');p.i(9,0,14,len(tuple(specials())))
    p.mark('special');p.i(35,12,17,0);p.call('profiles');p.i(9,12,12,4);p.i(9,14,14,-1);p.branch(5,14,0,'special')
    for op in MAJORS:
        p.i(9,0,16,0);label=f'op{op}';p.mark(label);p.constant(17,(op<<26)|(4<<21)|(5<<16));p.r(37,17,16,17);p.call('profiles')
        p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,label)
    p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
    p.mark('profiles');p.r(37,31,0,29);p.address(13,'statuses');p.i(9,0,15,4)
    p.mark('profile');p.i(35,13,18,0);p.i(9,0,20,0)
    p.mark('placement');p.call('execute');p.i(9,20,20,1);p.i(9,0,8,2);p.branch(5,20,8,'placement')
    p.i(9,13,13,4);p.i(9,15,15,-1);p.branch(5,15,0,'profile');p.r(8,29);p.word(0)
    p.mark('execute');p.constant(10,0xa0030000);p.address(26,'resume');p.constant(25,0x30008)
    for reg in (21,22,23,24):p.r(37,0,0,reg)
    p.r(37,17,0,8)
    if wrong_noop:p.r(37,0,0,8)
    p.branch(4,20,0,'direct');p.constant(9,0x10000001);p.i(43,10,9,0);p.i(43,10,8,4);p.branch(4,0,0,'common')
    p.mark('direct');p.i(43,10,8,0);p.i(43,10,0,4)
    p.mark('common');p.constant(9,0x40896000);p.i(43,10,9,8);p.constant(9,(26<<21)|8);p.i(43,10,9,12);p.i(43,10,0,16)
    p.constant(9,128);p.constant(10,0x30000);p.word(15);p.r(8,10);p.word(0x40926000) # Status transition in the entry jump's delay slot.
    p.mark('resume');p.word(0x401b6000);p.r(16,rd=11);p.r(18,rd=3);p.i(9,28,28,1)
    for offset,reg in enumerate((17,18,20,24,22,21,23,27,5,11,3,28)):p.i(63,2,reg,offset*8)
    p.i(43,30,0,12);p.r(8,31);p.word(0)
    p.mark('handler');p.word(0x40157000);p.word(0x40166800);p.word(0x40176000);p.i(9,24,24,1);p.word(0x40997000);p.word(0x42000018)
    p.mark('specials')
    for instruction in specials():p.word(instruction)
    p.mark('statuses')
    for status in STATUSES:p.word(status)
    while len(p.data)%8:p.data.append(0)
    p.mark('operands')
    for value in (0xfedcba9876543210,0x1122334455667788,0x5555555555555555,0xaaaaaaaaaaaaaaaa):p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=HERE/'doubleword_privilege.z64');a=p.parse_args();a.output.write_bytes(build())
