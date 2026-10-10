#!/usr/bin/env python3
"""Original ARM11 reversal instructions, complete stimuli and register capture."""
import argparse
from pathlib import Path
SHORT_VALUES=(0,1,127,128,255,32768,65534,65535)
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

def program(core,short=False,wrong_result=False,wrong_half=False,wrong_signed=False,wrong_flags=False):
    p=Program(0x20000000 if core==0 else 0x20010000)
    p.constant(12,0x1fe00000); p.constant(8,0x12345678); p.constant(9,0x89abcdef)
    count=6*32*(8 if short else 65536)
    if core==1:
        p.constant(6,0x20008000); p.mark('wait'); p.memory(7,6); p.emit(0xe3570001); p.branch('wait',cond=1)
    if core==0: p.puts('header')
    p.puts('begin'); p.address(11,'return'); p.address(13,'records'); p.mark('group')
    for n,r in enumerate((2,3,10,7,6)): p.memory(r,13,n*4)
    p.emit(0xe3520000|immediate(65536)); p.emit(0x31ca60b0); p.emit(0x258a6000)
    p.constant(1,0); p.mark('case')
    if short: p.address(6,'values'); p.emit(0xe7960101)
    else: p.emit(0xe1a00001)
    p.emit(0xe1e07000); p.emit(0xe1800807)
    p.memory(7,13,12); p.emit(0xe128f003); p.emit(0xe12fff17)
    p.mark('return'); p.emit(0xe10f5000)
    if wrong_result: p.emit(0xe2844001)
    if wrong_flags: p.emit(0xe2255000|immediate(0x08000000))
    p.puts('row')
    for r in (4,5,0,2,3,8,9): p.emit(0xe1a06000|r); p.branch('word',link=True)
    p.constant(6,10); p.memory(6,12,load=False,byte=True)
    p.emit(0xe2811001); p.constant(6,8 if short else 65536); p.emit(0xe1510006); p.branch('case',cond=1)
    p.emit(0xe28dd014); p.address(6,'records_end'); p.emit(0xe15d0006); p.branch('group',cond=1)
    p.puts('end')
    if core==0: p.constant(6,0x20008000); p.constant(7,1); p.memory(7,6,load=False)
    else: p.puts('complete')
    p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
    p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
    p.mark('word'); p.memory(6,12,8,load=False); p.emit(0xe12fff1e)
    p.mark('arm_thunk'); p.emit(0xe1a04000); p.emit(0xe12fff1b)
    p.mark('thumb_thunk'); p.emit(0x4758ba04)
    p.mark('values')
    for n in SHORT_VALUES: p.emit(n)
    p.mark('records')
    for thumb in (False,True):
        for kind in range(3):
            instruction=(0xba04+(0,0x40,0xc0)[kind]) if thumb else (0xe6bf4f30,0xe6bf4fb0,0xe6ff4fb0)[kind]
            actual_kind=0 if wrong_half and kind==1 else 1 if wrong_signed and kind==2 else kind
            actual=(0xba04+(0,0x40,0xc0)[actual_kind]) if thumb else (0xe6bf4f30,0xe6bf4fb0,0xe6ff4fb0)[actual_kind]
            thunk=p.labels['thumb_thunk' if thumb else 'arm_thunk']
            for flags in range(32):
                for word in (instruction,flags<<27|0xdf,thunk,thunk|int(thumb),actual): p.emit(word)
    p.mark('records_end')
    p.string('header','VELOCE 1 3ds reversal_registers\n')
    p.string('begin',f'BEGIN arm11_{core} cases={count} words=7\n')
    p.string('end',f'BLOCK complete arm11_{core}\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={count*2}\nEND observations_complete\n')
    while len(p.data)%4: p.data.append(0)
    if len(p.data)>=0x8000: raise ValueError('guest code overlaps synchronization')
    p.resolve(); return bytes(p.data)
def build(short=False,wrong_result=False,wrong_half=False,wrong_signed=False,wrong_flags=False):
    programs=[program(i,short,wrong_result,wrong_half,wrong_signed,wrong_flags) for i in (0,1)]
    p=Program(0x08000000);p.constant(12,0x1fe00000);p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle');p.resolve();programs.append(bytes(p.data))
    rom=bytearray(64);rom[:8]=b'V3DSP001'
    for core,(code,address) in enumerate(zip(programs,(0x20000000,0x20010000,0x08000000))):
        for n,value in enumerate((len(rom),address,address,len(code))): rom[16+core*16+n*4:20+core*16+n*4]=value.to_bytes(4,'little')
        rom.extend(code)
    return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('reversal_registers.v3ds'))
    args=parser.parse_args();args.output.write_bytes(build())
