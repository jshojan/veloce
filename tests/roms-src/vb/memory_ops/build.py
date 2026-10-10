#!/usr/bin/env python3
"""Original V810 CAXI and port-transaction register cartridge."""
import argparse
import hashlib
from pathlib import Path
PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,
          0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
def groups(full=True):
    count=256 if full else 16; half=65536 if full else 16
    byte=tuple(n*0x01010101 for n in range(count)); words=tuple(n*0x00010001 for n in range(half))
    return (('caxi_compare',byte,byte,(0x13579bdf,)),('caxi_replace',byte,byte,(0x13579bdf,)),
            ('caxi_boundary',PATTERNS,PATTERNS,PATTERNS),
            ('port_byte',byte,(),()),('port_half',words,(),()),('port_word',words,(),()))
def cases(group):
    name,left,right,third=group
    return len(left)*64*(len(right)*len(third) if name.startswith('caxi') else 1)

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
def build(full=True,wrong_caxi=False,wrong_port=False):
    p=Program(); domain=groups(full)
    p.constant(28,0x04000000); p.constant(13,0x05002000)
    p.constant(14,0x05001ffc); p.constant(15,0x05002004)
    p.constant(4,0x8000); p.constant(3,16)
    p.constant(1,0xdeadbeef); p.wide(0x37,14,1,0)
    p.constant(2,0x76543210); p.wide(0x37,15,2,0); p.puts('header')
    for group in domain:
        name,left,right,third=group; caxi=name.startswith('caxi')
        p.puts(name+'_header'); p.address(16,name+'_left'); p.constant(17,len(left))
        p.mark(name+'_left_loop'); p.wide(0x33,16,23,0)
        if caxi:
            p.address(18,name+'_right'); p.constant(19,len(right)); p.mark(name+'_right_loop'); p.wide(0x33,18,24,0)
            p.address(29,name+'_third'); p.constant(27,len(third)); p.mark(name+'_third_loop'); p.wide(0x33,29,11,0)
        else: p.reg(0,23,24)
        p.reg(0x10,0,26); p.mark(name+'_lane_loop'); p.reg(0x10,0,22); p.mark(name+'_flags_loop')
        p.reg(0,13,12); p.reg(1,26,12)
        if caxi:
            p.wide(0x37,13,23,0); p.reg(0,23 if name=='caxi_replace' else 24,20)
            p.reg(0,24 if name=='caxi_replace' else 11,30)
            p.reg(0,22,6); p.reg(0x0c,4,6); p.reg(0x1c,5,6)
            p.wide(0x37 if wrong_caxi else 0x3a,12,20,0)
            p.reg(0x1d,5,21); p.wide(0x33,13,25,0)
            row=(20,21,25,1,2,30,12,23,24)
        else:
            p.constant(11,0x89abcdef); p.wide(0x37,13,11,0)
            p.reg(0,22,6); p.reg(0x0c,4,6); p.reg(0x1c,5,6)
            out_op={'port_byte':0x3c,'port_half':0x3d,'port_word':0x3f}[name]
            in_op={'port_byte':0x38,'port_half':0x39,'port_word':0x3b}[name]
            p.wide(0x3f if wrong_port and name=='port_half' else out_op,12,24,0)
            p.reg(0x1d,5,21); p.wide(in_op,12,20,0); p.reg(0x1d,5,25)
            p.wide(0x33,13,11,0); row=(20,21,25,11,1,2,24,12,23)
        p.wide(0x33,14,1,0); p.wide(0x33,15,2,0)
        p.puts('row')
        for rt in row: p.reg(0,rt,10); p.jump(0x2b,'hex')
        p.reg(0x10,10,6); p.wide(0x34,28,6,0)
        p.reg(0x11,1,22); p.reg(3,3,22); p.branch(2,name+'_flags_done'); p.jump(0x2a,name+'_flags_loop'); p.mark(name+'_flags_done')
        p.reg(0x11,1,26); p.reg(0x13,4,26); p.branch(2,name+'_lane_done'); p.jump(0x2a,name+'_lane_loop'); p.mark(name+'_lane_done')
        if caxi:
            p.wide(0x28,29,29,4); p.reg(0x11,31,27)
            p.branch(2,name+'_third_done'); p.jump(0x2a,name+'_third_loop'); p.mark(name+'_third_done')
            p.wide(0x28,18,18,4); p.reg(0x11,31,19)
            p.branch(2,name+'_right_done'); p.jump(0x2a,name+'_right_loop'); p.mark(name+'_right_done')
        p.wide(0x28,16,16,4); p.reg(0x11,31,17)
        p.branch(2,name+'_left_done'); p.jump(0x2a,name+'_left_loop'); p.mark(name+'_left_done')
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
    for name,left,right,third in domain:
        for suffix,values in (('left',left),('right',right),('third',third)):
            p.mark(name+'_'+suffix)
            for value in values: p.data.extend(value.to_bytes(4,'little'))
    total=sum(cases(g) for g in domain)
    p.string('header',f'VELOCE 1 vb memory_ops\nINFO full={int(full)} groups={len(domain)}\n')
    for g in domain: p.string(g[0]+'_header',f'BEGIN {g[0]} cases={cases(g)} words=9\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={total}\nEND observations_complete\n')
    p.resolve(); size=1024
    while size<len(p.data)+0x200: size*=2
    p.data.extend(bytes(size-len(p.data))); p.data[size-0x200:size-0x1f8]=b'VVBHB001'
    p.data[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little')
    return bytes(p.data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('memory_ops.vb'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom)
    print(f'{hashlib.sha256(rom).hexdigest()}  {args.output}')
