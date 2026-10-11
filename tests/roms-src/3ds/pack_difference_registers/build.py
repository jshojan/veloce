#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original ARM11 ARMv6 packing and absolute differences register corpus."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'reversal_registers/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
Program=asm.Program;immediate=asm.immediate
SHORT_VALUES=(0,1,127,128,255,32768,65534,65535)
def groups():
    for top in (False,True):
        for amount in range(32):
            for flags in (0,31):yield 0,top,amount,flags
    for kind in (1,2):
        for position in range(4):
            for flags in (0,31):yield kind,False,position,flags
def instruction(kind,top,amount):
    return (0xe6804018|(int(top)<<6)|(amount<<7)) if kind==0 else (0xe784a810 if kind==2 else 0xe784f810)
def program(core,short=False,wrong_result=False,wrong_source=False,wrong_flags=False,wrong_accumulator=False,wrong_pack=False):
    p=Program(0x20000000 if core==0 else 0x20040000)
    p.constant(12,0x1fe00000); p.constant(8,0x12345678); p.constant(9,0x89abcdef)
    count=144*(8 if short else 65536)
    if core==1:
        p.constant(6,0x2003f000); p.mark('wait'); p.memory(7,6); p.emit(0xe3570001); p.branch('wait',cond=1)
    if core==0: p.puts('header')
    p.puts('begin');p.address(11,'return');p.address(13,'records'); p.mark('group')
    for n,r in enumerate((2,3,6,7,10)): p.memory(r,13,n*4)
    p.memory(10,7,load=False)
    p.constant(1,0); p.mark('case')
    if short: p.address(6,'values'); p.emit(0xe7960101)
    else: p.emit(0xe1a00001)
    p.memory(6,13,8);p.emit(0xe3560000);p.branch('pack_source',cond=0)
    p.emit(0xe1a08420);p.emit(0xe20000ff)
    for position in range(3):
        p.emit(0xe3560000|(position+1));p.branch('byte_'+str(position),cond=0)
    p.branch('byte_3')
    for position in range(4):
        p.mark('byte_'+str(position));shift=position*8
        p.constant(6,0x78563412&~(255<<shift));p.emit(0xe1860000|(shift<<7))
        p.constant(6,0x000180ff&~(255<<shift));p.emit(0xe1868008|(shift<<7));p.branch('shaped')
    p.mark('pack_source');p.emit(0xe1a08000);p.emit(0xe1e06008);p.emit(0xe1888806);p.memory(0,13,20)
    p.mark('shaped');p.memory(10,13,24);p.memory(7,13,12);p.emit(0xe12cf003);p.emit(0xe12fff17)
    p.mark('return');p.emit(0xe10f5000)
    if wrong_result:p.emit(0xe2844001)
    if wrong_flags:p.emit(0xe2255000|immediate(0x08000000))
    p.puts('row')
    for r in (4,5,0,8,2,3,10,9): p.emit(0xe1a06000|r); p.branch('word',link=True)
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
    p.mark('arm_thunk'); p.emit(0xe6804018); p.emit(0xe12fff1b)
    p.mark('values')
    for n in SHORT_VALUES: p.emit(n)
    p.mark('records')
    for kind,top,amount,flags in groups():
        requested=instruction(kind,top,amount);actual=requested
        if wrong_source:actual=(actual&~15)|1
        if wrong_accumulator and kind==2:actual=(actual&~0xf000)|0xf000
        if wrong_pack and kind==0:actual^=64
        left=(0x89abcdef if flags else 0x12345678) if kind==0 else 0x78563412
        seed=0xffffffff if flags else 0x7fffffff
        for word in (requested,flags<<27|(15 if flags else 0)<<16|0xdf,0 if kind==0 else amount+1,p.labels['arm_thunk'],actual,left,seed):p.emit(word)
    p.mark('records_end')
    p.string('header','VELOCE 1 3ds pack_difference_registers\n')
    p.string('begin',f'BEGIN arm11_{core} cases={count} words=8\n')
    p.string('end',f'BLOCK complete arm11_{core}\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={count*2}\nEND observations_complete\n')
    while len(p.data)%4: p.data.append(0)
    if len(p.data)>=0x3e000: raise ValueError('guest code overlaps synchronization')
    p.resolve(); return bytes(p.data)
def build(short=False,wrong_result=False,wrong_source=False,wrong_flags=False,wrong_accumulator=False,wrong_pack=False):
    programs=[program(i,short,wrong_result,wrong_source,wrong_flags,wrong_accumulator,wrong_pack) for i in (0,1)]
    p=Program(0x08000000);p.constant(12,0x1fe00000);p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle');p.resolve();programs.append(bytes(p.data))
    rom=bytearray(64);rom[:8]=b'V3DSP001'
    for core,(code,address) in enumerate(zip(programs,(0x20000000,0x20040000,0x08000000))):
        for n,value in enumerate((len(rom),address,address,len(code))): rom[16+core*16+n*4:20+core*16+n*4]=value.to_bytes(4,'little')
        rom.extend(code)
    return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('pack_difference_registers.v3ds'))
    args=parser.parse_args();args.output.write_bytes(build())
