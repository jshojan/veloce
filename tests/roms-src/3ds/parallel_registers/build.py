#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original ARM11 ARMv6 parallel arithmetic/SEL register corpus."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'reversal_registers/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
Program=asm.Program;immediate=asm.immediate
SHORT_VALUES=(0,1,127,128,255,32768,65534,65535)
def groups():
    for family in (1,2,3,5,6,7):
        for op in (0,1,2,3,4,7):
            for seed in ((0,) if op>=4 else (0x7fff8000,0xffff0001)):
                for flags in (0,31):yield family,op,seed,flags,0 if flags==0 else 15
    for ge in range(16):
        for flags in (0,31):yield 0,0,0,flags,ge
def instruction(family,op):
    return 0xe6004f18|(family<<20)|(op<<5) if family else 0xe6804fb8
def program(core,short=False,wrong_result=False,wrong_source=False,wrong_flags=False,wrong_select=False,wrong_ge=False):
    p=Program(0x20000000 if core==0 else 0x20040000)
    p.constant(12,0x1fe00000); p.constant(8,0x12345678); p.constant(9,0x89abcdef)
    count=152*(8 if short else 65536)
    if core==1:
        p.constant(6,0x2003f000); p.mark('wait'); p.memory(7,6); p.emit(0xe3570001); p.branch('wait',cond=1)
    if core==0: p.puts('header')
    p.puts('begin'); p.address(13,'records'); p.mark('group')
    for n,r in enumerate((2,3,6,7,10)): p.memory(r,13,n*4)
    p.memory(10,7,load=False)
    p.constant(1,0); p.mark('case')
    if short: p.address(6,'values'); p.emit(0xe7960101)
    else: p.emit(0xe1a00001)
    p.memory(6,13,8);p.emit(0xe3560001);p.branch('half_source',cond=0)
    p.emit(0xe1a08420);p.emit(0xe20000ff);p.emit(0xe1800400);p.emit(0xe1e06000);p.emit(0xe1800806)
    p.emit(0xe1e06008);p.emit(0xe20660ff);p.emit(0xe1888406);p.emit(0xe1888808);p.branch('shaped')
    p.mark('half_source');p.emit(0xe1e06000);p.emit(0xe1800806);p.memory(8,13,20)
    p.mark('shaped');p.address(11,'return');p.memory(7,13,12);p.emit(0xe12cf003);p.emit(0xe12fff17)
    p.mark('return');p.emit(0xe10f5000)
    if wrong_result:p.emit(0xe2844001)
    if wrong_flags:p.emit(0xe2255000|immediate(0x08000000))
    if wrong_ge:p.emit(0xe2255000|immediate(0x10000));p.emit(0xe124f005);p.emit(0xe10f5000)
    p.emit(0xe680afb8);p.emit(0xe10fb000)
    if wrong_select:p.emit(0xe22aa001)
    p.puts('row')
    for r in (4,5,0,8,2,3,10,11,9): p.emit(0xe1a06000|r); p.branch('word',link=True)
    p.constant(6,10); p.memory(6,12,load=False,byte=True)
    p.emit(0xe2811001); p.constant(6,8 if short else 65536);p.emit(0xe1510006); p.branch('case',cond=1)
    p.emit(0xe28dd01c); p.address(6,'records_end'); p.emit(0xe15d0006); p.branch('group',cond=1)
    p.puts('end')
    if core==0: p.constant(6,0x2003f000); p.constant(7,1); p.memory(7,6,load=False)
    else: p.puts('complete')
    p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
    p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
    p.mark('word'); p.memory(6,12,8,load=False); p.emit(0xe12fff1e)
    p.mark('arm_thunk'); p.emit(0xe6104f18); p.emit(0xe12fff1b)
    p.mark('values')
    for n in SHORT_VALUES: p.emit(n)
    p.mark('records')
    for family,op,seed,flags,ge in groups():
        requested=instruction(family,op)
        actual=(requested&~15)|1 if wrong_source else requested
        for word in (requested,flags<<27|ge<<16|0xdf,int(bool(family) and op<4),p.labels['arm_thunk'],actual,seed,0):p.emit(word)
    p.mark('records_end')
    p.string('header','VELOCE 1 3ds parallel_registers\n')
    p.string('begin',f'BEGIN arm11_{core} cases={count} words=9\n')
    p.string('end',f'BLOCK complete arm11_{core}\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={count*2}\nEND observations_complete\n')
    while len(p.data)%4: p.data.append(0)
    if len(p.data)>=0x3e000: raise ValueError('guest code overlaps synchronization')
    p.resolve(); return bytes(p.data)
def build(short=False,wrong_result=False,wrong_source=False,wrong_flags=False,wrong_select=False,wrong_ge=False):
    programs=[program(i,short,wrong_result,wrong_source,wrong_flags,wrong_select,wrong_ge) for i in (0,1)]
    p=Program(0x08000000);p.constant(12,0x1fe00000);p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle');p.resolve();programs.append(bytes(p.data))
    rom=bytearray(64);rom[:8]=b'V3DSP001'
    for core,(code,address) in enumerate(zip(programs,(0x20000000,0x20040000,0x08000000))):
        for n,value in enumerate((len(rom),address,address,len(code))): rom[16+core*16+n*4:20+core*16+n*4]=value.to_bytes(4,'little')
        rom.extend(code)
    return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('parallel_registers.v3ds'))
    args=parser.parse_args();args.output.write_bytes(build())
