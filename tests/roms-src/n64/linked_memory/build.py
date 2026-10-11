#!/usr/bin/env python3
"""Original linked load/store cartridge. No guest verdict or expected answers."""
import argparse,importlib.util
from pathlib import Path
_spec=importlib.util.spec_from_file_location('original_trap_assembler',Path(__file__).resolve().parents[1]/'integer_traps/build.py')
_assembler=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_assembler)
Program=_assembler.Program
VALUES=(0,0x7fffffff80000000,0x80000000ffffffff,0xfedcba9876543210)

def build(short=False,wrong_eret=False):
    p=Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('80000400');p.data[0x20:0x34]=b'VELOCE LINKED MEMORY';p.data[0x34:0x3c]=b'V64HB001'
    # Keep instructions uncached, but use cached KSEG0 for the atomic target.
    p.address(8,'uncached');p.constant(9,0x20000000);p.r(0x25,8,9,8);p.r(8,8);p.word(0)
    p.mark('uncached');p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.constant(3,0xa0008000);p.constant(10,0xa0030000)
    p.i(9,0,8,3);p.word(0x40888000);p.i(9,0,8,128);p.word(0x40886000);p.i(9,0,28,0)
    p.i(9,0,16,0)
    p.mark('width');p.i(9,0,17,0)
    p.mark('offset');p.address(12,'values');p.i(9,0,14,4)
    p.mark('value');p.i(55,12,18,0);p.r(0x27,18,0,19);p.i(9,0,20,0)
    p.mark('failure');p.call('execute');p.i(9,20,20,1);p.i(9,0,8,2);p.branch(5,20,8,'failure')
    p.i(9,12,12,8);p.i(9,14,14,-1);p.branch(5,14,0,'value')
    p.i(9,17,17,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,17,8,'offset')
    p.i(9,16,16,1);p.i(9,0,8,2);p.branch(5,16,8,'width')
    p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
    p.mark('execute');p.i(63,3,18,0);p.constant(4,0x80008000);p.r(0,rt=17,rd=8,sa=16);p.r(3,rt=8,rd=8,sa=16);p.r(0x2f,4,8,4)
    # Invalidate the old target line before each fresh LL. ERET at entry resets LLbit.
    p.constant(8,0x80008000);p.i(47,8,17,0)
    p.address(26,'linked_entry');p.constant(9,0x20000000);p.r(0x25,26,9,26);p.word(0x409a7000);p.word(0x42000018)
    p.mark('linked_entry');p.r(0x25,19,0,6)
    p.r(0,rt=16,rd=8,sa=28);p.constant(9,(48<<26)|(4<<21)|(5<<16));p.r(0x25,9,8,9);p.r(0x25,9,17,9);p.i(43,10,9,0)
    p.constant(9,(26<<21)|8);p.i(43,10,9,4);p.i(43,10,0,8)
    p.address(26,'after_ll');p.constant(9,0x20000000);p.r(0x25,26,9,26);p.word(15);p.r(8,10);p.word(0)
    p.mark('after_ll');p.word(0x40088800) # MFC0 t0,LLAddr diagnostic.
    p.branch(5,28,0,'not_first');p.i(43,30,0,12) # Real checkpoint with live LLbit, before first SC.
    p.mark('not_first');p.branch(4,20,0,'no_eret')
    p.address(26,'no_eret');p.constant(9,0x20000000);p.r(0x25,26,9,26);p.word(0x409a7000);p.word(0 if wrong_eret else 0x42000018)
    p.mark('no_eret');p.r(0,rt=16,rd=9,sa=28);p.constant(11,(56<<26)|(4<<21)|(6<<16));p.r(0x25,11,9,11);p.r(0x25,11,17,11);p.i(43,10,11,0)
    p.address(26,'after_sc');p.constant(9,0x20000000);p.r(0x25,26,9,26);p.word(15);p.r(8,10);p.word(0)
    p.mark('after_sc');p.constant(9,0x80008000);p.branch(5,16,0,'read_double');p.i(35,9,7,0);p.branch(4,0,0,'read_done');p.mark('read_double');p.i(55,9,7,0);p.mark('read_done')
    p.i(9,28,28,1)
    for offset,reg in enumerate((28,16,17,18,19,20,5,6,7,8)):p.i(63,2,reg,offset*8)
    p.i(43,30,0,12);p.r(8,31);p.word(0)
    while len(p.data)%8:p.data.append(0)
    p.mark('values')
    for value in VALUES:p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('linked_memory.z64'));a=p.parse_args();a.output.write_bytes(build())
