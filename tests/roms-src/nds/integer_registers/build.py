#!/usr/bin/env python3
"""Original DS dual-CPU ARM register cartridge, emitted without an SDK."""
import argparse
import hashlib
from pathlib import Path
OPS=('and','eor','sub','rsb','add','adc','sbc','rsc','tst','teq','cmp','cmn','orr','mov','bic','mvn')
PATTERNS=(0,1,2,15,16,127,128,255,0x7fff,0x8000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,0x55555555,0x89abcdef)
def groups(full=True):
    result=[]
    for kind,values in (('boundary',PATTERNS),('byte',tuple(n*0x01010101 for n in range(256 if full else 16)))):
        for opcode,op in enumerate(OPS): result.append((op+'_'+kind,op,opcode,(0,) if op in ('mov','mvn') else values,values))
    return tuple(result)
def cases(group,cpu=7): return len(group[3])*len(group[4])*(32 if cpu==9 else 16)

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

def program(cpu,full=True,wrong_adc=False):
    p=Program(0x02004000 if cpu==9 else 0x03800000); domain=groups(full)
    p.constant(12,0x04ff0000); p.constant(13,0x02001000)
    if cpu==7:
        p.mark('wait'); p.memory(6,13); p.emit(0xe3560001); p.branch('wait',cond=1)
    p.puts('header')
    for name,op,opcode,left,right in domain:
        p.puts(name+'_header'); p.address(8,name+'_left_values'); p.constant(9,len(left))
        p.mark(name+'_left'); p.memory(0,8); p.address(10,name+'_right_values'); p.constant(11,len(right))
        p.mark(name+'_right'); p.memory(1,10); p.constant(2,0); p.mark(name+'_flags')
        p.constant(4,0xdeadbeef); p.emit(0xe1a03002|((27 if cpu==9 else 28)<<7)); p.emit(0xe128f003)  # flags index <<28, MSR CPSR_f
        actual=4 if wrong_adc and op=='adc' else opcode
        p.emit(0xe0104001|(actual<<21)); p.emit(0xe10f5000)  # data op S, rd4/rn0/rm1; MRS r5
        p.puts('row')
        for reg in (4,5,0,1): p.emit(0xe1a06000|reg); p.branch('hex',link=True)
        p.constant(6,10); p.memory(6,12,load=False,byte=True)
        p.emit(0xe2822001); p.emit(0xe3520000|(32 if cpu==9 else 16)); p.branch(name+'_flags',cond=1)
        p.emit(0xe28aa004); p.emit(0xe25bb001); p.branch(name+'_right',cond=1)
        p.emit(0xe2888004); p.emit(0xe2599001); p.branch(name+'_left',cond=1)
    p.puts('complete')
    if cpu==9: p.constant(6,1); p.memory(6,13,load=False)
    p.constant(6,0); p.memory(6,12,4,load=False); p.branch('idle'); p.mark('idle'); p.branch('idle')
    p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
    p.mark('hex'); p.constant(3,28); p.mark('hex_loop')
    p.emit(0xe1a07336); p.emit(0xe207700f); p.emit(0xe357000a); p.emit(0x32877030); p.emit(0x22877057)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2533004); p.branch('hex_loop',cond=10); p.emit(0xe12fff1e)
    for name,_,_,left,right in domain:
        for label,values in ((name+'_left_values',left),(name+'_right_values',right)):
            p.mark(label)
            for value in values: p.emit(value)
    total=sum(cases(g,cpu) for g in domain)
    p.string('header',f'VELOCE 1 ds integer_registers cpu={cpu}\nINFO full={int(full)} groups={len(domain)} flags={32 if cpu==9 else 16}\n')
    for g in domain: p.string(g[0]+'_header',f'BEGIN cpu{cpu}_{g[0]} cases={cases(g,cpu)} words=4\n')
    p.string('row','REG r=')
    p.string('complete',f'LOG cpu={cpu} complete cases={total}\n'+('END observations_complete\n' if cpu==7 else ''))
    while len(p.data)%4: p.data.append(0)
    p.resolve(); return bytes(p.data)

def build(full=True,wrong_adc=False):
    nine=program(9,full,wrong_adc); seven=program(7,full,wrong_adc)
    offset7=(512+len(nine)+511)&~511
    rom=bytearray(offset7+len(seven)); rom[:12]=b'VELOCE ARM  '; rom[12:16]=b'VARM'; rom[0x180:0x188]=b'VDSHB001'
    for offset,fields in ((0x20,(512,0x02004000,0x02004000,len(nine))),(0x30,(offset7,0x03800000,0x03800000,len(seven)))):
        for n,field in enumerate(fields): rom[offset+n*4:offset+n*4+4]=field.to_bytes(4,'little')
    rom[0x80:0x84]=len(rom).to_bytes(4,'little'); rom[0x84:0x88]=(512).to_bytes(4,'little')
    rom[512:512+len(nine)]=nine; rom[offset7:]=seven
    return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('integer_registers.nds'))
    args=parser.parse_args(); rom=build(); args.output.write_bytes(rom); print(hashlib.sha256(rom).hexdigest(),args.output)
