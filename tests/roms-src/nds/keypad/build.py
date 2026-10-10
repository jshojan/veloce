#!/usr/bin/env python3
"""Original dual-CPU DS keypad observations, no SDK or expected answers."""
import argparse
import hashlib
from pathlib import Path
SYNC=0x02001000
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

def program(cpu,cases=4096,wrong_polarity=False):
    p=Program(0x02000000 if cpu==9 else 0x03800000)
    p.constant(13,SYNC); p.constant(11,0x04000130); p.constant(12,0x04ff0000)
    if cpu==9:
        p.puts('header'); p.constant(9,1); p.mark('wait')
        p.memory(6,13); p.emit(0xe1560009); p.branch('wait',cond=1)
        p.emit(0xe1db00b0); p.memory(1,11,load=True,byte=True); p.memory(2,11,1,load=True,byte=True)
        if wrong_polarity: p.emit(0xe2200001)
        p.puts('row')
        for reg in (0,1,2): p.emit(0xe1a06000|reg); p.branch('word',link=True)
        for n in range(6): p.memory(6,13,8+n*4); p.branch('word',link=True)
        p.emit(0xe1a06009); p.branch('word',link=True)
        p.constant(6,10); p.memory(6,12,load=False,byte=True)
        p.memory(9,13,4,load=False); p.emit(0xe2899001); p.constant(6,cases+1); p.emit(0xe1590006); p.branch('wait',cond=1)
        p.puts('complete')
    else:
        p.constant(8,0xffffffff); p.constant(9,1); p.mark('poll')
        p.emit(0xe1db00b0); p.emit(0xe1db30b6)
        p.emit(0xe180a803); p.emit(0xe15a0008); p.branch('poll',cond=0)
        p.emit(0xe1a0800a)
        p.memory(1,11,load=True,byte=True); p.memory(2,11,1,load=True,byte=True)
        p.memory(4,11,6,load=True,byte=True); p.memory(5,11,7,load=True,byte=True)
        for reg in range(6): p.memory(reg,13,8+reg*4,load=False)
        p.memory(9,13,load=False); p.mark('ack'); p.memory(6,13,4); p.emit(0xe1560009); p.branch('ack',cond=1)
        p.emit(0xe2899001); p.constant(6,cases+1); p.emit(0xe1590006); p.branch('poll',cond=1)
    p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
    if cpu==9:
        p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
        p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
        p.mark('word'); p.memory(6,12,8,load=False); p.emit(0xe12fff1e)
        p.string('header',f'VELOCE 1 ds keypad\nBEGIN digital cases={cases} words=10\n')
        p.string('row','REG r='); p.string('complete',f'LOG complete cases={cases}\nEND observations_complete\n')
    while len(p.data)%4: p.data.append(0)
    p.resolve(); return bytes(p.data)
def build(cases=4096,wrong_polarity=False):
    if not 1<=cases<=4096: raise ValueError('case count outside domain')
    nine=program(9,cases,wrong_polarity); seven=program(7,cases)
    if len(nine)>0x1000 or len(seven)>0x10000: raise ValueError('driver exceeds reserved code')
    offset7=512+len(nine); rom=bytearray(offset7+len(seven)); rom[:12]=b'VELOCE KEYS '; rom[12:16]=b'VKEY'; rom[0x180:0x188]=b'VDSHB001'
    for offset,items in ((0x20,(512,0x02000000,0x02000000,len(nine))),(0x30,(offset7,0x03800000,0x03800000,len(seven)))):
        for n,value in enumerate(items): rom[offset+n*4:offset+n*4+4]=value.to_bytes(4,'little')
    rom[512:offset7]=nine; rom[offset7:]=seven; return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('keypad.nds'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom); print(hashlib.sha256(rom).hexdigest(),args.output)
