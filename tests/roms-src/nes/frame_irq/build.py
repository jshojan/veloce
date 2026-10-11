#!/usr/bin/env python3
"""Original NROM guest: raw $4015 pairs around NTSC frame IRQ boundaries."""
from pathlib import Path
import hashlib
import sys
TIMES = (6, 7, 8, 29830, 29831, 29832, 29833, 59661, 59662, 59663)
class Program:
    def __init__(self):
        self.data=bytearray([0xea]*32768); self.at=0; self.cycles=4; self.cases=[]
    def ins(self, cycles, *data):
        self.data[self.at:self.at+len(data)]=bytes(data);self.at+=len(data);self.cycles+=cycles
        if self.at>=32762: raise ValueError('program overlaps vectors')
    def text(self, s):
        for c in s.encode(): self.ins(2,0xa9,c);self.ins(4,0x8d,0x1e,0x40)
    def write(self, address, value):
        self.ins(2,0xa9,value);self.ins(4,0x8d,address&255,address>>8)
    def delay(self, count):
        if count<0 or count==1: raise ValueError("unrepresentable instruction delay")
        while count>=6:
            # Keep DEX/BNE on one page; a taken crossing adds a cycle.
            if (0x8000+self.at+2)&255>=253:
                self.ins(2,0xea);count-=2;continue
            n=min(256,(count-1)//5)
            cost=5*n+1
            if count-cost==1: n-=1;cost-=5
            if n<1: break
            self.ins(cost,0xa2,n&255,0xca,0xd0,0xfd);count-=cost
        if count%2: self.ins(3,0x24,0xfe);count-=3
        while count: self.ins(2,0xea);count-=2
    def status(self, address):
        self.ins(3,0xa5,address)
        for _ in range(4): self.ins(2,0x4a)
        self.ins(2,0x09,0x30);self.ins(4,0x8d,0x1e,0x40)
        self.ins(3,0xa5,address);self.ins(2,0x29,15);self.ins(2,0x09,0x30);self.ins(4,0x8d,0x1e,0x40)

def build(mutation=False):
    p=Program();p.ins(2,0x78);p.ins(2,0xd8)
    p.write(0x2000,0);p.write(0x4015,0)
    p.text('VELOCE 1 nes frame_irq\nINFO cases=80\n')
    for control in (0,0x40,0x80,0xc0):
        for parity in (0,1):
            for target in TIMES:
                p.write(0x4017,0x40);p.delay(8)
                if (p.cycles+6)%2!=parity: p.ins(3,0x24,0xfe)
                p.write(0x4017,control)
                p.delay(target-4);p.ins(4,0xad,0x15,0x40)
                if mutation and not p.cases: p.ins(2,0x49,0x40)
                p.ins(3,0x85,0);p.ins(4,0xad,0x15,0x40);p.ins(3,0x85,1)
                p.cases.append((control,parity,target))
                p.text(f'REG id={len(p.cases)-1:03d} status=');p.status(0);p.text(',');p.status(1);p.text('\n')
    p.text('LOG observations_complete\n');p.write(0x401f,1)
    p.ins(3,0x4c,(0x8000+p.at)&255,(0x8000+p.at)>>8)
    for offset in (32762,32764,32766):p.data[offset:offset+2]=bytes((0,0x80))
    return b'NES\x1a'+bytes((2,0))+bytes(10)+p.data,p.cases
if __name__=='__main__':
    rom,_=build();output=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('frame_irq.nes')
    output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),output)
