#!/usr/bin/env python3
"""Original V810 integer ROM emitted without SDK or copyrighted assets."""
import argparse
import hashlib
from pathlib import Path

PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,
          0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
SPECS=(("add",1),("sub",2),("cmp",3),("or",12),("and",13),("xor",14))
SHIFTS=(("shl",4),("shr",5),("sar",7))


def groups(byte_values=256):
    if byte_values not in (16,256): raise ValueError("bad byte domain")
    result=[]
    for kind,values in (("boundary",PATTERNS),("byte",tuple(n*0x01010101 for n in range(byte_values)))):
        for name,opcode in SPECS: result.append((name+'_'+kind,name,opcode,values,values))
    for name,opcode in SHIFTS:
        result.append((name+'_amounts',name,opcode,PATTERNS,tuple(range(byte_values))))
    return tuple(result)


class Program:
    def __init__(self): self.data=bytearray(); self.labels={}; self.fixups=[]
    @property
    def pc(self): return 0x07000000+len(self.data)
    def mark(self,name):
        if name in self.labels: raise ValueError("duplicate label")
        self.labels[name]=self.pc
    def half(self,value): self.data.extend((value&65535).to_bytes(2,'little'))
    def reg(self,op,r1=0,r2=0): self.half(op<<10|r2<<5|r1)
    def wide(self,op,r1,r2,imm): self.reg(op,r1,r2); self.half(imm)
    def constant(self,rt,value):
        self.wide(0x2f,0,rt,value>>16); self.wide(0x2c,rt,rt,value&65535)
    def address(self,rt,label):
        self.fixups.append((len(self.data),label,'address')); self.constant(rt,0)
    def branch(self,cond,label):
        self.fixups.append((len(self.data),label,'branch')); self.half(0x8000|cond<<9)
    def jump(self,op,label):
        self.fixups.append((len(self.data),label,'jump')); self.half(op<<10); self.half(0)
    def puts(self,label): self.address(10,label); self.jump(0x2b,'puts')
    def string(self,label,text): self.mark(label); self.data.extend(text.encode('ascii')+b'\0')
    def resolve(self):
        for offset,label,kind in self.fixups:
            target=self.labels[label]; pc=0x07000000+offset
            if kind=='address':
                self.data[offset+2:offset+4]=(target>>16).to_bytes(2,'little')
                self.data[offset+6:offset+8]=(target&65535).to_bytes(2,'little')
            elif kind=='branch':
                delta=target-pc
                if delta&1 or not -256<=delta<256: raise ValueError("branch range")
                word=int.from_bytes(self.data[offset:offset+2],'little')|delta&0x1fe
                self.data[offset:offset+2]=word.to_bytes(2,'little')
            else:
                delta=target-pc
                if delta&1 or not -(1<<25)<=delta<(1<<25): raise ValueError("jump range")
                word=int.from_bytes(self.data[offset:offset+2],'little')|((delta>>16)&0x3ff)
                self.data[offset:offset+2]=word.to_bytes(2,'little')
                self.data[offset+2:offset+4]=(delta&65535).to_bytes(2,'little')


def build(byte_values=256,wrong_add=False):
    p=Program(); domain=groups(byte_values)
    p.constant(28,0x04000000); p.wide(0x28,0,27,16)
    p.puts('header')
    for name,operation,opcode,left,right in domain:
        p.puts(name+'_header')
        p.address(16,'patterns' if 'boundary' in name or operation in dict(SHIFTS) else 'bytes')
        p.wide(0x28,0,17,len(left)); p.mark(name+'_left')
        p.wide(0x33,16,23,0)
        table='amounts' if operation in dict(SHIFTS) else 'patterns' if 'boundary' in name else 'bytes'
        p.address(18,table); p.wide(0x28,0,19,len(right)); p.mark(name+'_right')
        p.wide(0x33,18,24,0); p.reg(0x10,0,22); p.mark(name+'_flags')
        p.reg(0,23,20); p.reg(0,24,25); p.reg(0x1c,5,22)
        p.reg(2 if wrong_add and operation=='add' else opcode,25,20)
        p.reg(0x1d,5,21)  # Actual PSW immediately after the instruction.
        p.puts('row'); p.reg(0,20,10); p.jump(0x2b,'hex'); p.reg(0,21,10); p.jump(0x2b,'hex')
        p.reg(0x10,10,6); p.wide(0x34,28,6,0)
        p.reg(0x11,1,22); p.reg(3,27,22); p.branch(10,name+'_flags')
        p.wide(0x28,18,18,4); p.reg(0x11,31,19); p.branch(10,name+'_right')
        p.wide(0x28,16,16,4); p.reg(0x11,31,17); p.branch(10,name+'_left')
    p.puts('complete'); p.wide(0x37,28,0,4); p.reg(0x1a)
    p.mark('puts'); p.wide(0x30,10,6,0); p.reg(0x13,0,6); p.branch(2,'puts_return')
    p.wide(0x34,28,6,0); p.reg(0x11,1,10); p.jump(0x2a,'puts')
    p.mark('puts_return'); p.reg(6,31)
    p.mark('hex'); p.wide(0x28,0,7,28); p.mark('hex_loop')
    p.reg(0,10,6); p.reg(5,7,6); p.wide(0x2d,6,6,15); p.reg(0x13,10,6)
    p.branch(1,'digit'); p.wide(0x29,6,6,39)
    p.mark('digit'); p.wide(0x28,6,6,48); p.wide(0x34,28,6,0)
    p.reg(0x11,28,7); p.branch(14,'hex_loop'); p.reg(6,31)
    while p.pc&3: p.data.append(0)
    for label,values in (('patterns',PATTERNS),('bytes',tuple(n*0x01010101 for n in range(byte_values))),('amounts',tuple(range(byte_values)))):
        p.mark(label)
        for value in values: p.data.extend(value.to_bytes(4,'little'))
    total=sum(len(left)*len(right)*16 for *_,left,right in domain)
    p.string('header',f'VELOCE 1 vb integer_registers\nINFO byte_values={byte_values} flags=16 groups={len(domain)}\n')
    for name,_,_,left,right in domain: p.string(name+'_header',f'BEGIN {name} cases={len(left)*len(right)*16} words=2\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={total}\nEND observations_complete\n')
    p.resolve(); size=1024
    while size<len(p.data)+0x200: size*=2
    p.data.extend(bytes(size-len(p.data))); p.data[size-0x200:size-0x1f8]=b'VVBHB001'
    # Reset starts at fffffff0, mirrored to the last 16 bytes of this cartridge.
    p.data[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little')
    return bytes(p.data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('integer_registers.vb'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom)
    print(f'{hashlib.sha256(rom).hexdigest()}  {args.output}')
