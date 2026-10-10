#!/usr/bin/env python3
"""Original DSi dual-CPU RAM mode/alias stimuli and register capture."""
import argparse
from pathlib import Path
SYNC=0x02001800
SEEDS=(0x13579bdf,0x2468ace0,0x55aa00ff,0xa55aff00)
def groups(short=False):
    for mode in (0,1,2):
        for bank in range(4):
            for store_alias in (0x02000000,0x0c000000,0x0d000000):
                for read_alias in (0x02000000,0x0c000000,0x0d000000):
                    for writer in (0,1):
                        for width in (1,2,4):
                            for lane in range(0,4,width):
                                yield (store_alias+bank*0x400000+0x10000+lane,read_alias+0x10000,width,writer,0x80000000|mode<<14)
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

def store(p,base=0,wrong_width=False):
    if wrong_width: p.memory(5,base,load=False,byte=True); return
    for width,word in ((1,0x05c05000),(2,0x01c050b0),(4,0x05805000)):
        p.emit(0xe3520000|width); p.emit(word|base<<16)
def program(cpu,short=False,wrong_bank=False,wrong_width=False,wrong_mode=False):
    p=Program(0x02000000 if cpu==9 else 0x03800000)
    p.constant(13,SYNC); p.constant(12,0x04ff0000); p.constant(9,1 if cpu==9 else 0)
    if cpu==9:
        records=list(groups(short)); cases=len(records)*(4 if short else 256)
        p.puts('header'); p.address(10,'records'); p.constant(11,len(records)); p.mark('group')
        for n,r in enumerate((0,1,2,3,4)): p.memory(r,10,n*4)
        p.constant(8,0); p.mark('case')
        p.constant(6,0x04004008); p.constant(7,0x80008000); p.memory(7,6,load=False)
        for bank,seed in enumerate(SEEDS):
            p.constant(6,0x02010000+bank*0x400000); p.constant(7,seed); p.memory(7,6,load=False)
        p.constant(6,0x04004008)
        if wrong_mode: p.constant(7,0x80008000); p.memory(7,6,load=False)
        else: p.memory(4,6,load=False)
        p.emit(0xe22870ff); p.emit(0xe1885407); p.emit(0xe1855808); p.emit(0xe1855c07)
        p.emit(0xe3530000); p.branch('request',cond=1)
        if wrong_bank: p.emit(0xe3c07503); store(p,7,wrong_width) # Clear bank-select bits 22-23.
        else: store(p,0,wrong_width)
        p.mark('request')
        for offset,r in ((8,0),(12,5),(16,2),(20,3),(24,1),(28,4)): p.memory(r,13,offset,load=False)
        p.memory(9,13,load=False); p.mark('ack'); p.memory(6,13,4); p.emit(0xe1560009); p.branch('ack',cond=1)
        p.puts('row'); p.emit(0xe1a07001)
        for bank in range(4):
            p.memory(6,7); p.branch('word',link=True)
            if bank<3: p.emit(0xe2877000|immediate(0x400000))
        for bank in range(4): p.memory(6,13,32+bank*4); p.branch('word',link=True)
        for r in (0,5,2,3,4,9): p.emit(0xe1a06000|r); p.branch('word',link=True)
        p.constant(6,10); p.memory(6,12,load=False,byte=True)
        p.emit(0xe2899001); p.emit(0xe2888001); p.constant(6,4 if short else 256); p.emit(0xe1580006); p.branch('case',cond=1)
        p.emit(0xe28aa014); p.emit(0xe25bb001); p.branch('group',cond=1)
        p.puts('complete'); p.constant(6,0xffffffff); p.memory(6,13,load=False)
        p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
        p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
        p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
        p.mark('word'); p.memory(6,12,8,load=False); p.emit(0xe12fff1e)
        p.string('header',f'VELOCE 1 dsi ram_registers\nBEGIN mappings cases={cases} words=14\n')
        p.string('row','REG r='); p.string('complete',f'LOG complete cases={cases}\nEND observations_complete\n')
        if len(p.data)>0x1800: raise ValueError('code overlaps sync')
        p.data.extend(bytes(0x2000-len(p.data))); p.mark('records')
        for record in records:
            for word in record: p.emit(word)
    else:
        p.mark('poll'); p.memory(6,13); p.constant(7,0xffffffff); p.emit(0xe1560007); p.branch('finish',cond=0)
        p.emit(0xe1560009); p.branch('poll',cond=0); p.emit(0xe1a09006)
        for offset,r in ((8,0),(12,5),(16,2),(20,3),(24,1)): p.memory(r,13,offset)
        p.emit(0xe3530001); p.branch('read',cond=1)
        if wrong_bank: p.emit(0xe3c07503); store(p,7,wrong_width)
        else: store(p,0,wrong_width)
        p.mark('read'); p.emit(0xe1a07001)
        for bank in range(4):
            p.memory(6,7); p.memory(6,13,32+bank*4,load=False)
            if bank<3: p.emit(0xe2877000|immediate(0x400000))
        p.memory(9,13,4,load=False); p.branch('poll')
        p.mark('finish'); p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
    while len(p.data)%4: p.data.append(0)
    p.resolve(); return bytes(p.data)
def build(short=False,wrong_bank=False,wrong_width=False,wrong_mode=False):
    nine=program(9,short,wrong_bank,wrong_width,wrong_mode); seven=program(7,short,wrong_bank,wrong_width,wrong_mode)
    offset7=512+len(nine); rom=bytearray(offset7+len(seven)); rom[:12]=b'VELOCE DSi  '; rom[12:16]=b'VRAM'; rom[0x12]=2; rom[0x180:0x188]=b'VDSI0001'
    for offset,items in ((0x20,(512,0x02000000,0x02000000,len(nine))),(0x30,(offset7,0x03800000,0x03800000,len(seven)))):
        for n,value in enumerate(items): rom[offset+n*4:offset+n*4+4]=value.to_bytes(4,'little')
    rom[512:offset7]=nine; rom[offset7:]=seven; return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('ram_registers.vdsi'))
    args=parser.parse_args();args.output.write_bytes(build())
