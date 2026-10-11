#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original ARM11 exclusive transactions with alternating native peer writers."""
import argparse,importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('owned_arm_assembler',Path(__file__).parents[1]/'reversal_registers/build.py');a=importlib.util.module_from_spec(s);s.loader.exec_module(a)
def program(core,short=False,wrong_status=False,missing_clear=False):
 p=a.Program(0x20000000 if core==0 else 0x20010000);p.constant(12,0x1fe00000);p.constant(13,0x20009000)
 def worker():
  p.constant(6,0x20008000);p.mark('worker');p.memory(7,6);p.emit(0xe3570001);p.branch('peer_store',cond=0);p.emit(0xe3570000|(2 if core==1 else 3));p.branch('worker_exit',cond=0);p.branch('worker')
  p.mark('peer_store');p.constant(7,0xdeadbeef);p.memory(7,13,load=False);p.constant(7,0);p.memory(7,6,load=False);p.branch('worker');p.mark('worker_exit')
 if core==1:worker()
 else:p.puts('header')
 for kind in range(4):
  for scenario in range(5):
   name=f'k{kind}s{scenario}';p.puts(name+'_header');p.constant(8,0);p.mark(name+'_raw');p.constant(11,0);p.mark(name+'_flags')
   p.emit(0xe1a02008);p.emit(0xe1822802);p.emit(0xe1e03002);p.constant(0,0xaaaaffff);p.constant(1,0x5555ffff);p.constant(4,0x89abcdef);p.constant(5,0x76543210);p.memory(4,13,load=False);p.memory(5,13,4,load=False);p.emit(0xf57ff01f)
   def flags():
    p.constant(6,0xf8000000);p.emit(0xe35b0000);p.emit(0x03a06000);p.emit(0xe128f006)
   flags()
   if scenario!=4:p.emit(0xe1900f9f|(kind<<21)|(13<<16))
   if scenario==1:
    if missing_clear:p.emit(0xe1a07007)
    else:p.emit(0xf57ff01f)
   if scenario==2:
    p.constant(6,0x20008000);p.constant(7,1);p.memory(7,6,load=False);p.mark(name+'_wait');p.memory(7,6);p.emit(0xe3570000);p.branch(name+'_wait',cond=1)
   flags();p.emit(0xe1804f92|(kind<<21)|(13<<16))
   if scenario==3:p.emit(0xe1804f92|(kind<<21)|(13<<16))
   p.emit(0xe10f5000)
   if wrong_status:p.emit(0xe2244001)
   p.memory(9,13);p.memory(10,13,4);p.puts('row')
   for reg in (0,1,2,3,4,5,9,10,8,11):p.memory(reg,12,8,load=False)
   p.constant(6,10);p.memory(6,12,load=False,byte=True);p.emit(0xe28bb001);p.emit(0xe35b0002);p.branch(name+'_flags',cond=1)
   p.emit(0xe2888001);p.constant(6,16 if short else 65536);p.emit(0xe1580006);p.branch(name+'_raw',cond=1)
 p.puts('block_end');p.constant(6,0x20008000);p.constant(7,2 if core==0 else 3);p.memory(7,6,load=False)
 if core==0:worker()
 else:p.puts('complete')
 p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle')
 p.mark('puts');p.memory(7,6,byte=True);p.emit(0xe3570000);p.emit(0x012fff1e);p.memory(7,12,load=False,byte=True);p.emit(0xe2866001);p.branch('puts')
 p.string('header',f'VELOCE 1 3ds exclusive_memory\nINFO short={int(short)}\n')
 for kind in range(4):
  for scenario in range(5):p.string(f'k{kind}s{scenario}_header',f'BEGIN arm11_{core}_k{kind}s{scenario} cases={(16 if short else 65536)*2} words=10\n')
 p.string('row','REG r=');p.string('block_end',f'BLOCK complete arm11_{core}\n');p.string('complete',f'LOG complete cases={(16 if short else 65536)*80}\nEND observations_complete\n')
 while len(p.data)%4:p.data.append(0)
 p.resolve();return bytes(p.data)
def build(short=False,wrong_status=False,missing_clear=False):
 programs=[program(i,short,wrong_status,missing_clear) for i in (0,1)]
 p=a.Program(0x08000000);p.constant(12,0x1fe00000);p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle');p.resolve();programs.append(bytes(p.data))
 rom=bytearray(64);rom[:8]=b'V3DSP001'
 for core,(code,address) in enumerate(zip(programs,(0x20000000,0x20010000,0x08000000))):
  if len(code)>=0x8000:raise ValueError('program overlaps shared controls')
  for n,v in enumerate((len(rom),address,address,len(code))):rom[16+core*16+n*4:20+core*16+n*4]=v.to_bytes(4,'little')
  rom.extend(code)
 return bytes(rom)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('exclusive_memory.v3ds'));args=p.parse_args();args.output.write_bytes(build())
