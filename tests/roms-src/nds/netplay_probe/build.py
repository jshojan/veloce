#!/usr/bin/env python3
"""Original dual-CPU DS polling probe for deterministic network replay."""
import hashlib
from pathlib import Path
import sys
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

def program(cpu):
    p=Program(0x02000000 if cpu==9 else 0x03800000)
    p.constant(0,0x04000130); p.constant(1,0x02001000 if cpu==9 else 0x02001010)
    p.constant(2,0); p.constant(5,0)
    p.mark('poll'); p.emit(0xe1d030b0); p.emit(0xe0822003); p.memory(2,1,load=False); p.memory(3,1,4,load=False)
    if cpu==7:
        p.emit(0xe1d040b6); p.emit(0xe0855004); p.memory(5,1,8,load=False); p.memory(4,1,12,load=False)
    else:
        p.emit(0xe2855001); p.memory(5,1,8,load=False)
    p.branch('poll'); p.resolve(); return bytes(p.data)
def build():
    nine=program(9); seven=program(7); offset7=512+len(nine)
    rom=bytearray(offset7+len(seven)); rom[:12]=b'VELOCE NET  '; rom[12:16]=b'VNET'; rom[0x180:0x188]=b'VDSHB001'
    for offset,items in ((0x20,(512,0x02000000,0x02000000,len(nine))),(0x30,(offset7,0x03800000,0x03800000,len(seven)))):
        for n,value in enumerate(items): rom[offset+n*4:offset+n*4+4]=value.to_bytes(4,'little')
    rom[512:offset7]=nine; rom[offset7:]=seven; return bytes(rom)
if __name__=='__main__':
    output=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('netplay_probe.nds')
    rom=build(); output.write_bytes(rom); print(hashlib.sha256(rom).hexdigest(),output)
