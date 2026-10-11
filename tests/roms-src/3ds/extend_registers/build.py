#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original ARM11 ARMv6 extend/accumulate and Thumb register corpus."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('original_assembler',HERE.parent/'reversal_registers/build.py');asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
Program=asm.Program;immediate=asm.immediate
SHORT_VALUES=(0,1,127,128,255,32768,65534,65535)
def groups():
    for kind in (0,2,3,4,6,7):
        for accumulate in (False,True):
            for rotation in range(4):
                for flags in (0,31):yield False,kind,accumulate,rotation,flags
    for kind in (3,2,7,6):
        for flags in (0,31):yield True,kind,False,0,flags
def instruction(thumb,kind,accumulate,rotation):
    if thumb:return 0xb204|((0 if kind==3 else 1 if kind==2 else 2 if kind==7 else 3)<<6)
    return 0xe6804070|(kind<<20)|((8 if accumulate else 15)<<16)|(rotation<<10)
def program(core,short=False,wrong_result=False,wrong_source=False,wrong_flags=False):
    p=Program(0x20000000 if core==0 else 0x20010000)
    p.constant(12,0x1fe00000); p.constant(8,0x12345678); p.constant(9,0x89abcdef)
    count=104*(8 if short else 65536)
    if core==1:
        p.constant(6,0x20008000); p.mark('wait'); p.memory(7,6); p.emit(0xe3570001); p.branch('wait',cond=1)
    if core==0: p.puts('header')
    p.puts('begin'); p.address(11,'return'); p.address(13,'records'); p.mark('group')
    for n,r in enumerate((2,3,10,7,6)): p.memory(r,13,n*4)
    p.emit(0xe3c77001)
    p.emit(0xe3520000|immediate(65536)); p.emit(0x31c760b0); p.emit(0x25876000)
    p.constant(1,0); p.mark('case')
    if short: p.address(6,'values'); p.emit(0xe7960101)
    else: p.emit(0xe1a00001)
    p.memory(6,13,24);p.emit(0xe3560000);p.branch('dual_source',cond=1)
    p.emit(0xe1e07000);p.emit(0xe1800807);p.branch('shaped')
    p.mark('dual_source');p.emit(0xe1a07420);p.emit(0xe20000ff);p.emit(0xe20770ff);p.emit(0xe1800807)
    p.emit(0xe3800000|immediate(0xa5000000));p.emit(0xe3800000|immediate(0x5a00))
    p.mark('shaped');p.emit(0xe1a00a70);p.memory(8,13,20)
    p.memory(7,13,12); p.emit(0xe128f003); p.emit(0xe12fff17)
    p.mark('return'); p.emit(0xe10f5000)
    if wrong_result: p.emit(0xe2844001)
    if wrong_flags: p.emit(0xe2255000|immediate(0x08000000))
    p.puts('row')
    for r in (4,5,0,2,3,8,9): p.emit(0xe1a06000|r); p.branch('word',link=True)
    p.constant(6,10); p.memory(6,12,load=False,byte=True)
    p.emit(0xe2811001); p.constant(6,8 if short else 65536); p.emit(0xe1510006); p.branch('case',cond=1)
    p.emit(0xe28dd01c); p.address(6,'records_end'); p.emit(0xe15d0006); p.branch('group',cond=1)
    p.puts('end')
    if core==0: p.constant(6,0x20008000); p.constant(7,1); p.memory(7,6,load=False)
    else: p.puts('complete')
    p.constant(6,0); p.memory(6,12,4,load=False); p.mark('idle'); p.branch('idle')
    p.mark('puts'); p.memory(7,6,load=True,byte=True); p.emit(0xe3570000); p.emit(0x012fff1e)
    p.memory(7,12,load=False,byte=True); p.emit(0xe2866001); p.branch('puts')
    p.mark('word'); p.memory(6,12,8,load=False); p.emit(0xe12fff1e)
    p.mark('arm_thunk'); p.emit(0xe6af4070); p.emit(0xe12fff1b)
    p.mark('thumb_thunk'); p.emit(0x4758b204)
    p.mark('values')
    for n in SHORT_VALUES: p.emit(n)
    p.mark('records')
    for thumb,kind,accumulate,rotation,flags in groups():
        requested=instruction(thumb,kind,accumulate,rotation)
        actual=requested
        if wrong_source:actual=(actual&~(56 if thumb else 15))|(8 if thumb else 1)
        thunk=p.labels['thumb_thunk' if thumb else 'arm_thunk']
        seed=0x7fffffff if flags==0 else 0x80008000
        for word in (requested,flags<<27|0xdf,(32-rotation*8)%32,thunk|int(thumb),actual,seed,int(kind%4==0)):p.emit(word)
    p.mark('records_end')
    p.string('header','VELOCE 1 3ds extend_registers\n')
    p.string('begin',f'BEGIN arm11_{core} cases={count} words=7\n')
    p.string('end',f'BLOCK complete arm11_{core}\n')
    p.string('row','REG r='); p.string('complete',f'LOG complete cases={count*2}\nEND observations_complete\n')
    while len(p.data)%4: p.data.append(0)
    if len(p.data)>=0x8000: raise ValueError('guest code overlaps synchronization')
    p.resolve(); return bytes(p.data)
def build(short=False,wrong_result=False,wrong_source=False,wrong_flags=False):
    programs=[program(i,short,wrong_result,wrong_source,wrong_flags) for i in (0,1)]
    p=Program(0x08000000);p.constant(12,0x1fe00000);p.constant(6,0);p.memory(6,12,4,load=False);p.mark('idle');p.branch('idle');p.resolve();programs.append(bytes(p.data))
    rom=bytearray(64);rom[:8]=b'V3DSP001'
    for core,(code,address) in enumerate(zip(programs,(0x20000000,0x20010000,0x08000000))):
        for n,value in enumerate((len(rom),address,address,len(code))): rom[16+core*16+n*4:20+core*16+n*4]=value.to_bytes(4,'little')
        rom.extend(code)
    return bytes(rom)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=Path(__file__).with_name('extend_registers.v3ds'))
    args=parser.parse_args();args.output.write_bytes(build())
