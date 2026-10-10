#!/usr/bin/env python3
"""Original MIT MIPS/RDP color and depth clear corpus, without SDK bytes."""
import argparse,importlib.util
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_fill_pipeline'
def patterns(short=False):
    for raw in range(8 if short else 65536):
        if raw&0x50 or (raw&0x20 and not raw&4):continue
        for size in (1,2,3):
            for source in range(2):
                for profile in range(4):yield raw,size,source,profile

def cases(short=False):
    for row in patterns(short):
        for phase in range(2):yield row+(phase,)

def build(short=False,wrong_capture=False):
    rows=list(patterns(short));p=asm.Program();asm.header(p,len(rows),11);p.i(15,0,27,0xa070);p.i(15,0,28,0xa430)
    p.address(8,'commands');p.i(13,27,9,256);p.i(9,0,10,24)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.puts('header');p.mark('sample');p.i(39,16,12,0);p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8);p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,19)
    p.mark('clear');p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,'clear')
    p.r(2,rt=12,rd=21,sa=19);p.i(12,21,21,3);p.i(9,0,23,3);p.i(9,0,8,3);p.branch(5,21,8,'width_done');p.i(9,0,23,2)
    p.mark('width_done');p.i(15,0,8,0xff10);p.r(37,8,23,8);p.i(43,27,8,264)
    p.r(2,rt=12,rd=8,sa=16);p.i(12,8,8,3);p.i(15,0,9,0xff00);p.branch(5,8,0,'format_done');p.i(15,0,9,0xff40)
    p.mark('format_done');p.i(9,8,8,1);p.r(0,rt=8,rd=8,sa=19);p.r(37,8,9,8);p.r(37,8,23,8);p.i(43,27,8,312)
    p.i(12,12,8,65535);p.i(15,0,9,0x0f0a);p.r(37,8,9,9);p.i(43,27,9,284)
    p.r(0,rt=8,rd=9,sa=16);p.i(14,8,8,65535);p.r(37,8,9,8);p.i(43,27,8,324)
    p.i(15,0,8,0xf600);p.i(13,8,8,0xc00c);p.i(43,27,8,296);p.i(43,27,8,328);p.i(43,27,0,300);p.i(43,27,0,332)
    p.branch(4,21,0,'whole');p.i(9,0,8,1);p.branch(4,21,8,'clipped');p.i(9,0,8,2);p.branch(4,21,8,'fractional')
    p.i(15,0,8,0xed00);p.i(43,27,8,272);p.i(12,12,8,1);p.i(13,8,8,2);p.r(0,rt=8,rd=8,sa=24);p.i(15,0,9,1);p.i(13,9,9,16);p.r(37,8,9,8);p.i(43,27,8,276);p.branch(4,0,0,'geometry_done')
    p.mark('whole');p.i(15,0,8,0xed00);p.i(43,27,8,272);p.i(15,0,8,1);p.i(13,8,8,16);p.i(43,27,8,276);p.branch(4,0,0,'geometry_done')
    p.mark('clipped');p.i(15,0,8,0xed00);p.i(13,8,8,0x4004);p.i(43,27,8,272);p.i(15,0,8,0x0100);p.i(13,8,8,0xc00c);p.i(43,27,8,276);p.branch(4,0,0,'geometry_done')
    p.mark('fractional');p.i(15,0,8,0xed00);p.i(13,8,8,0x5002);p.i(43,27,8,272);p.i(15,0,8,0x0100);p.i(13,8,8,0xb00a);p.i(43,27,8,276)
    p.i(15,0,8,0xf600);p.i(13,8,8,0xa009);p.i(43,27,8,296);p.i(43,27,8,328);p.i(9,0,8,0x3001);p.i(43,27,8,300);p.i(43,27,8,332)
    p.mark('geometry_done');p.i(13,27,18,256);p.i(13,27,19,352);p.i(9,0,20,0x15)
    p.r(2,rt=12,rd=8,sa=18);p.i(12,8,8,1);p.branch(4,8,0,'source_done');p.i(13,27,8,256);p.i(9,0,9,0xff0);p.i(9,0,10,24)
    p.mark('dmem_copy');p.r(37,25,9,11);p.i(39,8,13,0);p.i(43,11,13,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(12,9,9,4095);p.i(9,10,10,-1);p.branch(5,10,0,'dmem_copy')
    p.i(9,0,18,0xff0);p.i(9,0,19,0x1050);p.i(9,0,20,0x16)
    p.mark('source_done');p.i(43,24,20,12);p.i(43,24,18,0);p.i(43,24,19,4)
    p.mark('wait');p.i(39,24,8,8);p.i(15,0,10,255);p.i(13,10,10,65535);p.r(36,19,10,9);p.branch(5,8,9,'wait')
    p.puts('row');p.r(37,27,0,18);p.i(9,0,20,8)
    p.mark('color');p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    p.jump(3,'hex');p.i(9,18,18,8);p.i(9,20,20,-1);p.branch(5,20,0,'color')
    p.i(15,0,8,0xa06f);p.i(13,8,8,0xfff8);p.i(55,8,5,0);p.jump(3,'hex')
    for offset in (64,136):p.i(55,27,5,offset);p.jump(3,'hex')
    p.i(9,0,8,10);p.i(40,30,8,0);p.puts('row');p.i(9,27,18,72);p.i(9,0,20,8)
    p.mark('depth');p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    p.jump(3,'hex');p.i(9,18,18,8);p.i(9,20,20,-1);p.branch(5,20,0,'depth')
    for addr,offset in ((24,8),(24,12),(28,8)):p.i(39,addr,5,offset);p.jump(3,'hex')
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,4);p.i(9,17,17,-1);p.branch(5,17,0,'sample')
    p.puts('complete');p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN data cases={2*len(rows)} words=11\n');p.string('row','REG r=');p.string('complete',f'LOG complete cases={2*len(rows)}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('commands')
    for word in (0xfe00000000700048,0xff10000300700048,0xed00000000010010,0xefb82c3f0f0a4000,0xf7000000fffcfffc,0xf600c00c00000000,0xe700000000000000,0xff10000300700000,0xf700000000000000,0xf600c00c00000000,0xe700000000000000,0xe900000000000000):p.data.extend(word.to_bytes(8,'big'))
    p.mark('table')
    for raw,size,source,profile in rows:p.word(raw+((size-1)<<16)+(source<<18)+(profile<<19))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
