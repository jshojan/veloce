#!/usr/bin/env python3
"""Original MIT wide integer Kernel32 address cartridge; no SDK/assets."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('assembler',HERE.parent/'integer_traps/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
OPS=(26,27,39,44,45,52,55,60,63)
PROFILES=((0x10000000,0xfedcba9800000000),(0x10000080,0xfedcba9800000000),(0x10000000,0),(0x10000080,0))
def build(short=False,wrong_noop=False):
 p=asm.Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('80000400');p.data[0x20:0x34]=b'VELOCE KERNEL32 VA'.ljust(20,b' ');p.data[0x34:0x3c]=b'V64HB001'
 p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.i(9,0,28,0)
 p.address(8,'handler');p.constant(9,0xa0000180);p.i(9,0,10,6)
 p.mark('copy');p.i(35,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
 # Original RAM instruction pair is uncached; data pair is cached for LLD/SCD.
 for index,hi,lo in ((0,0x30000,0xc17),(1,0x60000,0x181f)):
  for value,reg in ((index,0),(hi,10),(0,5),(lo,2),(lo+64,3)):
   p.constant(8,value);p.word((16<<26)|(4<<21)|(8<<16)|(reg<<11))
  p.word(0x42000002)
 p.constant(9,128);p.word(0x40896000)
 for op in OPS:
  p.i(9,0,16,0);label=f'op{op}';p.mark(label);p.constant(17,(op<<26)|(4<<21)|(5<<16));p.r(37,17,16,17)
  p.address(13,'profiles');p.i(9,0,15,4)
  p.mark(label+'profile');p.i(55,13,18,0);p.i(55,13,19,8);p.i(9,0,20,0)
  p.mark(label+'placement');p.call('execute');p.i(9,20,20,1);p.i(9,0,8,2);p.branch(5,20,8,label+'placement')
  p.i(9,13,13,16);p.i(9,15,15,-1);p.branch(5,15,0,label+'profile')
  p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,label)
 p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
 p.mark('execute');p.address(8,'values');p.i(55,8,3,0);p.i(55,8,5,8)
 p.constant(10,0xa0060000);p.i(63,10,3,0);p.constant(10,0x60000);p.i(52,10,11,0) # defined cached LL reservation for SCD
 # Reconstruct base from a fixed low address/lane and each signed offset.
 p.i(12,16,8,7);p.constant(10,0x60000);p.r(45,10,8,4);p.r(0,rt=16,rd=8,sa=16);p.r(3,rt=8,rd=8,sa=16);p.r(47,4,8,4);p.r(37,4,19,4)
 p.constant(10,0xa0030000);p.address(26,'resume');p.constant(25,0x30008)
 for reg in (21,22,23,24):p.r(37,0,0,reg)
 p.r(37,17,0,8)
 if wrong_noop:p.r(37,0,0,8)
 p.branch(4,20,0,'direct');p.constant(9,0x10000001);p.i(43,10,9,0);p.i(43,10,8,4);p.branch(4,0,0,'common')
 p.mark('direct');p.i(43,10,8,0);p.i(43,10,0,4)
 p.mark('common');p.constant(9,0x40896000);p.i(43,10,9,8);p.constant(9,(26<<21)|8);p.i(43,10,9,12);p.i(43,10,0,16)
 p.constant(9,128);p.constant(10,0x30000);p.word(15);p.r(8,10);p.word(0x40926000)
 p.mark('resume');p.word(0x401b6000);p.constant(10,0x60000);p.i(47,10,21,0);p.constant(10,0xa0060000);p.i(55,10,3,0);p.i(9,28,28,1)
 for offset,reg in enumerate((17,18,20,4,24,22,21,23,5,3,27,28)):p.i(63,2,reg,offset*8)
 p.i(43,30,0,12);p.r(8,31);p.word(0)
 p.mark('handler');p.word(0x40357000);p.word(0x40166800);p.word(0x40374000);p.i(9,24,24,1);p.word(0x40997000);p.word(0x42000018)
 while len(p.data)%8:p.data.append(0)
 p.mark('profiles')
 for status,high in PROFILES:
  for value in (status,high):p.data.extend(value.to_bytes(8,'big'))
 p.mark('values')
 for value in (0x89abcdef01234567,0x1122334455667788):p.data.extend(value.to_bytes(8,'big'))
 p.resolve();return bytes(p.data)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=HERE/'kernel32_addresses.z64');a=p.parse_args();a.output.write_bytes(build())
