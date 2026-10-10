#!/usr/bin/env python3
"""Original MIT DP command-state progression ROM; no SDK or commercial bytes."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_tile_commands'
def cases(short=False):
    for raw in range(8 if short else 65536):
        for source in range(2):yield raw,source

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=asm.Program();asm.header(p,len(rows),7)
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430)
    p.puts('header');p.mark('sample');p.i(39,16,12,0)
    p.i(9,0,8,0x800);p.i(43,28,8,0) # Acknowledge only the prior real FullSync.
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8);p.i(63,27,8,64)
    p.i(13,27,18,256);p.i(15,0,19,0x70);p.i(13,19,19,280);p.i(9,0,20,0x15)
    p.r(2,rt=12,rd=8,sa=16);p.branch(4,8,0,'source_done')
    p.i(13,25,18,0xff0);p.i(9,0,19,0x1008);p.i(9,0,20,0x16)
    p.mark('source_done');p.i(13,18,9,0);p.i(9,16,8,4);p.i(9,0,10,6)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1)
    # Wrap DMEM at 4096 while retaining the aligned DP stream address.
    p.r(2,rt=12,rd=13,sa=16);p.branch(4,13,0,'copy_no_wrap');p.i(12,9,9,4095);p.r(37,25,9,9)
    p.mark('copy_no_wrap');p.branch(5,10,0,'copy')
    # FullSync lies just beyond the first END; it must not run before extension.
    p.i(15,0,8,0x2900);p.i(43,9,8,0);p.i(43,9,0,4)
    p.i(43,24,20,12);p.i(12,18,18,65535);p.r(2,rt=12,rd=8,sa=16);p.branch(5,8,0,'start_done');p.i(15,0,18,0x70);p.i(13,18,18,256)
    p.mark('start_done');p.i(43,24,18,0);p.i(43,24,19,4)
    p.mark('wait_first');p.i(39,24,8,8);p.branch(5,8,19,'wait_first');p.puts('row')
    for addr,offset in ((24,8),(24,12),(28,8)):
        p.i(39,addr,5,offset)
        if wrong_capture and (addr,offset)==(24,8):p.i(14,5,5,1)
        p.jump(3,'hex')
    p.i(9,19,19,8);p.i(43,24,19,4)
    p.mark('wait_sync');p.i(39,24,8,8);p.branch(5,8,19,'wait_sync')
    for addr,offset in ((24,8),(24,12),(28,8)):p.i(39,addr,5,offset);p.jump(3,'hex')
    p.i(55,27,5,64);p.jump(3,'hex');p.i(9,0,8,10);p.i(40,30,8,0)
    p.i(9,16,16,28);p.i(9,17,17,-1);p.branch(5,17,0,'sample')
    p.puts('complete');p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN data cases={len(rows)} words=7\n');p.string('row','REG r=');p.string('complete',f'LOG complete cases={len(rows)}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('table')
    for raw,source in rows:
        p.word(raw+(source<<16));payload=(raw*0x1000100010001)&0x00ffffffffffffff
        for value in (0xfd00000000000000|payload,0xf500000000000000|(payload&~(7<<24))|((raw%8)<<24),0xf200000000000000|(payload&~(7<<24))|(((raw+3)%8)<<24)):p.data.extend(value.to_bytes(8,'big'))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
