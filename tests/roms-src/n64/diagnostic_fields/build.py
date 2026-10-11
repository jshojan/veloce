#!/usr/bin/env python3
"""Original MIT CP0 TagLo/TagHi/PErr writable/reserved cartridge."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('assembler',HERE.parent/'cache_coherence/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
class Program(asm.Program):
 def constant(self,rt,x):self.i(15,0,rt,x>>16);self.i(13,rt,rt,x&65535)
 def call(self,name):self.jump(3,name)

def build(short=False,wrong_read=False):
 p=Program();p.data[:4]=bytes.fromhex('80371240');p.data[8:12]=bytes.fromhex('a0000400');p.data[0x20:0x34]=b'VELOCE DIAG FIELDS'.ljust(20,b' ');p.data[0x34:0x3c]=b'V64HB001'
 p.constant(30,0xbff00000);p.constant(2,0xa0002000);p.i(9,0,28,0);p.constant(8,128);p.word(0x40886000)
 for reg in (26,28,29):
  for form in (4,5):
   p.constant(17,(16<<26)|(form<<21)|(5<<16)|(reg<<11));p.i(9,0,25,reg);p.constant(6,0x40150000|(reg<<11));p.constant(7,0x40360000|(reg<<11))
   for shift in (0,16,32,48):
    label=f'r{reg}f{form}s{shift}';p.i(9,0,19,shift);p.address(13,f'patterns{shift}');p.i(9,0,15,4)
    p.mark(label+'pattern');p.i(55,13,18,0);p.i(9,0,16,0)
    p.mark(label+'value');p.r(20,rs=19,rt=16,rd=5);p.r(37,5,18,5);p.i(9,0,24,0)
    p.mark(label+'placement');p.call('execute');p.i(9,24,24,1);p.i(9,0,8,2);p.branch(5,24,8,label+'placement')
    p.i(9,16,16,4369 if short else 1);p.constant(8,69904 if short else 65536);p.branch(5,16,8,label+'value')
    p.i(9,13,13,8);p.i(9,15,15,-1);p.branch(5,15,0,label+'pattern')
 p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt')
 p.mark('execute');p.constant(8,0x42);p.word(0x4088d000);p.constant(8,0x012345c0);p.word(0x4088e000);p.word(0x4080e800);p.constant(10,0xa0030000);p.address(26,'resume')
 p.branch(4,24,0,'direct');p.constant(8,0x10000001);p.i(43,10,8,0);p.i(43,10,17,4);p.branch(4,0,0,'common')
 p.mark('direct');p.i(43,10,17,0);p.i(43,10,0,4)
 p.mark('common');p.i(43,10,0,8);p.i(43,10,0,12);p.i(43,10,6,16);p.i(43,10,0,20)
 p.constant(8,0x40368000) if wrong_read else p.r(37,7,0,8);p.i(43,10,8,24);p.i(43,10,0,28)
 p.constant(8,0x40896000);p.i(43,10,8,32);p.i(43,10,0,36);p.i(43,10,0,40);p.constant(8,(26<<21)|8);p.i(43,10,8,44);p.i(43,10,0,48)
 p.constant(9,128);p.word(0x40176000);p.word(0);p.word(0);p.word(15);p.r(8,10);p.word(0)
 p.mark('resume');p.word(0x403be000);p.word(0x4029e800);p.word(0x402bd000);p.word(0);p.word(0);p.i(9,28,28,1)
 for offset,reg in enumerate((17,5,18,19,24,25,21,22,27,9,11,28)):p.i(63,2,reg,offset*8)
 p.i(43,30,0,12);p.r(8,31);p.word(0)
 while len(p.data)%8:p.data.append(0)
 for shift in (0,16,32,48):
  p.mark(f'patterns{shift}')
  for pattern in (0,0xffffffffffffffff,0x5555555555555555,0xaaaaaaaaaaaaaaaa):p.data.extend((pattern&~(65535<<shift)).to_bytes(8,'big'))
 p.resolve();return bytes(p.data)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=HERE/'diagnostic_fields.z64');a=p.parse_args();a.output.write_bytes(build())
