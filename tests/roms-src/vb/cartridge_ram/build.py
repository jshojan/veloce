#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original byte SRAM mirror/guard register cartridge; no oracle in the guest."""
import argparse,hashlib,importlib.util
from pathlib import Path
s=importlib.util.spec_from_file_location('owned_assembler',Path(__file__).parents[1]/'immediates/build.py');a=importlib.util.module_from_spec(s);s.loader.exec_module(a)
def build(full=True,wrong_read=False,wrong_store=False):
 p=a.Program();offsets=8192 if full else 16;values=256 if full else 16
 p.constant(28,0x04000000);p.constant(12,values);p.constant(15,offsets);p.constant(18,0x06000000);p.constant(13,8191);p.puts('header')
 for group,base in enumerate((0x06000000,0x06002000,0x06ffc000,0x86000000)):
  name='a'+str(group);p.puts(name+'_header');p.constant(16,base);p.reg(0x10,0,24);p.mark(name+'_offset');p.reg(0x10,0,23);p.mark(name+'_value')
  # Guard addresses wrap within the declared SRAM rather than crossing board regions.
  p.wide(0x29,24,14,-1);p.reg(0x0d,13,14);p.reg(1,18,14)
  p.wide(0x29,24,19,1);p.reg(0x0d,13,19);p.reg(1,18,19)
  p.constant(6,0x5a);p.wide(0x34,14,6,0);p.constant(6,0xa5);p.wide(0x34,19,6,0)
  p.reg(0,18,17);p.reg(1,24,17);p.constant(6,0x800f);p.reg(0x1c,5,6)
  if not wrong_store:p.wide(0x34,16,23,0)
  else:p.reg(0);p.reg(0)
  if wrong_read:p.reg(0x10,0,20);p.reg(0)
  else:p.wide(0x30,16,20,0)
  p.wide(0x30,17,21,0);p.reg(0x1d,5,22);p.wide(0x30,14,25,0);p.wide(0x30,19,27,0);p.constant(30,0xdeadbeef)
  p.puts('row')
  for r in (20,21,22,23,24,25,27,30):p.wide(0x37,28,r,8)
  p.reg(0x10,10,6);p.wide(0x34,28,6,0);p.reg(0x11,1,23);p.reg(3,12,23);p.branch(2,name+'_value_done');p.jump(0x2a,name+'_value');p.mark(name+'_value_done')
  p.reg(0x11,1,24);p.reg(0x11,1,16);p.reg(3,15,24);p.branch(2,name+'_offset_done');p.jump(0x2a,name+'_offset');p.mark(name+'_offset_done')
 p.puts('complete');p.wide(0x37,28,0,4);p.reg(0x1a)
 p.mark('puts');p.wide(0x30,10,6,0);p.reg(0x13,0,6);p.branch(2,'puts_return');p.wide(0x34,28,6,0);p.reg(0x11,1,10);p.jump(0x2a,'puts');p.mark('puts_return');p.reg(6,31)
 p.string('header',f'VELOCE 1 vb cartridge ram\nINFO full={int(full)} capacity=8192 groups=4\n')
 for group in range(4):p.string('a'+str(group)+'_header',f'BEGIN a{group} cases={offsets*values} words=8\n')
 p.string('row','REG r=');p.string('complete',f'LOG complete cases={offsets*values*4}\nEND observations_complete\n');p.resolve();size=1024
 while size<len(p.data)+512:size*=2
 p.data.extend(bytes(size-len(p.data)));p.data[-512:-504]=b'VVBHB001';p.data[-504:-496]=b'VVBCRAM1';p.data[-496:-492]=(8192).to_bytes(4,'big')
 p.data[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little');return bytes(p.data)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('cartridge_ram.vb'));args=p.parse_args();rom=build();args.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),args.output)
