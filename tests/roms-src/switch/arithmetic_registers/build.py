#!/usr/bin/env python3
"""Original MIT AArch64/NSO0 integer register guest; no SDK or firmware."""
from pathlib import Path
import argparse,struct
class Program:
    def __init__(self):self.code=bytearray();self.labels={};self.fixups=[]
    @property
    def pc(self):return len(self.code)
    def mark(self,label):self.labels[label]=self.pc
    def emit(self,w):self.code.extend(struct.pack('<I',w))
    def mov(self,reg,value,shift=0):self.emit(0xd2800000|(shift//16)<<21|(value&65535)<<5|reg)
    def adr(self,reg,label):self.fixups.append((self.pc,'adr',label));self.emit(0x10000000|reg)
    def branch(self,label,cond=None,link=False):
        self.fixups.append((self.pc,'b' if cond is None else 'cond',label));self.emit((0x94000000 if link else 0x14000000) if cond is None else 0x54000000|cond)
    def cbz(self,reg,label):self.fixups.append((self.pc,'cond',label));self.emit(0xb4000000|reg)
    def put(self,label):self.adr(4,label);self.branch('puts',link=True)
    def resolve(self):
        for at,kind,label in self.fixups:
            offset=self.labels[label]-at;w=struct.unpack_from('<I',self.code,at)[0]
            if kind=='adr':
                assert -(1<<20)<=offset<(1<<20)
                value=offset&0x1fffff;w|=(value&3)<<29|(value>>2)<<5
            elif kind=='b':assert offset%4==0;w|=(offset//4)&0x3ffffff
            else:assert offset%4==0;w|=((offset//4)&0x7ffff)<<5
            struct.pack_into('<I',self.code,at,w)
FORMS=(0x2b010002,0x6b010002,0x1a010002,0x3a010002,0x5a010002,0x7a010002)
def build(short=False,wrong_capture=False):
    p=Program();domain=4 if short else 256;count=domain*domain*24
    p.mov(28,0xffff,16);p.put('header');p.adr(21,'table');p.mov(19,domain)
    p.mark('a_loop');p.emit(0xf94002a0);p.adr(22,'table');p.mov(20,domain)
    p.mark('b_loop');p.emit(0xf94002c1)
    for carry in range(2):
        for width in (32,64):
            for form,w in enumerate(FORMS):
                context=carry*12+(width==64)*6+form
                p.mov(3,(9 if not carry else 15)<<12,16);p.emit(0xd51b4203)
                p.emit(w|((width==64)<<31));p.emit(0xd53b4203)
                if wrong_capture:p.mov(6,1);p.emit(0xca060042) # Actual EOR flips one captured result bit.
                p.emit(0xb900139f) # STR WZR,[X28,#16]: begin actual capture row.
                for reg in (0,1,2,3):p.emit(0xf9000780|reg) # STR Xn,[X28,#8].
                p.mov(5,context);p.emit(0xf9000785);p.mov(5,10);p.emit(0x39000385)
    p.emit(0x910022d6);p.emit(0xf1000694);p.branch('b_loop',cond=1)
    p.emit(0x910022b5);p.emit(0xf1000673);p.branch('a_loop',cond=1)
    p.put('complete');p.emit(0xb900079f);p.mark('halt');p.branch('halt')
    p.mark('puts');p.emit(0x39400085);p.cbz(5,'return');p.emit(0x39000385);p.emit(0x91000484);p.branch('puts')
    p.mark('return');p.emit(0xd65f03c0)
    ro=bytearray();
    for label,text in (('header',f'VELOCE 1 switch arithmetic_registers\nBEGIN arithmetic cases={count} words=5\n'),('complete',f'LOG complete cases={count}\nEND observations_complete\n')):
        p.labels[label]=0x10000+len(ro);ro.extend(text.encode()+b'\0')
    while len(ro)%8:ro.append(0)
    p.labels['table']=0x10000+len(ro)
    for value in range(256):ro.extend((value*0x0101010101010101).to_bytes(8,'little'))
    p.resolve();header=bytearray(256);header[:4]=b'NSO0'
    for n,(offset,address,data) in enumerate(((256,0,p.code),(256+len(p.code),0x10000,ro),(256+len(p.code)+len(ro),0x20000,b'\0'*16))):
        struct.pack_into('<III',header,16+n*16,offset,address,len(data))
    struct.pack_into('<I',header,60,4096)
    return bytes(header+p.code+ro+b'\0'*16)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('arithmetic_registers.nso'));args=parser.parse_args();args.output.write_bytes(build())
