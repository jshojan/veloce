#!/usr/bin/env python3
"""Original MIT Config fixed-field/CU/K0 cartridge, with actual cache loads."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('assembler',HERE.parent/'cache_coherence/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
class Program(asm.Program):
 def constant(self,rt,x):self.i(15,0,rt,x>>16);self.i(13,rt,rt,x&65535)
 def call(self,name):self.jump(3,name)
WRITABLE=0x0f00800f
MASK=(1<<64)-1

def build(short=False,wrong_read=False):
 p=Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('a0000400');p.data[0x20:0x34]=b'VELOCE CONFIG FIELDS'.ljust(20,b' ');p.data[0x34:0x3c]=b'V64HB001'
 p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.i(9,0,28,0);p.constant(8,128);p.word(0x40886000)
 p.address(8,'values');p.i(55,8,12,0)
 for form in (4,5):
  p.constant(17,(16<<26)|(form<<21)|(5<<16)|(16<<11))
  for shift in (0,16):
   label=f'f{form}s{shift}';p.i(9,0,19,shift);p.address(13,f'patterns{shift}');p.i(9,0,15,2)
   p.mark(label+'pattern');p.i(55,13,18,0);p.i(9,0,16,0)
   p.mark(label+'value');p.r(20,rs=19,rt=16,rd=5);p.r(36,5,12,5);p.r(37,5,18,6);p.i(9,0,20,0)
   p.mark(label+'control');p.r(37,6,20,5);p.i(9,0,24,0)
   p.mark(label+'placement');p.call('execute');p.i(9,24,24,1);p.i(9,0,8,2);p.branch(5,24,8,label+'placement')
   p.i(9,20,20,1);p.i(9,0,8,16);p.branch(5,20,8,label+'control')
   p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,label+'value')
   p.i(9,13,13,8);p.i(9,15,15,-1);p.branch(5,15,0,label+'pattern')
 p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
 p.mark('execute');p.constant(10,0x80700000);p.i(47,10,21,0);p.constant(10,0xa0700000);p.address(8,'values');p.i(55,8,9,8);p.i(63,10,9,0)
 p.constant(10,0xa0030000);p.address(26,'resume')
 p.branch(4,24,0,'direct');p.constant(8,0x10000001);p.i(43,10,8,0);p.i(43,10,17,4);p.branch(4,0,0,'common')
 p.mark('direct');p.i(43,10,17,0);p.i(43,10,0,4)
 p.mark('common');p.i(43,10,0,8);p.i(43,10,0,12);p.constant(8,0x40158000);p.i(43,10,8,16);p.i(43,10,0,20)
 p.constant(8,0x40368000 if not wrong_read else 0x40367000);p.i(43,10,8,24);p.i(43,10,0,28);p.constant(8,(26<<21)|8);p.i(43,10,8,32);p.i(43,10,0,36)
 p.word(15);p.r(8,10);p.word(0)
 p.mark('resume');p.constant(10,0x80700000);p.i(55,10,11,0);p.address(8,'values');p.i(55,8,9,16);p.constant(10,0xa0700000);p.i(63,10,9,0);p.constant(10,0x80700000);p.i(55,10,23,0);p.i(47,10,21,0);p.word(0x401b6000);p.word(0);p.word(0);p.i(9,28,28,1)
 for offset,reg in enumerate((17,5,18,19,20,24,21,22,11,23,27,28)):p.i(63,2,reg,offset*8)
 p.i(43,30,0,12);p.r(8,31);p.word(0)
 while len(p.data)%8:p.data.append(0)
 p.mark('values')
 for value in (MASK^WRITABLE,0x1122334455667788,0x8877665544332211):p.data.extend(value.to_bytes(8,'big'))
 for shift in (0,16):
  p.mark(f'patterns{shift}')
  for pattern in (0,MASK):p.data.extend(((pattern&~(65535<<shift)&~WRITABLE)|0x8000).to_bytes(8,'big'))
 p.resolve();return bytes(p.data)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=HERE/'config_fields.z64');a=p.parse_args();a.output.write_bytes(build())
