#!/usr/bin/env python3
"""Original LoROM: actual stack/register/memory observations, no verdicts."""
from pathlib import Path
import hashlib
import sys
OPS=('pea','pei','per','phd','jsl','jsrx','pld','plb','rtl','pha','pla')
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
    def capture16(self,opcode,address):
        self.emit(opcode);self.long(0x8f,address);self.emit(0xeb);self.long(0x8f,address+1)

def build(mutation=False):
    p=Program();cases=[];p.long(0x5c,0x808004);p.emit(0x78,0xd8);p.text('VELOCE 1 snes stack_boundaries\nINFO cases=5632\n')
    for mode in (0,1):
        for op in OPS:
            for low in range(256):
                p.reserve();sp=0x100+low;value=(low<<8)|(low^255)
                count=3 if op in ('jsl','rtl') else 1 if op in ('plb','pha','pla') else 2
                pushing=op in ('pea','pei','per','phd','jsl','jsrx','pha')
                wrapped=bool(mode and op in ('pha','pla'))
                addresses=set()
                for delta in range(-3,5):
                    addresses.add((sp+delta)&65535);addresses.add(0x100|((low+delta)&255))
                addresses=sorted(addresses)
                memory={a:(a*29+17+(a>>8)*73)&255 for a in addresses}
                p.emit(0x18,0xfb,0xc2,0x20,0xa9);p.word(value if op=='phd' else 0);p.emit(0x5b)
                p.emit(0xe2,0x30) # native, 8-bit A/X before optional emulation
                # Initialize DBR=0 using a safe stack before the measured operation.
                p.emit(0xa2,0xf0,0x9a,0xa9,0,0x48,0xab)
                p.emit(0xc2,0x20,0xa9);p.word(sp);p.emit(0x1b,0xe2,0x20)
                if mode:p.emit(0x38,0xfb)
                for address,v in memory.items():p.store(address,v)
                if op=='pei':p.store(0x20,value);p.store(0x21,value>>8)
                if op in ('pld','plb','pla'):
                    for n in range(count):
                        address=(sp+n+1)&65535
                        if wrapped:address=0x100|((low+n+1)&255)
                        v=(value>>(8*n))&255;memory[address]=v;p.store(address,v)
                if op=='rtl':
                    # Target the instruction immediately following RTL.
                    target=p.address()+3*6+1
                    for n,v in enumerate(((target-1)&255,((target-1)>>8)&255,target>>16)):
                        memory[sp+n+1]=v;p.store(sp+n+1,v)
                if op=='jsrx':
                    p.emit(0xa2,0) # index zero, pointer in bank's WRAM mirror
                at=p.address()
                if op=='pea':p.emit(0xf4);p.word(value)
                elif op=='pei':p.emit(0xd4,0x20)
                elif op=='per':p.emit(0x62);p.word(value-((at+3)&65535))
                elif op=='phd':p.emit(0x0b)
                elif op=='jsl':p.long(0x22,at+4)
                elif op=='jsrx':
                    p.emit(0xfc);p.word((at+3)&65535);p.word((at+5)&65535)
                elif op=='pld':p.emit(0x2b)
                elif op=='plb':p.emit(0xab)
                elif op=='rtl':p.emit(0x6b)
                elif op=='pha':p.emit(0xa9,value&255,0x48)
                elif op=='pla':p.emit(0x68)
                cases.append({'mode':mode,'op':op,'sp':sp,'value':value,'at':at,'addresses':addresses,'initial':memory.copy()})
                p.store(0x72,p.address()>>16)
                # Capture actual S/D before any probe uses stack memory.
                p.capture16(0x3b,0x7e3000)
                if mutation and len(cases)==1:p.long(0xaf,0x7e3000);p.emit(0x49,1);p.long(0x8f,0x7e3000)
                p.capture16(0x7b,0x7e3002)
                p.text(f'REG id={len(cases)-1:04d} ram=')
                for address in addresses:p.hex(address)
                p.text(' s=');p.hex(0x7e3001);p.hex(0x7e3000)
                p.text(' d=');p.hex(0x7e3003);p.hex(0x7e3002)
                # RAM is already captured; PHB/PLA are now safe probe effects.
                p.emit(0x8b,0x68);p.long(0x8f,0x7e3004)
                p.text(' db=');p.hex(0x7e3004);p.text('\n')
    p.reserve();p.text('LOG observations_complete\n');p.store(0x0021ff,1);p.emit(0x80,0xfe);p.hex_routine()
    p.data[0x7fc0:0x7fd5]=b'VELOCE STACK OBSERVE'.ljust(21,b' ')
    p.data[0x7fd5:0x7fdc]=bytes((0x20,0,12,0,1,0x33,0))
    p.data[0x7fdc:0x7fe0]=bytes((0xff,0xff,0,0))
    for vector in (0x7ffc,0x7ffe):p.data[vector:vector+2]=bytes((0,0x80))
    return bytes(p.data),cases
if __name__=='__main__':
    rom,_=build();out=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('stack_boundaries.sfc');out.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),out)
