#!/usr/bin/env python3
"""Original MIT LoROM emitting actual decimal SBC accumulator/status pairs."""
from pathlib import Path
import hashlib,sys
class Program:
    def __init__(self): self.data=bytearray([0xea]*(4*1024*1024));self.at=0;self.calls=[]
    def address(self):return (0x80|self.at//32768)<<16 | 0x8000+(self.at%32768)
    def emit(self,*values):
        if self.at+len(values)>len(self.data): raise ValueError("LoROM capacity exceeded")
        self.data[self.at:self.at+len(values)]=bytes(values);self.at+=len(values)
    def reserve(self):
        end=32704 if self.at//32768==0 else 32768
        if end-self.at%32768<2048:
            bank=self.at//32768+1;self.emit(0x5c,0,0x80,bank|0x80);self.at=bank*32768
    def word(self,v):self.emit(v&255,(v>>8)&255)
    def long(self,opcode,address):self.emit(opcode,address&255,(address>>8)&255,address>>16)
    def store(self,address,value):self.emit(0xa9,value&255);self.long(0x8f,address)
    def text(self,text):
        for c in text.encode():self.store(0x0021fe,c)
    def digit(self):
        self.emit(0x09,0x30,0xc9,0x3a,0x90,2,0x69,0x26);self.long(0x8f,0x0021fe)
    def hex(self,address):
        target=self.address()+16+4
        self.store(0x70,target);self.store(0x71,target>>8)
        self.long(0xaf,address);self.calls.append(self.at);self.long(0x5c,0)
    def hex_routine(self):
        target=self.address()
        for at in self.calls:self.data[at+1:at+4]=bytes((target&255,(target>>8)&255,target>>16))
        self.long(0x8f,0x75);self.emit(0x4a,0x4a,0x4a,0x4a);self.digit()
        self.long(0xaf,0x75);self.emit(0x29,15);self.digit();self.emit(0xdc,0x70,0)


def stimuli():
    cases=[]
    for base in (0x0000,0x9999,0xffff,0x5555):
        for shift in (0,4,8,12):
            for left in range(16):
                for right in range(16):
                    for carry in (0,1):
                        mask=0xffff^(15<<shift)
                        cases.append(((base&mask)|(left<<shift),(base&mask)|(right<<shift),carry))
    cases.append((0xab1d,0xf1e2,1))
    return cases

def build(mutation=False):
    p=Program();cases=stimuli();p.long(0x5c,0x808004);p.emit(0x78,0xd8)
    p.text(f'VELOCE 1 snes decimal_sbc\nINFO cases={len(cases)}\n')
    p.emit(0x18,0xfb,0xc2,0x20,0xa9,0xff,0x01,0x1b,0xe2,0x20)
    for n,(a,b,c) in enumerate(cases):
        p.reserve();p.store(0x72,p.address()>>16)
        p.emit(0xc2,0x20,0xa9);p.word(a);p.emit(0xd8,0xb8,0xf8,0x38 if c else 0x18,0xe9);p.word(b)
        p.emit(0x08);p.long(0x8f,0x7e3000);p.emit(0xe2,0x20,0x68);p.long(0x8f,0x7e3002);p.emit(0xd8)
        if mutation and n==len(cases)-1:
            p.long(0xaf,0x7e3000);p.emit(0x49,1);p.long(0x8f,0x7e3000)
        p.text(f'REG id={n:04d} a=');p.hex(0x7e3001);p.hex(0x7e3000)
        p.text(' p=');p.hex(0x7e3002);p.text('\n')
    p.reserve();p.text('LOG observations_complete\n');p.store(0x0021ff,1);p.emit(0x80,0xfe);p.hex_routine()
    p.data[0x7fc0:0x7fd5]=b'VELOCE DECIMAL SBC'.ljust(21,b' ')
    p.data[0x7fd5:0x7fdc]=bytes((0x20,0,12,0,1,0x33,0))
    p.data[0x7fdc:0x7fe0]=bytes((0xff,0xff,0,0))
    for vector in (0x7ffc,0x7ffe):p.data[vector:vector+2]=bytes((0,0x80))
    return bytes(p.data),cases
if __name__=='__main__':
    rom,_=build();out=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('decimal_sbc.sfc');out.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),out)
