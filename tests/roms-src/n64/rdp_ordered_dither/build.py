#!/usr/bin/env python3
"""Original MIT one-cycle blender triangle RAM observations; no SDK or expected output."""
import argparse,importlib.util,itertools
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('dp_assembler',HERE.parent/'dp_registers'/'build.py')
asm=importlib.util.module_from_spec(spec);spec.loader.exec_module(asm)
SUITE='rdp_ordered_dither'
VARIANTS=list(itertools.product(range(9),range(2)))
INDEX={v:i for i,v in enumerate(VARIANTS)}
def cases(short=False):
    for raw in ((0,1,7,8,127,128,247,248,254,255,0x7f7f,0x8080,0xfeff,0xffff) if short else range(65536)):
        for v in VARIANTS:yield raw,v

def profile_mode(profile,source):
    rgb=(0,1,3)[profile//3];alpha=(0,1,3)[profile%3]
    inactive=(3<<54)|(15<<32)|(1<<15)|(2047<<41)
    flags=64|16384|8|512|1|(4096|8192 if source else 0)
    return (rgb<<38)|(alpha<<36)|(2<<26)|(1<<22)|flags|inactive

def commands(v):
    profile,source=v
    image=(0xff<<56)|(2<<51)|(3<<32)|0x700000
    mode=(0xef<<56)|profile_mode(profile,source)
    scissor=(0xed<<56)|(16<<12)|16
    primitive=0xfa00000000000000
    fog=0xf800000017c96b83;blend=0xf9000000e143297f
    combine=(0xfc<<56)|(15<<52)|(15<<28)|(31<<47)|(4<<15)|(7<<44)|(7<<12)|(7<<41)|(4<<9)
    combine|=(15<<37)|(15<<24)|(31<<32)|(3<<6)|(7<<21)|(7<<3)|(7<<18)|3
    tri=[0xcc80001000080000,(2<<48)|0xffff0000,0,65536,(100<<48)|(200<<32)|(50<<16)]+[0]*7
    return [image,mode,scissor,primitive,fog,blend,combine]+tri+[0xe900000000000000]

def build(short=False,wrong_capture=False):
    rows=list(cases(short));p=asm.Program();asm.header(p,len(rows),13)
    p.address(8,'hidden_helper');p.i(13,25,9,0x800);p.i(9,0,10,10)
    p.mark('helper_copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'helper_copy')
    p.i(15,0,27,0xa070);p.i(15,0,28,0xa430);p.address(14,'templates');p.puts('header')
    p.mark('sample');p.i(39,16,12,0);p.r(2,rt=12,rd=8,sa=16)
    p.r(0,rt=8,rd=9,sa=8);p.r(33,14,9,22)
    p.i(39,22,23,0);p.i(9,0,8,0x800);p.i(43,28,8,0)
    p.i(15,0,8,0x5a5a);p.i(13,8,8,0x5a5a);p.r(0x3c,rt=8,rd=9);p.r(37,8,9,8)
    p.i(15,0,9,0xa06f);p.i(13,9,9,0xfff8);p.i(9,0,10,10)
    p.mark('clear');p.i(63,9,8,0);p.i(9,9,9,8);p.i(9,10,10,-1);p.branch(5,10,0,'clear')
    p.i(9,22,8,8);p.i(13,27,9,256);p.r(2,rt=23,rd=10,sa=2)
    p.mark('copy');p.i(39,8,11,0);p.i(43,9,11,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(9,10,10,-1);p.branch(5,10,0,'copy')
    p.r(2,rt=12,rd=8,sa=8);p.i(12,8,8,255);p.i(12,12,9,255)
    p.r(0,rt=8,rd=10,sa=24);p.i(14,8,11,255);p.r(0,rt=11,rd=11,sa=16);p.r(37,10,11,10)
    p.r(0,rt=8,rd=11,sa=4);p.r(33,8,11,11);p.i(12,11,11,255);p.r(0,rt=11,rd=11,sa=8);p.r(37,10,11,10);p.r(37,10,9,10);p.i(43,27,10,284)
    p.i(14,8,8,127);p.i(41,27,8,350)
    p.i(39,22,8,4);p.i(15,0,18,0x70);p.i(13,18,18,256);p.r(33,18,23,19);p.i(9,0,20,0x15);p.branch(4,8,0,'source_done')
    p.i(13,27,8,256);p.i(9,0,9,0xff0);p.r(2,rt=23,rd=10,sa=2)
    p.mark('xbus_copy');p.r(37,25,9,11);p.i(39,8,13,0);p.i(43,11,13,0);p.i(9,8,8,4);p.i(9,9,9,4);p.i(12,9,9,4095);p.i(9,10,10,-1);p.branch(5,10,0,'xbus_copy')
    p.i(9,0,18,0xff0);p.r(33,18,23,19);p.i(9,0,20,0x16)
    p.mark('source_done');p.i(43,24,20,12);p.i(43,24,18,0);p.i(43,24,19,4)
    p.mark('wait');p.i(39,24,8,8);p.branch(5,8,19,'wait')
    p.r(37,27,0,18);p.i(13,25,8,0x800);p.r(9,rs=8,rd=31);p.word(0)
    p.puts('row');p.r(37,27,0,18);p.i(9,0,15,4)
    p.mark('capture');p.i(55,18,5,0)
    if wrong_capture:p.i(14,5,5,1)
    p.jump(3,'hex');p.i(9,18,18,8);p.i(9,15,15,-1);p.branch(5,15,0,'capture')
    for reg in (20,21,22,23):p.r(37,reg,0,5);p.jump(3,'hex')
    p.i(15,0,8,0xa06f);p.i(13,8,8,0xfff8);p.i(55,8,5,0);p.jump(3,'hex');p.i(55,27,5,32);p.jump(3,'hex')
    for addr,offset in ((24,8),(24,12),(28,8)):p.i(39,addr,5,offset);p.jump(3,'hex')
    p.i(9,0,8,10);p.i(40,30,8,0);p.i(9,16,16,4);p.i(9,17,17,-1);p.branch(5,17,0,'sample')
    p.puts('complete');p.i(43,30,0,4);p.mark('halt');p.branch(4,0,0,'halt');asm.output_routines(p)
    p.string('header',f'VELOCE 1 n64 {SUITE}\nBEGIN data cases={len(rows)} words=13\n');p.string('row','REG r=');p.string('complete',f'LOG complete cases={len(rows)}\nEND observations_complete\n')
    while p.pc&7:p.data.append(0)
    p.mark('hidden_helper');p.i(9,0,8,0x400);p.i(43,28,8,0)
    for offset,reg in ((0,20),(8,21),(16,22),(24,23)):p.i(55,18,reg,offset)
    p.i(9,0,8,0x200);p.i(43,28,8,0);p.r(8,rs=31);p.word(0)
    p.mark('templates')
    for v in VARIANTS:
        words=commands(v);record=(len(words)*8).to_bytes(4,'big')+v[1].to_bytes(4,'big')+b''.join(w.to_bytes(8,'big') for w in words)
        assert len(record)<=256;p.data.extend(record+b'\0'*(256-len(record)))
    p.mark('table')
    for raw,v in rows:p.word(raw|(INDEX[v]<<16))
    p.resolve();return bytes(p.data)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',nargs='?',type=Path,default=HERE/(SUITE+'.z64'));args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(build())
