#!/usr/bin/env python3
"""Original Nintendo V810 register stimuli; no expected answers in the guest."""
import argparse,hashlib,importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('owned_v810_assembler',Path(__file__).parents[1]/'immediates/build.py')
assembler=importlib.util.module_from_spec(spec);spec.loader.exec_module(assembler)
Program=assembler.Program
SHORT16=(0,1,127,128,255,256,32767,32768,65535)
SHORT17=(0,1,32767,32768,65535,65536,65537,131071)
SHORTPSW=(0,1,2,15,0x1000,0x8000,0xff3ff)
def groups():
    return tuple((f'xb{i}',8,upper,0) for i,upper in enumerate((0,0xffff0000,0x89ab0000,0x80000000)))+\
           tuple((f'xh{i}',9,low,0) for i,low in enumerate((0,0xffff,0x89ab,0x8000)))+\
           tuple((f'rev{i}',10,i,0) for i in range(3))+\
           tuple((f'mpy{i}-{j}',12,upper,b) for i,upper in enumerate((0,0x55540000,0xaaa80000,0xfffe0000)) for j,b in enumerate((0,1,0x80000000,0x89abcdef)))+\
           (('cli',0x16,0,0),('sei',0x1e,0,0))
def domain(sub,full):
    if sub in (0x16,0x1e):return tuple((x&1023)|((x>>10)<<12) for x in range(262144)) if full else SHORTPSW
    return range(131072 if sub==12 else 65536) if full else (SHORT17 if sub==12 else SHORT16)
def flags(sub):return (0,) if sub>=16 else (0,15) if sub==12 else range(16)
def cases(g,full):return len(domain(g[1],full))*len(flags(g[1]))
def build(full=True,wrong_result=False,wrong_flags=False,wrong_mpyhw16=False,only=None):
    p=Program();gs=tuple(g for g in groups() if only is None or g[0]==only)
    if not gs:raise ValueError('empty group')
    p.constant(28,0x04000000);p.constant(26,0x05001000)
    p.constant(1,(6<<10)|31);p.wide(0x37,26,1,4);p.puts('header')
    for name,sub,parameter,b in gs:
        p.puts(name+'_header');p.address(16,'raw'+str(sub if sub>=16 or sub==12 else 8));p.constant(17,len(domain(sub,full)))
        instruction=(sub<<10) if sub>=16 else (0x3e<<10|20<<5|(25 if sub in (10,12) else 0))|((sub<<10)<<16)
        p.constant(24,instruction);p.wide(0x37,26,24,0);p.mark(name+'_raw');p.wide(0x33,16,18,0)
        p.reg(0x10,0,19);p.mark(name+'_flags')
        p.constant(25,0x76543210);p.constant(23,0x89abcdef)
        if sub==8:p.constant(23,parameter);p.reg(0x0c,18,23)
        elif sub==9:p.reg(0,18,23);p.reg(0x14,16,23);p.constant(6,parameter);p.reg(0x0c,6,23)
        elif sub==10:
            p.reg(0,18,25)
            if parameter in (1,2):
                p.reg(0,18,6);p.reg(0x14,16,6)
                if parameter==1:p.reg(0,6,25)
                else:p.reg(0x0c,6,25)
        elif sub==12:
            p.constant(25,parameter);p.reg(0x0c,18,25);p.constant(23,b)
            if wrong_mpyhw16:p.reg(0x14,16,25);p.reg(0x17,16,25)
        p.reg(0,23,20);p.constant(30,0xdeadbeef);p.constant(2,0x76543210)
        if sub>=16:p.reg(0,18,22)
        else:
            p.constant(22,0x8000);p.reg(0,19,6)
            if sub==12:p.reg(0x14,4,6);p.reg(2,19,6)
            p.reg(0x0c,6,22)
        p.address(31,name+'_return');p.reg(0x1c,5,22);p.reg(6,26);p.mark(name+'_return');p.reg(0x1d,5,21)
        if wrong_result:p.reg(0x10,1,6);p.reg(0x0e,6,20)
        if wrong_flags:p.reg(0x10,1,6);p.reg(0x0e,6,21)
        p.puts('row')
        for rt in (20,21,25,23,30,2,24,22):p.wide(0x37,28,rt,8)
        p.reg(0x10,10,6);p.wide(0x34,28,6,0)
        p.reg(0x11,1,19);p.constant(7,len(flags(sub)));p.reg(3,7,19);p.branch(2,name+'_flags_done');p.jump(0x2a,name+'_flags');p.mark(name+'_flags_done')
        p.wide(0x28,16,16,4);p.reg(0x11,31,17);p.branch(2,name+'_raw_done');p.jump(0x2a,name+'_raw');p.mark(name+'_raw_done')
    p.puts('complete');p.wide(0x37,28,0,4);p.reg(0x1a)
    p.mark('puts');p.wide(0x30,10,6,0);p.reg(0x13,0,6);p.branch(2,'puts_return')
    p.wide(0x34,28,6,0);p.reg(0x11,1,10);p.jump(0x2a,'puts');p.mark('puts_return');p.reg(6,31)
    while p.pc&3:p.data.append(0)
    for sub in sorted(set(s if s>=16 or s==12 else 8 for _,s,_,_ in gs)):
        p.mark('raw'+str(sub))
        for raw in domain(sub,full):p.data.extend(raw.to_bytes(4,'little'))
    total=sum(cases(g,full) for g in gs)
    p.string('header',f'VELOCE 1 vb nintendo\nINFO full={int(full)} groups={len(gs)}\n')
    for g in gs:p.string(g[0]+'_header',f'BEGIN {g[0]} cases={cases(g,full)} words=8\n')
    p.string('row','REG r=');p.string('complete',f'LOG complete cases={total}\nEND observations_complete\n')
    p.resolve();size=1024
    while size<len(p.data)+0x200:size*=2
    p.data.extend(bytes(size-len(p.data)));p.data[size-0x200:size-0x1f8]=b'VVBHB001'
    p.data[-16:-10]=((0x2f<<10)|(10<<5)).to_bytes(2,'little')+bytes.fromhex('0007')+((6<<10)|10).to_bytes(2,'little')
    return bytes(p.data)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('nintendo_registers.vb'));a=p.parse_args();rom=build();a.output.write_bytes(rom);print(hashlib.sha256(rom).hexdigest(),a.output)
