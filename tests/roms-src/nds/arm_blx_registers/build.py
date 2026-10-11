#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original ARM BLX immediate cartridge with transient Thumb targets."""
import argparse,hashlib
from pathlib import Path
CALL=0x02200000;SYNC=0x02001000;LINK=0x89abcdef;BASE=0x023e0000
PREFIXES=(0,0x010000,0xfe0000,0xff0000)
def groups():return tuple((prefix,h) for prefix in PREFIXES for h in (0,1))
def fields(g,full=True):
    values=range(65536) if full else (0,1,2,32767,32768,65533,65534,65535)
    return tuple(n for n in values if not (g[0]==0xff0000 and n in (65533,65534)))
def name(g):return f'prefix_{g[0]:06x}_h{g[1]}'
def cases(g,full=True):return len(fields(g,full))*32
def immediate(value):
    for rotation in range(16):
        count=rotation*2; candidate=((value<<count)|(value>>(32-count if count else 32)))&0xffffffff
        if candidate<256: return (rotation<<8)|candidate
    raise ValueError('unencodable immediate')
class Program:
    def __init__(self,base): self.base=base; self.data=bytearray(); self.labels={}; self.fixups=[]
    @property
    def pc(self): return self.base+len(self.data)
    def emit(self,word): self.data.extend(word.to_bytes(4,'little'))
    def mark(self,name):
        if name in self.labels: raise ValueError('duplicate label')
        self.labels[name]=self.pc
    def branch(self,label,cond=14,link=False):
        self.fixups.append((len(self.data),label,'branch',cond,link)); self.emit(0)
    def constant(self,reg,value):
        self.emit(0xe3a00000|(reg<<12)|immediate(value&255))
        for shift in (8,16,24):
            chunk=value&(255<<shift)
            if chunk: self.emit(0xe3800000|(reg<<16)|(reg<<12)|immediate(chunk))
    def address(self,reg,label):
        self.fixups.append((len(self.data),label,'address',reg,False))
        for _ in range(4): self.emit(0)
    def puts(self,label): self.address(6,label); self.branch('puts',link=True)
    def memory(self,reg,base,offset=0,load=True,byte=False):
        self.emit(0xe5800000|(int(load)<<20)|(int(byte)<<22)|(base<<16)|(reg<<12)|offset)
    def string(self,label,value): self.mark(label); self.data.extend(value.encode('ascii')+b'\0')
    def resolve(self):
        for offset,label,kind,reg,link in self.fixups:
            target=self.labels[label]
            if kind=='branch':
                delta=target-(self.base+offset+8)
                if delta%4 or not -(1<<25)<=delta<(1<<25): raise ValueError('branch range')
                words=[(reg<<28)|0x0a000000|(int(link)<<24)|((delta//4)&0xffffff)]
            else:
                words=[0xe3a00000|(reg<<12)|immediate(target&255)]
                for count in (8,16,24): words.append(0xe3800000|(reg<<16)|(reg<<12)|immediate(target&(255<<count)))
            for n,word in enumerate(words): self.data[offset+n*4:offset+n*4+4]=word.to_bytes(4,'little')

def program(full=True,mutant=False):
    p=Program(BASE);p.constant(12,0x04ff0000);p.constant(13,SYNC);p.puts('header')
    for g in groups():
        prefix,h=g;tag=name(g);p.puts(tag+'_header');p.constant(9,0)
        p.mark(tag+'_field')
        if not full:p.address(6,tag+'_fields');p.emit(0xe796a109) # LDR r10,[r6,r9,LSL #2]
        else:p.emit(0xe1a0a009)
        if full and prefix==0xff0000:
            for invalid in (65533,65534):
                p.constant(6,invalid);p.emit(0xe15a0006);p.branch(tag+'_next',cond=0)
        p.constant(8,0);p.mark(tag+'_flags')
        p.constant(1,prefix);p.emit(0xe181100a);p.constant(2,h);p.constant(3,CALL)
        p.emit(0xe1a05401);p.emit(0xe1a05445) # sign extend imm24 via LSL/ASR #8
        p.emit(0xe0835105);p.emit(0xe2855008|(h*2)) # call + imm*4 + 8 + H*2
        p.emit(0xe3c56003);p.memory(6,13,0x18,load=False)
        p.memory(4,6);p.memory(4,13,0x20,load=False);p.memory(4,6,4);p.memory(4,13,0x24,load=False)
        p.memory(4,3);p.memory(4,13,0x10,load=False)
        for off,value in ((0,0x4674),(2,0x467e),(4,0x4738)):
            p.constant(4,value);p.emit(0xe1c540b0|off)
        p.constant(6,0xfa000000|((h^int(mutant))<<24));p.emit(0xe1866001);p.memory(6,3,load=False)
        p.constant(0,LINK);p.emit(0xe1a0e000);p.emit(0xe1a06008|(27<<7));p.emit(0xe128f006)
        p.address(7,tag+'_returned');p.emit(0xe12fff13)
        p.mark(tag+'_returned');p.emit(0xe10f5000);p.memory(4,13,8,load=False);p.memory(6,13,0x80,load=False)
        p.memory(6,13,0x18);p.memory(4,13,0x20);p.memory(4,6,load=False);p.memory(4,13,0x24);p.memory(4,6,4,load=False)
        p.memory(4,13,0x10);p.memory(4,3,load=False)
        p.puts('row');p.memory(6,13,8);p.branch('word',link=True);p.emit(0xe1a06005);p.branch('word',link=True)
        p.memory(6,13,0x80);p.branch('word',link=True)
        for reg in (0,1,2,3):p.emit(0xe1a06000|reg);p.branch('word',link=True)
        p.memory(6,3);p.branch('word',link=True);p.constant(6,10);p.memory(6,12,load=False,byte=True)
        p.emit(0xe2888001);p.emit(0xe3580020);p.branch(tag+'_flags',cond=1)
        p.mark(tag+'_next');p.emit(0xe2899001);p.constant(6,65536 if full else len(fields(g,False)));p.emit(0xe1590006);p.branch(tag+'_field',cond=1)
    p.puts('complete');p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle')
    p.mark('puts');p.memory(7,6,load=True,byte=True);p.emit(0xe3570000);p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True);p.emit(0xe2866001);p.branch('puts')
    p.mark('word');p.memory(6,12,8,load=False);p.emit(0xe12fff1e)
    if not full:
        for g in groups():
            p.mark(name(g)+'_fields')
            for value in fields(g,False):p.emit(value)
    p.string('header',f'VELOCE 1 ds arm_blx_registers cpu=9\nINFO full={int(full)} groups=8 flags=32\n')
    for g in groups():p.string(name(g)+'_header',f'BEGIN {name(g)} cases={cases(g,full)} words=8\n')
    p.string('row','REG r=');p.string('complete',f'LOG complete cases={sum(cases(g,full) for g in groups())}\nEND observations_complete\n')
    while len(p.data)%4:p.data.append(0)
    p.resolve();return bytes(p.data)
def build(full=True,mutant=False):
    driver=program(full,mutant);main=bytearray(0x400000);main[BASE-0x02000000:BASE-0x02000000+len(driver)]=driver
    seven=bytes.fromhex('feffffea');offset7=512+len(main);rom=bytearray(offset7+4)
    rom[:12]=b'VELOCE ABLX ';rom[12:16]=b'VABX';rom[0x180:0x188]=b'VDSHB001'
    for offset,items in ((0x20,(512,BASE,0x02000000,len(main))),(0x30,(offset7,0x03800000,0x03800000,4))):
        for n,value in enumerate(items):rom[offset+n*4:offset+n*4+4]=value.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little');rom[0x84:0x88]=(512).to_bytes(4,'little')
    rom[512:offset7]=main;rom[offset7:]=seven;return bytes(rom)
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('arm_blx_registers.nds'))
    a=ap.parse_args();rom=build();a.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),a.output)
