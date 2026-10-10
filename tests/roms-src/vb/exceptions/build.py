#!/usr/bin/env python3
"""Original V810 TRAP and reserved-opcode exception cartridge."""
import argparse
import hashlib
from pathlib import Path
FAULT_PC=0x05003000
RESERVED=(0x1b,0x32,0x36)
def status_values(full=True):
    # Enumerate every writable field while NP is clear. The reduced encoding
    # sweep still exercises each condition flag and both saved-context banks.
    if not full: return tuple(flags|ep for ep in (0,0x4000) for flags in range(16))
    return tuple(low|(id_<<12)|(ae<<13)|(ep<<14)|(level<<16)
        for ep in range(2) for level in range(16) for ae in range(2)
        for id_ in range(2) for low in range(1024))
def groups(full=True):
    result=[]; complete=status_values(full); short=status_values(False)
    for vector in range(32):
        result.append((f'trap_{vector:02d}',0,(0x18<<10|vector,),complete))
    for op in RESERVED:
        result.append((f'illegal_{op:02x}_status',2,(op<<10,),complete))
        result.append((f'illegal_{op:02x}_encoding',2,tuple(op<<10|n for n in range(1024)),short))
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
def build(full=True,wrong_trap=False,wrong_capture=False):
    p=Program(); domain=groups(full)
    p.constant(28,0x04000000); p.puts('header')
    p.constant(10,FAULT_PC); p.wide(0x28,0,6,(6<<10)|7); p.wide(0x35,10,6,2)
    for name,advance,words,statuses in domain:
        p.puts(name+'_header'); p.address(16,name+'_words'); p.constant(17,len(words))
        p.wide(0x28,0,14,advance); p.mark(name+'_word_loop')
        p.wide(0x31,16,19,0); p.constant(10,FAULT_PC); p.wide(0x35,10,19,0)
        p.address(26,'full_status' if full and not name.endswith('_encoding') else 'short_status')
        p.constant(27,len(statuses)); p.mark(name+'_status_loop')
        p.wide(0x33,26,18,0)
        for sr,value in ((0,0x05001000),(1,0x101),(2,0x05002000),(3,0x202)):
            p.constant(29,value); p.reg(0x1c,sr,29)
        p.constant(1,0x89abcdef); p.constant(2,0x76543210); p.constant(30,0x13579bdf)
        p.reg(0,0,8); p.reg(0,0,9); p.reg(0,0,11); p.reg(0,0,12)
        p.address(7,name+'_returned'); p.constant(10,FAULT_PC); p.reg(0x1c,5,18); p.reg(6,10)
        p.mark(name+'_returned')
        for sr,rt in enumerate(range(20,26)): p.reg(0x1d,sr,rt)
        p.puts('row')
        for rt in (20,21,22,23,24,25,8,9,11,12,1,2,30):
            p.reg(0,rt,10); p.jump(0x2b,'hex')
        p.reg(0x10,10,6); p.wide(0x34,28,6,0)
        p.wide(0x28,26,26,4); p.reg(0x11,31,27)
        p.branch(2,name+'_status_done'); p.jump(0x2a,name+'_status_loop'); p.mark(name+'_status_done')
        p.wide(0x28,16,16,2); p.reg(0x11,31,17)
        p.branch(2,name+'_word_done'); p.jump(0x2a,name+'_word_loop'); p.mark(name+'_word_done')
    p.puts('complete'); p.wide(0x37,28,0,4); p.reg(0x1a)
    p.mark('handler'); p.reg(0x1d,5,11); p.wide(0x2d,11,6,0x8000)
    p.reg(0x13,0,6); p.branch(10,'nested')
    p.reg(0x1d,0,9); p.reg(0x1d,4 if wrong_capture else 1,12)
    p.reg(0,9,29); p.reg(1,14,29); p.reg(0x1c,0,29); p.reg(0x19)
    p.mark('nested'); p.reg(0x1d,2,9); p.reg(0x1d,4 if wrong_capture else 3,12)
    p.reg(0,9,29); p.reg(1,14,29); p.reg(0x1c,2,29); p.reg(0x19)
    p.mark('puts'); p.wide(0x30,10,6,0); p.reg(0x13,0,6); p.branch(2,'puts_return')
    p.wide(0x34,28,6,0); p.reg(0x11,1,10); p.jump(0x2a,'puts')
    p.mark('puts_return'); p.reg(6,31)
    p.mark('hex'); p.wide(0x28,0,7,28); p.mark('hex_loop')
    p.reg(0,10,6); p.reg(5,7,6); p.wide(0x2d,6,6,15); p.reg(0x13,10,6)
    p.branch(1,'digit'); p.wide(0x29,6,6,39)
    p.mark('digit'); p.wide(0x28,6,6,48); p.wide(0x34,28,6,0)
    p.reg(0x11,28,7); p.branch(14,'hex_loop'); p.reg(6,31)
    while p.pc&3: p.data.append(0)
    for name,_,words,_ in domain:
        p.mark(name+'_words')
        for word in words:
            if wrong_trap and name.startswith('trap_'): word^=16
            p.half(word)
    while p.pc&3: p.data.append(0)
    for label,values in (('full_status',status_values(full)),('short_status',status_values(False))):
        p.mark(label)
        for value in values: p.data.extend(value.to_bytes(4,'little'))
    total=sum(len(words)*len(statuses) for _,_,words,statuses in domain)
    p.string('header',f'VELOCE 1 vb exceptions\nINFO full={int(full)} groups={len(domain)}\n')
    for name,_,words,statuses in domain: p.string(name+'_header',f'BEGIN {name} cases={len(words)*len(statuses)} words=13\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={total}\nEND observations_complete\n')
    p.resolve(); size=1024
    while size<len(p.data)+0x200: size*=2
    p.data.extend(bytes(size-len(p.data))); p.data[size-0x200:size-0x1f8]=b'VVBHB001'
    for offset,tag in ((0x90,0),(0xa0,1),(0xb0,2),(0xd0,3)):
        vector=Program(); handler=p.labels['handler']; vector.reg(0x10,tag,8)
        vector.wide(0x2f,0,29,(handler+0x8000)>>16); vector.wide(0x28,29,29,handler&65535); vector.reg(6,29)
        assert len(vector.data)<=16
        pos=size-0x100+offset; p.data[pos:pos+len(vector.data)]=vector.data
    p.data[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little')
    return bytes(p.data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('exceptions.vb'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom)
    print(f'{hashlib.sha256(rom).hexdigest()}  {args.output}')
